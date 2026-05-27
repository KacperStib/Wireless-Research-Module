#include "include/esp_now_c.h"
#include "dev_config.h"

#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define TAG "ESPNOW"

static espnow_rx_cb_t s_rx_cb = NULL;
static bool s_wifi_initialized;

int rssi = 0;

uint32_t tx_time = 0;

// Callbacki ESP-NOW
// Przy wysylce
static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    ESP_LOGD(TAG, "TX %s", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// Przy odbiorze
static void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{	
    rssi = info->rx_ctrl->rssi; 
    sd_log_event(false, false, 0, rssi, gps_fix, gps_lat, gps_lon);
	
    ESP_LOGD(TAG, "RX %d B od " MACSTR, len, MAC2STR(info->src_addr));
    
    if (s_rx_cb)
    {
        s_rx_cb(data, len);
    }
}

// Inicjalizacja
esp_err_t espnow_init(espnow_rx_cb_t rx_callback)
{
    if (s_wifi_initialized) 
    {
        ESP_LOGW(TAG, "ESP-NOW juz zainicjalizowane");
        return ESP_OK;
    }

    s_rx_cb = rx_callback;

    // Inicjalizacja sieci i domyslnej petli zdarzen
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Konfiguracja i uruchomienie WiFi w trybie Station
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Inicjalizacja ESP-NOW i rejestracja funkcji zwrotnych
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    s_wifi_initialized = true;
    ESP_LOGI(TAG, "ESP-NOW init OK");
    return ESP_OK;
}

// Ustaw peer
esp_err_t espnow_set_peer(const uint8_t mac[6])
{
    // Usun starego peera jesli istnieje
    esp_now_del_peer(mac);

    esp_now_peer_info_t peer = {
        .channel = 0, // 0 = aktualny kanal WiFi
        .ifidx   = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, mac, 6);

    esp_err_t err = esp_now_add_peer(&peer);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Peer: " MACSTR, MAC2STR(mac));
    }
    else
    {
        ESP_LOGE(TAG, "Blad dodawania peer: 0x%x", err);
    }
    
    return err;
}

// Wysylka danych
esp_err_t espnow_send(const uint8_t *data, size_t len)
{
    return esp_now_send(radio_cfg.peer_mac, data, len);
}

// Deinit espnow
void espnow_deinit(void)
{
    if (!s_wifi_initialized) 
    {
        return;
    }
    
    esp_now_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    s_wifi_initialized = false; 
}

void espnow_rx_handler(const uint8_t *data, int len) 
{
    ESP_LOGI("RADIO", "ESPNOW RX %d B: %.*s", len, len, data);
}