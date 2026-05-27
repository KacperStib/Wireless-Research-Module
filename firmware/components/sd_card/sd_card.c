#include <stdio.h>
#include "include/sd_card.h"
#include "ina219.h"

char LOG_FILE_NAME[64] = {0,};
char PP_FILE_NAME[64] = {0,};

bool sd_card_ready = 0;

// Kolejki do zapisu
QueueHandle_t xLogQueue = NULL;
QueueHandle_t xProfileQueue = NULL;

static power_profile_t tx_profile;               

// Inicjalizacja sprzetowa i montowanie karty SD w trybie SPI
esp_err_t sd_card_init(void)
{	
    sdmmc_card_t *card;
    
    // Inicjalizacja SPI w osobnym komponencie spi (wspolne dla SD i LoRa)
    
    // Konfiguracja hosta SD-SPI 
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
 
    // Konfiguracja slotu SD oraz przypisanie pinu CS
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS;
    slot_config.host_id = (spi_host_device_t)host.slot; // SPI2_HOST
 
    // Parametry montowania systemu plikow FAT
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files              = 5,
        .allocation_unit_size   = 16 * 1024,
    };
 
    ESP_LOGI(SD_TAG, "Montowanie karty SD (CS=GPIO%d)...", SD_CS);
    esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    
    if (ret != ESP_OK) 
    {
        if (ret == ESP_FAIL) 
        {
            ESP_LOGE(SD_TAG, "Blad montowania systemu plikow.");
        } 
        else 
        {
            ESP_LOGE(SD_TAG, "Blad inicjalizacji karty SD: %s", esp_err_to_name(ret));
        }
        return ret;
    }
 
    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(SD_TAG, "Karta SD zamontowana w %s", MOUNT_POINT);
    sd_card_ready = 1;
    return ESP_OK;
}

// Nadpisanie pliku nowymi danymi
esp_err_t s_example_write_file(const char *path, char *data)
{
    ESP_LOGI("SD", "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) 
    {
        ESP_LOGE("SD", "Failed to open file for writing");
        return ESP_FAIL;
    }
    
    fprintf(f, "%s", data);
    fclose(f);
    ESP_LOGI("SD", "File written");

    return ESP_OK;
}

// Dopisywanie danych na koncu pliku
esp_err_t s_example_append_file(const char *path, char *data)
{
    FILE *f = fopen(path, "a");
    if (f == NULL) 
    {
        ESP_LOGE("SD", "Failed to open file for writing");
        return ESP_FAIL;
    }
    
    fprintf(f, "%s", data);
    fclose(f);
    return ESP_OK;
}

// Odczyt zawartosci pliku (pierwszej linii)
esp_err_t s_example_read_file(const char *path)
{
    ESP_LOGI("SD", "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) 
    {
        ESP_LOGE("SD", "Failed to open file for reading");
        return ESP_FAIL;
    }
    
    char line[EXAMPLE_MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // Usuniecie znaku nowej linii z konca bufora
    char *pos = strchr(line, '\n');
    if (pos) 
    {
        *pos = '\0';
    }
    ESP_LOGI("SD", "Read from file: '%s'", line);

    return ESP_OK;
}

// Sprawdzenie czy plik istnieje w systemie plikow
bool file_exists(const char *path) 
{
    FILE *fd = fopen(path, "r");
    if (fd == 0) 
    {
        return false;
    }
    fclose(fd);
    return true;
}

// Sformatowanie zdarzenia i wyslanie go do kolejki logowania SD
esp_err_t sd_log_event(bool tech, bool is_tx, float current_mA_peak, int rssi, bool gps_fix, float gps_lat, float gps_lon)
{
    log_event_t ev = {
        .has_gps  = gps_fix,
        .lat      = gps_lat,
        .lon      = gps_lon,
    };
		
    if (is_tx)
    {
        // Konfiguracja struktury dla zdarzenia nadawczego TX
        ev.is_tx    = true;
        ev.peak_mA  = current_mA_peak;
        strncpy(ev.tech, tech ? "LORA" : "ESPNOW", sizeof(ev.tech));
    }
    else 
    {
        // Konfiguracja struktury dla zdarzenia odbiorczego RX
        ev.is_tx   = false;
        ev.rssi    = rssi;
        strncpy(ev.tech, tech ? "LORA" : "ESPNOW", sizeof(ev.tech));
    }
	
    // Przeslanie gotowego obiektu zdarzenia do kolejki logow
    if (xQueueSend(xLogQueue, &ev, 0) != pdTRUE)
    {
        return ESP_ERR_NO_MEM; // Kolejka jest pelna
    }
        
    return ESP_OK;
}

// Wylap probki do profilowania energetycznego i wrzuc do kolejki
void sd_capture_and_log_profile(const char* tech, uint32_t duration_ms, int64_t *tx_end_time, int64_t t_start, float *current_mA_peak)
{
    if (xProfileQueue == NULL) return;
	
	// Przygotuj zmienne
    memset(&tx_profile, 0, sizeof(power_profile_t));
    strncpy(tx_profile.tech, tech, sizeof(tx_profile.tech) - 1);
    
    // Zapisz próbki bezpośrednio do struktury modułu SD i odczytaj pik
    *current_mA_peak = ina219_capture_profile(&tx_profile, duration_ms, tx_end_time);
    
    // Wypychnij do kolejki
    xQueueSend(xProfileQueue, &tx_profile, 0);
}

// Zapisz pomiary profilowania energetycznego do .csva
void sd_save_profile_to_csv(const power_profile_t *prof)
{
    if (!sd_card_ready) return;
	
	// Nazwa pliku
    uint32_t time_sec = (uint32_t)(esp_timer_get_time() / 1000000) % 100000;
    char tech_char = (prof->tech[0] == 'L' || prof->tech[0] == 'l') ? 'L' : 'E';
    snprintf(PP_FILE_NAME, sizeof(PP_FILE_NAME), "%s/P_%c%lu.csv", MOUNT_POINT, tech_char, time_sec);
    
    // Operacja na plikach
    FILE *f = fopen((const char*)PP_FILE_NAME, "w");
    if (f == NULL) {
        ESP_LOGE("SD", "Nie udalo sie otworzyc pliku %s do zapisu!", (const char*)PP_FILE_NAME);
        return;
    }
    
    // Naglowek jak w power profiler kit 2
    fprintf(f, "Timestamp(ms),Current(uA)\n");
    
    for(uint32_t i = 0; i < prof->total_samples; i++) 
    {
        fprintf(f, "%.2f,%.3f\n", 
                (float)prof->samples[i].rel_time_us / 1000.0f, 
                prof->samples[i].current_mA * 1000.0f);
    }
    fclose(f);
    ESP_LOGI("SD", "Profil zapisany: %s (probek: %lu)", PP_FILE_NAME, prof->total_samples);
}