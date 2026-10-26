#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Raspberry Pi Pico W + ST7735 128x160
// SPI0: SCK GP18, MOSI GP19, CS GP17, DC/A0 GP16, RST GP20
#define TFT_W 128
#define TFT_H 160
#define PIN_SCK 18
#define PIN_MOSI 19
#define PIN_CS 17
#define PIN_DC 16
#define PIN_RST 20
#define CELL 8
#define PLAY_TOP 24
#define GRID_W (TFT_W / CELL)
#define GRID_H ((TFT_H - 24) / CELL)
#define STEP_MS 220

#define BLACK      0x0000
#define WHITE      0xFFFF
#define GREEN      0x07E0
#define DARK_GREEN 0x03E0
#define RED        0xF800
#define YELLOW     0xFFE0
#define GRAY       0x8410
#define DARK       0x2104

static int snake_x[GRID_W * GRID_H];
static int snake_y[GRID_W * GRID_H];
static int snake_len = 3;
static int dir_x = 1, dir_y = 0;
static int next_x = 1, next_y = 0;
static int food_x = 5, food_y = 5;
static int score = 0;
static bool started = false, paused = false, game_over = false;
static absolute_time_t last_step;
static char serial_line[32];
static size_t serial_len = 0;

// 5x7 font; each byte contains 5 useful bits, MSB first.
// Return the 7 rows for supported characters.
static const uint8_t *glyph(char c) {
    static const uint8_t blank[7] = {0,0,0,0,0,0,0};
    // Values correspond to a compact 5x7 bitmap, each row uses low 5 bits.
    switch (c) {
        case 'A': { static const uint8_t g[]={14,17,17,31,17,17,17}; return g; }
        case 'B': { static const uint8_t g[]={30,17,17,30,17,17,30}; return g; }
        case 'C': { static const uint8_t g[]={14,17,16,16,16,17,14}; return g; }
        case 'D': { static const uint8_t g[]={30,17,17,17,17,17,30}; return g; }
        case 'E': { static const uint8_t g[]={31,16,16,30,16,16,31}; return g; }
        case 'F': { static const uint8_t g[]={31,16,16,30,16,16,16}; return g; }
        case 'G': { static const uint8_t g[]={14,17,16,23,17,17,15}; return g; }
        case 'H': { static const uint8_t g[]={17,17,17,31,17,17,17}; return g; }
        case 'I': { static const uint8_t g[]={31,4,4,4,4,4,31}; return g; }
        case 'J': { static const uint8_t g[]={7,2,2,2,18,18,12}; return g; }
        case 'K': { static const uint8_t g[]={17,18,20,24,20,18,17}; return g; }
        case 'L': { static const uint8_t g[]={16,16,16,16,16,16,31}; return g; }
        case 'M': { static const uint8_t g[]={17,27,21,21,17,17,17}; return g; }
        case 'N': { static const uint8_t g[]={17,25,21,19,17,17,17}; return g; }
        case 'O': { static const uint8_t g[]={14,17,17,17,17,17,14}; return g; }
        case 'P': { static const uint8_t g[]={30,17,17,30,16,16,16}; return g; }
        case 'Q': { static const uint8_t g[]={14,17,17,17,21,18,13}; return g; }
        case 'R': { static const uint8_t g[]={30,17,17,30,20,18,17}; return g; }
        case 'S': { static const uint8_t g[]={15,16,16,14,1,1,30}; return g; }
        case 'T': { static const uint8_t g[]={31,4,4,4,4,4,4}; return g; }
        case 'U': { static const uint8_t g[]={17,17,17,17,17,17,14}; return g; }
        case 'V': { static const uint8_t g[]={17,17,17,17,17,10,4}; return g; }
        case 'W': { static const uint8_t g[]={17,17,17,21,21,21,10}; return g; }
        case 'X': { static const uint8_t g[]={17,17,10,4,10,17,17}; return g; }
        case 'Y': { static const uint8_t g[]={17,17,10,4,4,4,4}; return g; }
        case 'Z': { static const uint8_t g[]={31,1,2,4,8,16,31}; return g; }
        case '0': { static const uint8_t g[]={14,17,19,21,25,17,14}; return g; }
        case '1': { static const uint8_t g[]={4,12,4,4,4,4,14}; return g; }
        case '2': { static const uint8_t g[]={14,17,1,2,4,8,31}; return g; }
        case '3': { static const uint8_t g[]={30,1,1,14,1,1,30}; return g; }
        case '4': { static const uint8_t g[]={2,6,10,18,31,2,2}; return g; }
        case '5': { static const uint8_t g[]={31,16,16,30,1,1,30}; return g; }
        case '6': { static const uint8_t g[]={14,16,16,30,17,17,14}; return g; }
        case '7': { static const uint8_t g[]={31,1,2,4,8,8,8}; return g; }
        case '8': { static const uint8_t g[]={14,17,17,14,17,17,14}; return g; }
        case '9': { static const uint8_t g[]={14,17,17,15,1,1,14}; return g; }
        case ':': { static const uint8_t g[]={0,4,4,0,4,4,0}; return g; }
        case '-': { static const uint8_t g[]={0,0,0,31,0,0,0}; return g; }
        case '!': { static const uint8_t g[]={4,4,4,4,4,0,4}; return g; }
        default: return blank;
    }
}

static inline void cs(bool high) { gpio_put(PIN_CS, high); }
static void write_cmd(uint8_t cmd) {
    cs(false); gpio_put(PIN_DC, 0); spi_write_blocking(spi0, &cmd, 1); cs(true);
}
static void write_data(const uint8_t *data, size_t len) {
    cs(false); gpio_put(PIN_DC, 1); spi_write_blocking(spi0, data, len); cs(true);
}
static void command_data(uint8_t cmd, const uint8_t *data, size_t len) {
    cs(false); gpio_put(PIN_DC, 0); spi_write_blocking(spi0, &cmd, 1);
    if (data && len) { gpio_put(PIN_DC, 1); spi_write_blocking(spi0, data, len); }
    cs(true);
}
static void init_display(void) {
    gpio_init(PIN_CS); gpio_set_dir(PIN_CS, GPIO_OUT); gpio_put(PIN_CS, 1);
    gpio_init(PIN_DC); gpio_set_dir(PIN_DC, GPIO_OUT); gpio_put(PIN_DC, 0);
    gpio_init(PIN_RST); gpio_set_dir(PIN_RST, GPIO_OUT); gpio_put(PIN_RST, 1);
    spi_init(spi0, 2 * 1000 * 1000);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_put(PIN_RST, 1); sleep_ms(50); gpio_put(PIN_RST, 0); sleep_ms(50);
    gpio_put(PIN_RST, 1); sleep_ms(150);
    write_cmd(0x01); sleep_ms(150); // SWRESET
    write_cmd(0x11); sleep_ms(120); // SLPOUT
    const uint8_t b1[]={0x01,0x2C,0x2D}; command_data(0xB1,b1,sizeof b1);
    const uint8_t b2[]={0x01,0x2C,0x2D}; command_data(0xB2,b2,sizeof b2);
    const uint8_t b3[]={0x01,0x2C,0x2D,0x01,0x2C,0x2D}; command_data(0xB3,b3,sizeof b3);
    const uint8_t b4[]={0x07}; command_data(0xB4,b4,sizeof b4);
    const uint8_t c0[]={0xA2,0x02,0x84}; command_data(0xC0,c0,sizeof c0);
    const uint8_t c1[]={0xC5}; command_data(0xC1,c1,sizeof c1);
    const uint8_t c2[]={0x0A,0x00}; command_data(0xC2,c2,sizeof c2);
    const uint8_t c3[]={0x8A,0x2A}; command_data(0xC3,c3,sizeof c3);
    const uint8_t c4[]={0x8A,0xEE}; command_data(0xC4,c4,sizeof c4);
    const uint8_t c5[]={0x0E}; command_data(0xC5,c5,sizeof c5);
    write_cmd(0x20); // INVOFF
    const uint8_t colmod[]={0x05}; command_data(0x3A,colmod,sizeof colmod);
    const uint8_t madctl[]={0xC8}; command_data(0x36,madctl,sizeof madctl);
    const uint8_t e0[]={0x02,0x1C,0x07,0x12,0x37,0x32,0x29,0x2D,0x29,0x25,0x2B,0x39,0x00,0x01,0x03,0x10}; command_data(0xE0,e0,sizeof e0);
    const uint8_t e1[]={0x03,0x1D,0x07,0x06,0x2E,0x2C,0x29,0x2D,0x2E,0x2E,0x37,0x3F,0x00,0x00,0x02,0x10}; command_data(0xE1,e1,sizeof e1);
    write_cmd(0x13); sleep_ms(10); write_cmd(0x29); sleep_ms(100);
}
static void set_window(int x0,int y0,int x1,int y1) {
    uint8_t d[4];
    d[0]=(uint8_t)(x0>>8); d[1]=(uint8_t)x0; d[2]=(uint8_t)(x1>>8); d[3]=(uint8_t)x1; command_data(0x2A,d,4);
    d[0]=(uint8_t)(y0>>8); d[1]=(uint8_t)y0; d[2]=(uint8_t)(y1>>8); d[3]=(uint8_t)y1; command_data(0x2B,d,4);
    write_cmd(0x2C);
}
static void fill_rect(int x,int y,int w,int h,uint16_t color) {
    if(w<=0||h<=0) return;
    int x0=x<0?0:x, y0=y<0?0:y, x1=x+w-1, y1=y+h-1;
    if(x1>=TFT_W) x1=TFT_W-1; if(y1>=TFT_H) y1=TFT_H-1;
    if(x0>x1||y0>y1) return;
    set_window(x0,y0,x1,y1);
    uint8_t buf[512]; uint8_t hi=(uint8_t)(color>>8), lo=(uint8_t)color;
    for(size_t i=0;i<sizeof(buf);i+=2){buf[i]=hi;buf[i+1]=lo;}
    size_t remaining=(size_t)(x1-x0+1)*(size_t)(y1-y0+1);
    cs(false); gpio_put(PIN_DC,1);
    while(remaining>=256){spi_write_blocking(spi0,buf,sizeof buf);remaining-=256;}
    if(remaining){uint8_t tail[512]; for(size_t i=0;i<remaining;i++){tail[2*i]=hi;tail[2*i+1]=lo;} spi_write_blocking(spi0,tail,remaining*2);}
    cs(true);
}
static void draw_char(int x,int y,char c,uint16_t color,int scale) {
    const uint8_t *rows=glyph(c);
    for(int r=0;r<7;r++) for(int col=0;col<5;col++)
        if(rows[r] & (1u << (4-col))) fill_rect(x+col*scale,y+r*scale,scale,scale,color);
}
static void draw_text(int x,int y,const char *s,uint16_t color,int scale) {
    while(*s){char c=*s++; if(c>='a'&&c<='z') c=(char)(c-'a'+'A'); draw_char(x,y,c,color,scale); x+=6*scale;}
}
static void cell_rect(int gx,int gy,uint16_t color) { fill_rect(gx*CELL,PLAY_TOP+gy*CELL,CELL-1,CELL-1,color); }
static bool occupied(int x,int y) { for(int i=0;i<snake_len;i++) if(snake_x[i]==x&&snake_y[i]==y) return true; return false; }
static void spawn_food(void) {
    int free_count=0;
    for(int y=0;y<GRID_H;y++) for(int x=0;x<GRID_W;x++) if(!occupied(x,y)) free_count++;
    if(!free_count){food_x=-1;food_y=-1;return;}
    int target=(int)(rand()%(unsigned)free_count);
    for(int y=0;y<GRID_H;y++) for(int x=0;x<GRID_W;x++) if(!occupied(x,y)) { if(target--==0){food_x=x;food_y=y;return;} }
}
static void draw_header(void) {
    char score_text[20]; snprintf(score_text,sizeof score_text,"SCORE:%d",score);
    fill_rect(0,0,TFT_W,PLAY_TOP,DARK); draw_text(4,4,"SNAKE",GREEN,1); draw_text(72,4,score_text,WHITE,1);
}
static void draw_game(void) {
    fill_rect(0,0,TFT_W,TFT_H,BLACK); draw_header();
    fill_rect(0,PLAY_TOP,TFT_W,1,GRAY); fill_rect(0,PLAY_TOP+GRID_H*CELL-1,TFT_W,1,GRAY);
    for(int i=0;i<snake_len;i++) cell_rect(snake_x[i],snake_y[i],i==0?DARK_GREEN:GREEN);
    if(food_x>=0) cell_rect(food_x,food_y,RED);
    fill_rect(0,PLAY_TOP+GRID_H*CELL,TFT_W,TFT_H-(PLAY_TOP+GRID_H*CELL),DARK);
    draw_text(3,TFT_H-15,"WASD MOVE P PAUSE",WHITE,1);
}
static void draw_message(const char *line1,const char *line2) {
    fill_rect(8,52,TFT_W-16,48,BLACK); draw_text(16,60,line1,YELLOW,1); if(line2) draw_text(16,76,line2,WHITE,1);
}
static void show_start(void) {
    fill_rect(0,0,TFT_W,TFT_H,BLACK); draw_text(22,30,"SNAKE GAME",GREEN,1);
    draw_text(10,58,"TYPE START",WHITE,1); draw_text(10,74,"WASD + ENTER",WHITE,1);
    draw_text(10,98,"P PAUSE",YELLOW,1); draw_text(10,114,"R RESTART",YELLOW,1);
}
static void reset_game(void) {
    int cx=GRID_W/2, cy=GRID_H/2; snake_len=3;
    snake_x[0]=cx; snake_y[0]=cy; snake_x[1]=cx-1; snake_y[1]=cy; snake_x[2]=cx-2; snake_y[2]=cy;
    dir_x=next_x=1; dir_y=next_y=0; score=0; game_over=false; paused=false; started=true;
    spawn_food(); last_step=get_absolute_time(); draw_game();
}
static void end_game(const char *msg) {
    game_over=true; char s[20]; snprintf(s,sizeof s,"SCORE:%d",score); draw_message(msg,s);
    printf("Fin del juego. Puntuacion: %d. Escribe R y Enter para reiniciar.\n",score);
}
static void step_game(void) {
    dir_x=next_x; dir_y=next_y;
    int nx=snake_x[0]+dir_x, ny=snake_y[0]+dir_y;
    if(nx<0||nx>=GRID_W||ny<0||ny>=GRID_H){end_game("GAME OVER");return;}
    bool eating=(nx==food_x&&ny==food_y);
    int check_len=eating?snake_len:snake_len-1;
    for(int i=0;i<check_len;i++) if(snake_x[i]==nx&&snake_y[i]==ny){end_game("GAME OVER");return;}
    int old_tail_x=snake_x[snake_len-1], old_tail_y=snake_y[snake_len-1];
    if(eating && snake_len < GRID_W*GRID_H){snake_len++; score++;}
    for(int i=snake_len-1;i>0;i--){snake_x[i]=snake_x[i-1];snake_y[i]=snake_y[i-1];}
    snake_x[0]=nx; snake_y[0]=ny;
    if(eating){spawn_food(); if(food_x<0){game_over=true;char s[20];snprintf(s,sizeof s,"SCORE:%d",score);draw_message("YOU WIN!",s);return;} draw_game();}
    else {cell_rect(old_tail_x,old_tail_y,BLACK);cell_rect(snake_x[0],snake_y[0],DARK_GREEN);draw_header();}
    last_step=get_absolute_time();
}
static void set_direction(char c) {
    int x=next_x,y=next_y;
    if(c=='W'){x=0;y=-1;} else if(c=='S'){x=0;y=1;} else if(c=='A'){x=-1;y=0;} else if(c=='D'){x=1;y=0;} else return;
    if(x==-dir_x&&y==-dir_y)return; next_x=x;next_y=y;
}
static void handle_command(char *line) {
    for(char *p=line;*p;p++){if(*p>='a'&&*p<='z')*p=(char)(*p-'a'+'A');}
    if(!line[0])return;
    if(strcmp(line,"START")==0||strcmp(line,"GO")==0){reset_game();puts("Snake iniciado. W/A/S/D + Enter; P pausa; R reinicia; Q sale.");}
    else if(strlen(line)==1 && strchr("WASD",line[0])){if(!started)puts("Escribe START y Enter para comenzar.");else if(game_over)puts("Fin del juego: escribe R para reiniciar.");else set_direction(line[0]);}
    else if(strcmp(line,"P")==0){if(started&&!game_over){paused=!paused;if(paused){draw_message("PAUSED","PRESS P TO PLAY");puts("Pausa.");}else{draw_game();puts("Reanudado.");}}}
    else if(strcmp(line,"R")==0){reset_game();puts("Juego reiniciado.");}
    else if(strcmp(line,"Q")==0){started=false;paused=false;show_start();puts("Juego detenido. Escribe START para jugar.");}
    else if(strcmp(line,"HELP")==0||strcmp(line,"H")==0||strcmp(line,"?")==0) puts("Comandos: START, W, A, S, D, P, R, Q, HELP.");
    else printf("Comando no reconocido: %s. Escribe HELP.\n",line);
}
static void poll_serial(void) {
    int ch;
    while((ch=getchar_timeout_us(0))!=PICO_ERROR_TIMEOUT){
        if(ch=='\r'||ch=='\n'){if(serial_len){serial_line[serial_len]='\0';handle_command(serial_line);serial_len=0;}}
        else if(ch==8||ch==127){if(serial_len)serial_len--;}
        else if(ch>=32&&ch<=126&&serial_len<sizeof(serial_line)-1){serial_line[serial_len++]=(char)ch;}
    }
}
int main(void) {
    stdio_init_all(); sleep_ms(1500); srand((unsigned)to_us_since_boot(get_absolute_time()));
    puts("Inicializando ST7735..."); init_display(); show_start();
    puts("Snake listo. Escribe START y Enter. Controles W/A/S/D + Enter. P pausa, R reinicia, Q vuelve al inicio, HELP ayuda.");
    while(true){poll_serial(); if(started&&!paused&&!game_over && absolute_time_diff_us(last_step,get_absolute_time()) >= STEP_MS*1000) step_game(); sleep_ms(2);}
}
