/*
 * oled.h
 *
 * Modul definicji rejestrow, komend i struktur dla kontrolera wyswietlacza SSD1306.
 * Zawiera parametry konfiguracji hardware, typy przewijania oraz prototypy funkcji.
 */

#ifndef OLED_H
#define OLED_H

#include <string.h>
#include <esp_err.h>
#include <driver/i2c.h>
#include "dev_config.h"

#define OLED_CONTROL_BYTE_CMD_STREAM    0x00
#define OLED_CONTROL_BYTE_DATA_STREAM   0x40

// Komendy podstawowe
#define OLED_CMD_SET_CONTRAST           0x81
#define OLED_CMD_DISPLAY_RAM            0xA4
#define OLED_CMD_DISPLAY_NORMAL         0xA6
#define OLED_CMD_DISPLAY_OFF            0xAE
#define OLED_CMD_DISPLAY_ON             0xAF

// Tabela komend adresowania
#define OLED_CMD_SET_MEMORY_ADDR_MODE   0x20
#define OLED_CMD_SET_PAGE_ADDR_MODE     0x02

// Konfiguracja sprzetowa
#define OLED_CMD_SET_DISPLAY_START_LINE 0x40
#define OLED_CMD_SET_SEGMENT_REMAP_0    0xA0    
#define OLED_CMD_SET_SEGMENT_REMAP_1    0xA1    
#define OLED_CMD_SET_MUX_RATIO          0xA8    
#define OLED_CMD_SET_COM_SCAN_MODE      0xC8    
#define OLED_CMD_SET_DISPLAY_OFFSET     0xD3    
#define OLED_CMD_SET_COM_PIN_MAP        0xDA    

// Parametry taktowania i zasilania
#define OLED_CMD_SET_DISPLAY_CLK_DIV    0xD5    
#define OLED_CMD_SET_VCOMH_DESELCT      0xDB    

// Przetwornica zasilania Charge Pump
#define OLED_CMD_SET_CHARGE_PUMP        0x8D    

// Komendy przewijania
#define OLED_CMD_DEACTIVE_SCROLL        0x2E

#define I2C_ADDRESS                     0x3C

// Struktura pojedynczej strony pamieci ekranu
typedef struct {
    uint8_t _segs[128];
} PAGE_t;

// Glowna struktura urzadzenia SSD1306
typedef struct {
    int _address;
    int _width;
    int _height;
    int _pages;
    PAGE_t _page[8];
    bool _flip;
    i2c_port_t _i2c_num;
} SSD1306_t;

// Instancja wyswietlacza
extern SSD1306_t dev;

// Inicjalizacja struktury
void ssd1306_init(SSD1306_t * dev, int width, int height);

// Czyszczenie calego ekranu
void ssd1306_clear_screen(SSD1306_t * dev, bool invert);

// Zmiana jasnosci/kontrastu 
void ssd1306_contrast(SSD1306_t * dev, int contrast);

// Rysowanie mapy bitowej w okreslonym miejscu
void ssd1306_display_image(SSD1306_t * dev, int page, int seg, const uint8_t * images, int width);

// Renderowanie tekstu o podanej dlugosci na wybranej stronie
void ssd1306_display_text(SSD1306_t * dev, int page, const char * text, int text_len, bool invert);

// Aktualizacja danych na ekranie
void ssd1306_update(float current_mA, int gps_fix, int gps_sats, float gps_lat, float gps_lon, float per,
					radio_config_t radio, float current_mA_peak, int rssi, int snr, uint32_t tx_time, bool connected);

#endif // OLED_H