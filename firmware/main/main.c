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

// Instancja wyswietlacza
SSD1306_t dev;

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
		char buf[32];
		
		// Zuzycie chwilowe pradu
		memset(buf, ' ', sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
		sprintf(buf, "Prad: %.2f", current_mA);
		ssd1306_display_text(&dev, 1, buf,sizeof(buf) - 1, false);
		
		// FIX GPS
		memset(buf, ' ', sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
		sprintf(buf, "GPS FIX: %s (%d)", gps_fix ? "Y" : "N", gps_sats);
		ssd1306_display_text(&dev, 2, buf, sizeof(buf) - 1, false);
		// Wspolrzedne GPS
		if (gps_fix)
		{
			memset(buf, ' ', sizeof(buf) - 1);
        	buf[sizeof(buf) - 1] = '\0';
			sprintf(buf, "%.2f N %.2f E", gps_lat, gps_lon);
			ssd1306_display_text(&dev, 3, buf, sizeof(buf) - 1, false);
		}
		
		// Technologia radiowa
		memset(buf, ' ', sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
		sprintf(buf, "%s %s",
				radio_cfg.tech == RADIO_TECH_LORA ? "LORA" : "ESPNOW",
				radio_cfg.dir ? "TX" : "RX");
		ssd1306_display_text(&dev, 4, buf, sizeof(buf) - 1, false);
		
		// Informacje o transmisji
		memset(buf, ' ', sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        // RSSI tylko w trybie RX
        if (radio_cfg.dir == RADIO_DIR_RX) 
        {
            sprintf(buf, "RSSI: %d dBm", rssi);
            ssd1306_display_text(&dev, 5, buf, sizeof(buf) - 1, false);
        } 
        // Pik pradowy w trybie TX
        else 
        {	
			memset(buf, ' ', sizeof(buf) - 1);
        	buf[sizeof(buf) - 1] = '\0';
			sprintf(buf, "Peak: %.2f mA", current_mA_peak);
            ssd1306_display_text(&dev, 5, buf, sizeof(buf) - 1, false); // czyść linię
            
            if (tx_time < 2000) 
            {
                // Dla bardzo krótkich czasów (np. ESP-NOW) wypisujemy w uS
                sprintf(buf, "TX Time: %lu us", tx_time);
            } 
            else 
            {
                // Dla dłuższych czasów (np. LoRa) zamieniamy na milisekundy z przecinkiem
                sprintf(buf, "TX Time: %.1f ms", (float)tx_time / 1000.0f);
            }
			ssd1306_display_text(&dev, 6, buf, sizeof(buf) - 1, false);
        }
        
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
	
    for (;;) 
    {
		// 1. Kolejka ogolnych logow
		if (xQueueReceive(xLogQueue, &ev, portMAX_DELAY) == pdTRUE)
        {
            if (!sd_card_ready)
                sd_card_init();

            sd_save_event_to_csv(&ev);
        }
	    
	    // 2. Kolejka logow profilowania pradowego
	    if (xQueueReceive(xProfileQueue, &pp, 0) == pdTRUE) 
        {
            sd_save_profile_to_csv(&pp);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ====== TASK RADIO ======
// Zarzadza przesylaniem lub odbieraniem danych w wybranej technologii
void vRadioTask(void *pv) 
{
	uint8_t buf8[32];
	for (;;) 
	{	
		// Technologia LoRa
		if (radio_cfg.tech == RADIO_TECH_LORA)
		{	
			// tryb TX
			if (radio_cfg.dir)
			{
		        int send_len = sprintf((char *)buf8, "Hello World!!");
			
			    int64_t t_start = esp_timer_get_time();
			    int64_t t_end_tx = 0; 
			    
			    // Start nadawania
			    lora_send_packet(buf8, send_len); 
			    
			    // Wywolanie funkcji z sd_card, ktora sama podstawi strukture
			    sd_capture_and_log_profile("LORA", 50, &t_end_tx, t_start, &current_mA_peak); 
			    
			    // Obliczanie czasu nadawania (tx_time)
			    if (t_end_tx != 0) 
			    {
			        tx_time = (uint32_t)(t_end_tx - t_start);
			    } 
			    else 
			    {
			        tx_time = (uint32_t)(esp_timer_get_time() - t_start);
			    }
			
			    // Log tekstowy do pliku log.txt
			    sd_log_event("LORA",  true,  current_mA_peak, 0,    gps_fix, gps_lat, gps_lon);
			
			    vTaskDelay(pdMS_TO_TICKS(6000));
		  	}
		  	
		  	// Tryb RX
		  	else 
		  	{
				lora_receive();
				if (lora_received()) 
				{
					int rxLen = lora_receive_packet(buf8, sizeof(buf8));
					// Zmierz RSSI
					rssi = (int)lora_packet_rssi();
					// Wrzuc log do kolejki dla karty SD
					sd_log_event("LORA",  false, 0,               rssi, gps_fix, gps_lat, gps_lon);
					ESP_LOGI(pcTaskGetName(NULL), "%d byte packet received:[%.*s]", rxLen, rxLen, buf8);
					
				}
				// W przypadku LoRa trzeba robic recznie polling wiadomosci, nie ma mechanizmu callbacku
				vTaskDelay(1); 
			}
		}
		// ESPNOW
		else 
		{ 
			// Tryb TX
			if (radio_cfg.dir)
			{
		        int len = sprintf((char *)buf8, "Hello ESP-NOW!");
                int64_t t_end_tx = 0;
                int64_t t_start = esp_timer_get_time();
                
                // Start nadawania ESP-NOW
                espnow_send(buf8, len);
                
                // Agresywne próbkowanie prądu dla ESP-NOW. 
                sd_capture_and_log_profile("ESPN", 5, &t_end_tx, t_start, &current_mA_peak);
                
                // Obliczanie czasu nadawania (tx_time)
                if (t_end_tx != 0) {
                    tx_time = (uint32_t)(t_end_tx - t_start);
                } else {
                    tx_time = (uint32_t)(esp_timer_get_time() - t_start);
                }
                
                // Zapis zdarzenia do ogólnego logu
                sd_log_event("ESPNOW", true, current_mA_peak, 0,    gps_fix, gps_lat, gps_lon);
                
                vTaskDelay(pdMS_TO_TICKS(6000));
		  	}
		  	// Tryb RX
		  	else 
		  	{	
				// Cala logika odbioru jest w callbacku, zarejestrowanym przy inicjalizacji
				vTaskDelay(pdMS_TO_TICKS(100));
			}
		}
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
	ina219_power_on(0.05, 6.4);
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
	//snprintf(LOG_FILE_NAME, sizeof(LOG_FILE_NAME), "%s/log.txt", MOUNT_POINT);
	//s_example_write_file((const char*)LOG_FILE_NAME, "Measurements:");
	
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
	
	// Rozpocznij Zadania
	xTaskCreate(vRadioTask,  "RADIO",  8192, NULL, 5, NULL);
	xTaskCreate(vLogTask,  "SD",  8192, NULL, 1, NULL);
	xTaskCreate(vPowerTask,  "PWR",  2048, NULL, 2, NULL);
	xTaskCreate(vOledTask,  "OLED",  4096, NULL, 3, NULL);
	
	// Konsola
	shell_init();
}