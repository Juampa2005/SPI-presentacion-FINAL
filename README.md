# Comunicación SPI con Raspberry Pi Pico W

## Descripción del proyecto

Este proyecto tiene como objetivo estudiar e implementar el periférico **SPI (Serial Peripheral Interface)** en una Raspberry Pi Pico W basada en el microcontrolador RP2040.

Se estudia el funcionamiento de SPI, su arquitectura, la configuración de sus registros y las alternativas de programación mediante acceso directo al hardware y mediante el Pico SDK.

Como aplicación práctica, se utiliza una pantalla TFT ST7735 de 128 × 160 píxeles para demostrar la comunicación entre el microcontrolador y un dispositivo externo.

El proyecto también contempla la integración de un REPL (*Read-Eval-Print Loop*) que permite ejecutar comandos desde una consola para controlar las acciones mostradas en la pantalla.

## Objetivos

- Comprender el funcionamiento del protocolo SPI y sus señales.
- Analizar la arquitectura del periférico y su relación con la CPU, el bus interno y los GPIO.
- Identificar los principales registros de configuración, datos y estado.
- Implementar operaciones de acceso directo a registros mediante punteros en C.
- Configurar y utilizar SPI mediante las funciones del Pico SDK.
- Comparar las características relevantes de los microcontroladores RP2040 y RP2350.
- Establecer comunicación con una pantalla TFT mediante SPI.
- Diseñar una interfaz de comandos que permita controlar el dispositivo externo.

## Tecnologías y componentes

### Hardware

- Raspberry Pi Pico W (RP2040).
- Pantalla TFT ST7735 de 128 × 160 píxeles.
- Protoboard y cables de conexión.
- Cable USB para alimentación, programación y comunicación con la consola.

### Software

- Lenguaje de programación C.
- Raspberry Pi Pico SDK.
- CMake para la configuración del proyecto.
- Herramientas de compilación compatibles con el Pico SDK.
- Terminal serial para interactuar con el REPL.

## ¿Qué es SPI?

SPI es un protocolo de comunicación serial síncrono que permite intercambiar datos entre un microcontrolador y dispositivos periféricos.

Utiliza principalmente cuatro señales:

| Señal | Función |
|---|---|
| SCK | Reloj que sincroniza la comunicación. |
| MOSI | Transmite datos desde el maestro hacia el periférico. |
| MISO | Transmite datos desde el periférico hacia el maestro. |
| CS/CSn | Selecciona el dispositivo con el que se desea comunicar. |

SPI permite la transmisión y recepción simultánea de datos. La frecuencia, el modo de reloj y el formato de transferencia deben configurarse de acuerdo con las características del dispositivo conectado.

## Arquitectura del sistema

El funcionamiento general del sistema se organiza de la siguiente manera:

1. **CPU:** ejecuta el programa y configura las operaciones.
2. **Bus interno:** permite acceder a los registros del periférico.
3. **Controlador SPI:** gestiona las transferencias y genera el reloj.
4. **Registros y FIFO:** permiten configurar el hardware y almacenar temporalmente los datos transmitidos y recibidos.
5. **GPIO:** conectan las señales del periférico con los pines físicos.
6. **Pantalla TFT:** recibe e interpreta los comandos y datos enviados por el microcontrolador.

## Métodos de implementación

### 1. Acceso directo a registros

Este método permite interactuar con el hardware mediante direcciones de memoria, punteros y operaciones sobre bits.

Se estudian conceptos como:

- Dirección base y offsets.
- Punteros y desreferenciación.
- Uso de `volatile`.
- Máscaras y operaciones bit a bit.
- Registros de configuración, datos y estado.

Las direcciones y los campos utilizados deben corresponder al mapa de memoria del microcontrolador seleccionado.

### 2. Implementación mediante Pico SDK

El Pico SDK proporciona funciones que simplifican la configuración y el uso de SPI.

Entre las funciones relevantes se encuentran:

- `spi_init()`: inicializa el controlador SPI con una frecuencia solicitada.
- `gpio_set_function()`: configura la función alternativa de un GPIO.
- `spi_write_blocking()`: transmite datos y espera a completar la operación solicitada.
- `spi_read_blocking()`: recibe datos y espera a completar la lectura solicitada.

La configuración también debe establecer el formato de transferencia, los pines utilizados y la selección del dispositivo.

## Interfaz REPL

El proyecto contempla una interfaz de comandos para controlar la pantalla desde una terminal.

El flujo de funcionamiento previsto es:

**Comando → interpretación en C → controlador SPI → pantalla TFT → resultado visible.**

Ejemplos de comandos propuestos:

| Comando | Acción |
|---|---|
| `help` | Muestra los comandos disponibles. |
| `heart` | Dibuja un corazón en la