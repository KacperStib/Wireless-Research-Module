/*
 * dev_config.h
 *
 * Konfiguracja radia i struktur danych dla ESP32.
 * Zawiera definicje dla LoRa oraz ESP-NOW.
 */

#ifndef DEV_CONFIG_H
#define DEV_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include "lora.h"
#include "esp_now_c.h"
#include "esp_log.h"

// Typy technologii radiowych
typedef enum {
    RADIO_TECH_LORA   = 0, // LoRa
    RADIO_TECH_ESPNOW = 1  // ESP-NOW
} radio_tech_t;

// Kierunek pracy radia
typedef enum {
    RADIO_DIR_RX = 0, // Odbiornik
    RADIO_DIR_TX = 1  // Nadajnik
} radio_dir_t;

// Przeprowadzane testy
typedef enum {
	TEST_IDLE = 0,
	TEST_GENERAL = 1,
	TEST_POWER = 2,
	TEST_PER = 3
} test_scenario_t;

// Parametry fizyczne modulu LoRa
typedef struct {
    uint8_t sf;  // Spreading Factor 
    uint8_t bw;  // Bandwidth (szerokosc pasma)
    uint8_t cr;  // Coding Rate
    uint8_t pwr; // Moc nadawania w dBm
} lora_config_t;

// Glowna konfiguracja urzadzenia zapisywana w NVS
typedef struct {
    radio_tech_t tech;        // Wybrana technologia 
    radio_dir_t  dir;         // Tryb pracy 
    uint8_t      peer_mac[6]; // Adres MAC dla ESP-NOW
    lora_config_t lora;       // Ustawienia szczegolowe LoRa
    test_scenario_t test;
} radio_config_t;

// Globalna zmienna z konfiguracja radia
extern volatile radio_config_t radio_cfg;

// Funkcja aplikujaca ustawienia z radio_cfg do sprzetu
void radio_apply_config(void);

#endif // DEV_CONFIG_H