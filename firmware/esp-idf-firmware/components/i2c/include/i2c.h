/*
 * i2c_oled.h
 *
 * Modul konfiguracji magistrali I2C oraz niskopoziomowej obslugi ekranu OLED.
 * Definiuje piny, parametry transmisji oraz funkcje zapisu/odczytu rejestrow.
 */

#ifndef I2C_OLED_H
#define I2C_OLED_H

#include <stdio.h>
#include <driver/i2c.h>
#include <esp_err.h>
#include "esp_log.h"
#include "oled.h"

#define I2C_MASTER_SCL_IO           3      // Numer GPIO dla linii zegarowej SCL
#define I2C_MASTER_SDA_IO           2      // Numer GPIO dla linii danych SDA
#define I2C_MASTER_NUM              0      // Numer portu I2C urzadzenia master
#define I2C_MASTER_FREQ_HZ          1000000 // Czestotliwosc taktowania magistrali I2C
#define I2C_MASTER_TX_BUF_DISABLE   0      // Brak bufora TX dla trybu master
#define I2C_MASTER_RX_BUF_DISABLE   0      // Brak bufora RX dla trybu master
#define I2C_MASTER_TIMEOUT_MS       1000

#define I2C_TICKS_TO_WAIT           100    // Maksymalny czas oczekiwania na magistrale

#define ACK_EN                      1
#define ACK_DIS                     0

#define TAG                         "SSD1306"
#define CONFIG_OFFSETX              2

extern esp_err_t err;

// Inicjalizacja magistrali I2C
esp_err_t i2c_master_init(void);

// Odpytanie rejestru
esp_err_t i2c_write_reg(uint8_t ADDR, uint8_t REG);

// Zapis jednego bajtu do rejestru
esp_err_t i2c_write_val(uint8_t ADDR, uint8_t REG, uint8_t VAL);

// Odczyt danych do bufora
esp_err_t i2c_read(uint8_t ADDR, uint8_t *buf, uint8_t bytesToReceive);

// Zapis dwoch bajtow do rejestru
esp_err_t i2c_write_2byte(uint8_t ADDR, uint8_t REG, uint16_t VAL);

// Zapis i odczyt po kolei w jednej sekwencji
esp_err_t i2c_write_read(uint8_t ADDR, uint8_t REG, uint8_t *buf, uint8_t bytesToReceive);

// Superszybki odczyt na potrzeby profilowania energetycznego
esp_err_t i2c_write_read_fast(uint8_t ADDR, uint8_t REG, uint8_t *buf, uint8_t len);

// Funkcje obslugi wyswietlacza OLED
void i2c_init(SSD1306_t * dev, int width, int height);
void i2c_display_image(SSD1306_t * dev, int page, int seg, const uint8_t * images, int width);
void i2c_contrast(SSD1306_t * dev, int contrast);

#endif // I2C_OLED_H