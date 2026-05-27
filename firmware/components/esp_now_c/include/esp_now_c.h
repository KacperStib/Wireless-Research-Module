/*
 * esp_now_c.h
 *
 * Modul obslugi komunikacji bezprzewodowej ESP-NOW.
 * Zawiera funkcje inicjalizacji, wysylania oraz callbacki odbioru danych.
 */

#ifndef ESP_NOW_C_H
#define ESP_NOW_C_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "sd_card.h"
#include "gps.h"

// Zmienna przechowujaca RSSI
extern int rssi;

// Zmienna czasu nadawania
extern uint32_t tx_time;

// Callback wywolywany gdy przyjdzie pakiet ESP-NOW
typedef void (*espnow_rx_cb_t)(const uint8_t *data, int len);

// Funkcje dostepowe modulu ESP-NOW
esp_err_t espnow_init(espnow_rx_cb_t rx_callback);
esp_err_t espnow_set_peer(const uint8_t mac[6]);
esp_err_t espnow_send(const uint8_t *data, size_t len);
void      espnow_deinit(void);

// Glowna funkcja obslugi odebranych danych
void espnow_rx_handler(const uint8_t *data, int len);

#endif // ESP_NOW_C_H