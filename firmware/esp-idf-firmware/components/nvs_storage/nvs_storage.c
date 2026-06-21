#include "include/nvs_storage.h"

static const char *TAG = "nvs_cfg";

// Zapis struktury konfiguracyjnej do pamieci NVS
esp_err_t nvs_cfg_save(const radio_config_t *cfg)
{
    nvs_handle_t h;
    esp_err_t err;

    // Otwarcie pamieci NVS w trybie odczytu i zapisu
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "nvs_open: %s", esp_err_to_name(err));
        return err;
    }

    // Zapis struktury jako dane binarne (blob) i zatwierdzenie zmian
    err = nvs_set_blob(h, NVS_KEY, cfg, sizeof(*cfg));
    if (err == ESP_OK)
    {
        err = nvs_commit(h);
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_set_blob/commit: %s", esp_err_to_name(err));
    }

    nvs_close(h);
    return err;
}

// Odczyt struktury konfiguracyjnej z pamieci NVS
esp_err_t nvs_cfg_load(radio_config_t *cfg)
{
    nvs_handle_t h;
    esp_err_t err;

    // Otwarcie pamieci NVS w trybie tylko do odczytu
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "nvs_open: %s", esp_err_to_name(err));
        return err;
    }

    size_t len = sizeof(*cfg);
    err = nvs_get_blob(h, NVS_KEY, cfg, &len);

    // Walidacja pobranych danych i sprawdzenie rozmiaru bloba
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Brak zapisanego configu w NVS");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_get_blob: %s", esp_err_to_name(err));
    }
    else if (len != sizeof(*cfg)) 
    {
        ESP_LOGW(TAG, "Rozmiar bloba (%u) != sizeof(radio_config_t) (%u) - ignoruje",
                 (unsigned)len, (unsigned)sizeof(*cfg));
        err = ESP_ERR_INVALID_SIZE;
    }

    nvs_close(h);
    
    printf("=== Konfiguracja ===\n");
    printf("  tech:     %s\n", radio_cfg.tech == RADIO_TECH_LORA ? "lora" : "espnow");
    printf("  dir:      %s\n", radio_cfg.dir == RADIO_DIR_TX ? "tx" : "rx");
    printf("  peer_mac: " MACSTR "\n", MAC2STR(radio_cfg.peer_mac));
    printf("  test:     %d\n", radio_cfg.test);
    
    return err;
}