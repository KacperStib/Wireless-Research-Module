/**
 * @file main.c
 * @author Kacper Stiborski
 * @brief Główny moduł aplikacji urządzenia radiowego dla ESP32.
 *        Zarządza zadaniami FreeRTOS (Radio, SD, OLED, GPS, Power).
 * @version 1.0
 * @date 2026-05-16
 * 
 * @copyright Copyright (c) 2026
 * 
 */
 
#include <stdio.h>
#include <stdbool.h>
#include <sys/_intsup.h>
#include <unistd.h>
#include <esp_timer.h>

#include "esp_log.h"
#include "spi.h"
#include "ina219.h"
#include "oled.h"
#include "sd_card.h"
#include "lora.h"
#include "gps.h"
#include "shell_mng.h"
#include "dev_config.h"
#include "nvs_storage.h"
#include "radio.h"
#include "esp_task_wdt.h"

// ====== TASK PWR ======
// Cykliczny odczyt danych o zuzyciu pradu
void vPowerTask(void *pv) 
{
	for (;;) 
	{
    	current_mA = ina219_read_current() * 1000;
    	vTaskDelay(pdMS_TO_TICKS(1000));
  	}
}

// ====== TASK OLED ======
// Wyswietlanie informacji na ekranie
void vOledTask(void *pv) 
{
	for (;;) 
	{
		ssd1306_update(current_mA, gps_fix, gps_sats, gps_lat, gps_lon, per,
					radio_cfg, current_mA_peak, rssi, snr, tx_time, connected);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

// ====== TASK SD ======
// Zapis logow na karte SD
void vLogTask(void *pv) 
{
	// Instancja zdarzenia
	log_event_t ev;
	power_profile_t pp;
	log_per_t pl;
	
    for (;;) 
    {
		// 1. Kolejka ogolnych logow
		if (xQueueReceive(xLogQueue, &ev, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            if (!sd_card_ready)
                sd_card_init();

            sd_save_event_to_csv(&ev);
        }
	    
	    // 2. Kolejka logow profilowania pradowego
	    if (xQueueReceive(xProfileQueue, &pp, pdMS_TO_TICKS(1000)) == pdTRUE) 
        {	
			if (!sd_card_ready)
                sd_card_init();
                
            sd_save_profile_to_csv(&pp);
        }
        
        // 3. Kolejka logow packet lossow
	    if (xQueueReceive(xPerQueue, &pl, pdMS_TO_TICKS(1000)) == pdTRUE) 
        {	
			if (!sd_card_ready)
                sd_card_init();
                
            sd_save_per_to_csv(&pl);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ====== TASK RADIO ======
// Zarzadza przesylaniem lub odbieraniem danych w wybranej technologii
void vRadioTask(void *pv) 
{		
	for (;;) 
	{	
		// Technologia LoRa
		if (radio_cfg.tech == RADIO_TECH_LORA)
		{	
			// tryb TX
			if (radio_cfg.dir == RADIO_DIR_TX)
			{
		        lora_send_sequence();
			    vTaskDelay(pdMS_TO_TICKS(6000));
		  	}
		  	
		  	// Tryb RX
		  	else 
		  	{
				lora_receive_sequence();
				// W przypadku LoRa trzeba robic recznie polling wiadomosci, nie ma mechanizmu callbacku
				vTaskDelay(1); 
			}
		}
		// ESPNOW
		else
		{ 
			// Tryb TX
			if (radio_cfg.dir == RADIO_DIR_TX)
			{
		        espnow_send_sequence();
                vTaskDelay(pdMS_TO_TICKS(6000));
		  	}
		  	// Tryb RX
		  	else 
		  	{	
				// Cala logika odbioru jest w callbacku, zarejestrowanym przy inicjalizacji
				// zabepizczenie przy per
				check_per_espnow();
				vTaskDelay(pdMS_TO_TICKS(100));
			}
		}
		// Czas od ostatniej odebranej wiadomosci
		if ((esp_timer_get_time() / 1000) - last_recv >= connection_timeout)
			connected = false;
		else
 			connected = true;
	}
}

void app_main(void)
{	
	// Inicjalizacja magistrali I2C
	if (i2c_master_init() != ESP_OK)
		ESP_LOGE("I2c", "I2C init: ERROR");
	// Inicjalizacja magistrali SPI
	spi_init();
	
	// NVS — jedno wywołanie, tutaj, przed wszystkim co z NVS korzysta
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
	    ESP_ERROR_CHECK(nvs_flash_erase());
	    ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	
	// Wczytaj ostatni config z NVS (jeśli brak — zostaną defaulty z dev_config.c)
	nvs_cfg_load((radio_config_t *)&radio_cfg);
	
	// Zastosuj config sprzętowo dla radia
	radio_apply_config();

	// Urchomienie i config ina219
	ina219_power_on(0.2, 1.6);
	if (err != ESP_OK)
    	ESP_LOGE("INA219", "I2C CMD ERROR: 0x%x", err);	
	vTaskDelay(200 / portTICK_PERIOD_MS);
	
	// Inicjalizacja ekranu OLED
	ssd1306_init(&dev, 128, 64);
	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text(&dev, 0, "Radio Module", 13, false);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	
	// Inicjalizacja SD karty
	sd_card_init();
	
	// Inicjalizacja handlera GPS - tworzy task GPS
	/* NMEA parser configuration */
    nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT(GPS_RX);
    /* init NMEA parser library */
    nmea_parser_handle_t nmea_hdl = nmea_parser_init(&config);
    /* register event handler for NMEA parser library */
    nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);
	
	// Kolejka do zapisywania logow na SD
	xLogQueue = xQueueCreate(8, sizeof(log_event_t));
	xProfileQueue = xQueueCreate(2, sizeof(power_profile_t));
	xPerQueue = xQueueCreate(2, sizeof(log_per_t));
	
	
	// Rozpocznij Zadania
	xTaskCreate(vRadioTask,  "RADIO",  12288, NULL, 5, NULL);
	xTaskCreate(vLogTask,  "SD",  12288, NULL, 1, NULL);
	xTaskCreate(vPowerTask,  "PWR",  2048, NULL, 2, NULL);
	xTaskCreate(vOledTask,  "OLED",  4096, NULL, 3, NULL);
	
	// Konsola
	shell_init();
}