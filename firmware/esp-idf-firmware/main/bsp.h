#ifndef MAIN_BSP_H_
#define MAIN_BSP_H_

#define I2C_SDA 2
#define I2C_SCL 3

#define GPS_RX 11 
#define GPS_TX 10

#define SPI_MISO 21
#define SPI_MOSI 19
#define SPI_SCK 20
#define LORA_CS 22
#define SD_CS 18

#define LORA_RST 0
#define LORA_IO 1
#define LORA_PWR 23

#define UART_RX 4
#define UART_TX 5

#define LED_PIN 15

#define OLED_ADDR 0x3c 
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1   //   QT-PY / XIAO


#endif /* MAIN_BSP_H_ */
