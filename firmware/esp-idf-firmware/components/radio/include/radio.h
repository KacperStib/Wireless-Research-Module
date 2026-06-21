#ifndef RADIO_H
#define RADIO_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "dev_config.h"
#include "ina219.h"
#include "sd_card.h"

#define TAG_INIT "RADIO"

#define ASK_TAG 0xAA
#define ANS_TAG 0xBB

extern uint8_t buf8[250];
extern uint8_t len;

extern uint8_t lora_len;
extern uint8_t espnow_len;

extern bool rtt_back;

// Globalne zmienne pomiarowe
extern float current_mA;
extern float current_mA_peak;

// Zmienna przechowujaca RSSI
extern int rssi;

// Zmienna czasu nadawania
extern uint32_t tx_time;

void lora_send_sequence();
void lora_receive_sequence();
void espnow_send_sequence();
void espnow_receive_sequence(const esp_now_recv_info_t *info, const uint8_t *data, int len);

#endif // RADIO_H