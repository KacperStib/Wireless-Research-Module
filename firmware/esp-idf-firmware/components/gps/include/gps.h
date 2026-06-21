/**
 * @file gps.h
 * @brief Odchudzony, zoptymalizowany podsystem parsera NMEA GPS dla ESP32.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_types.h"
#include "esp_event.h"
#include "esp_err.h"
#include "driver/uart.h"

// Makra konfiguracyjne parsera
#define CONFIG_NMEA_PARSER_RING_BUFFER_SIZE  1024
#define CONFIG_NMEA_PARSER_TASK_STACK_SIZE   2560
#define CONFIG_NMEA_PARSER_TASK_PRIORITY     2

// Wspierane kluczowe instrukcje NMEA
#define CONFIG_NMEA_STATEMENT_GGA            1
#define CONFIG_NMEA_STATEMENT_RMC            1
#define CONFIG_NMEA_STATEMENT_GLL            1

// Zmienne globalne zasilajace reszte aplikacji (np. OLED, Logi)
extern volatile uint8_t gps_sats;
extern volatile bool    gps_fix;
extern volatile float   gps_lat;
extern volatile float   gps_lon;

/**
 * @brief Deklaracja bazy zdarzen dla systemu ESP Event Loop
 */
ESP_EVENT_DECLARE_BASE(ESP_NMEA_EVENT);

/**
 * @brief Struktura przechowujaca wylacznie kluczowe dane GPS
 */
typedef struct {
    float latitude;       /*!< Szerokosc geograficzna (stopnie) */
    float longitude;      /*!< Dlugosc geograficzna (stopnie) */
    uint8_t sats_in_use;  /*!< Liczba satelitow w uzyciu */
    bool valid;           /*!< Poprawnosc danych pozycji (A/V) */
    uint8_t fix_status;   /*!< Status fix (0=brak, 1=GPS, 2=DGPS) */
} gps_t;

typedef enum {
    STATEMENT_UNKNOWN = 0,
    STATEMENT_GGA,
    STATEMENT_RMC,
    STATEMENT_GLL
} nmea_statement_t;

typedef struct {
    struct {
        uart_port_t uart_port;
        uint32_t rx_pin;
        uint32_t baud_rate;
        uart_word_length_t data_bits;
        uart_parity_t parity;
        uart_stop_bits_t stop_bits;
        uint32_t event_queue_size;
    } uart;
} nmea_parser_config_t;

typedef void *nmea_parser_handle_t;

#define NMEA_PARSER_CONFIG_DEFAULT(rx_gpio)       \
    {                                             \
        .uart = {                                 \
            .uart_port = UART_NUM_1,              \
            .rx_pin = rx_gpio,                     \
            .baud_rate = 9600,                    \
            .data_bits = UART_DATA_8_BITS,        \
            .parity = UART_PARITY_DISABLE,        \
            .stop_bits = UART_STOP_BITS_1,        \
            .event_queue_size = 16                \
        }                                         \
    }

typedef enum {
    GPS_UPDATE,
    GPS_UNKNOWN
} nmea_event_id_t;

nmea_parser_handle_t nmea_parser_init(const nmea_parser_config_t *config);
esp_err_t nmea_parser_deinit(nmea_parser_handle_t nmea_hdl);
esp_err_t nmea_parser_add_handler(nmea_parser_handle_t nmea_hdl, esp_event_handler_t event_handler, void *handler_args);
void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

#ifdef __cplusplus
}
#endif