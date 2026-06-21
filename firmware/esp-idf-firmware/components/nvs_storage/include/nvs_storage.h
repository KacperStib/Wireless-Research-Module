/*
 * nvs_storage.h
 *
 * Modul obslugi pamieci nieulotnej NVS (Non-Volatile Storage) dla ESP32.
 * Umozliwia zapis oraz odczyt struktury konfiguracyjnej radia z pamieci flash.
 */

#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include "dev_config.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#define NVS_NAMESPACE  "radio" // Przestrzen nazw w pamieci NVS
#define NVS_KEY        "cfg"   // Klucz dla struktury konfiguracji radia

// Zapis struktury konfiguracyjnej do pamieci NVS
esp_err_t nvs_cfg_save(const radio_config_t *cfg);

// Odczyt struktury konfiguracyjnej z pamieci NVS
esp_err_t nvs_cfg_load(radio_config_t *cfg);

#endif // NVS_STORAGE_H