/*
 * sd_card.h
 *
 * Modul obslugi karty SD w trybie SPI oraz kolejkowania logow systemowych.
 * Definiuje struktury danych dla zdarzen, parametry montowania i funkcje plikowe.
 */

#ifndef SD_CARD_H
#define SD_CARD_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include "esp_log.h"
#include <string.h>
#include <esp_timer.h>

#include <driver/sdspi_host.h>
#include "driver/spi_common.h"
#include <driver/sdmmc_host.h>
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#include "../../main/bsp.h"

#define SD_TAG "SD_CARD"

#define EXAMPLE_MAX_CHAR_SIZE    64
#define MOUNT_POINT "/sdcard"

// Zmienne pomocnicze
extern char PP_FILE_NAME[64];
extern char LOG_FILE_NAME[64];
extern bool sd_card_ready;

// Struktura do zapisywania logow podstawowych (RX/TX z danymi RSSI, pradu i GPS)
typedef struct {
    char tech[8];       
    bool is_tx;
    float peak_mA;      
    int rssi;           
    bool has_gps;
    float lat, lon;
} log_event_t;

extern QueueHandle_t xLogQueue;

// Struktura do profilowania zuzycia pradu
#define PROFILE_SAMPLES 500

typedef struct {
    uint32_t rel_time_us; // Czas względny od początku nadawania
    float current_mA;     // Prąd w mA
} profile_sample_t;

typedef struct {
    char tech[8];
    uint32_t total_samples;
    profile_sample_t samples[PROFILE_SAMPLES];
} power_profile_t;

extern QueueHandle_t xProfileQueue;

// Inicjalizacja sprzetowa i montowanie karty SD w systemie plikow FAT
esp_err_t sd_card_init(void);

// Podstawowe operacje na plikach
esp_err_t s_example_write_file(const char *path, char *data);
esp_err_t s_example_append_file(const char *path, char *data);
esp_err_t s_example_read_file(const char *path);
bool file_exists(const char *path);

// Funkcja pomocnicza do zapisu zdarzenia do kolejki logow
esp_err_t sd_log_event(bool tech, bool is_tx, float current_mA_peak, int rssi, bool gps_fix, float gps_lat, float gps_lon);

// Funkcja pomocnicza do zapisu zdarzen do kolejki profilowania energetycznego
void sd_capture_and_log_profile(const char* tech, uint32_t duration_ms, int64_t *tx_end_time, int64_t t_start, float *current_mA_peak);
void sd_save_profile_to_csv(const power_profile_t *prof);

#endif // SD_CARD_H