#include <stdio.h>
#include "esp_timer.h"
#include "radio.h"

#include <inttypes.h>

uint8_t buf8[250];

// Globalne zmienne pomiarowe
float current_mA = 0;
float current_mA_peak = 0;

int rssi = 0;
uint32_t tx_time = 0;

// Test predkosci
static uint16_t packets_received = 0;
static uint32_t speed_test_start = 0;

// Test PER
static uint32_t last_seq = 0;
static uint32_t total_received = 0;
static uint32_t total_lost = 0;
static uint8_t num;

void lora_send_sequence()
{
	// radio_cfg, payload, current_mA_peak, tx_time
	int send_len = sprintf((char *)buf8, "Hello World!!");
	int64_t t_start = esp_timer_get_time();
	int64_t t_end_tx = 0; 
			    
	// Start nadawania
	switch (radio_cfg.test)
	{
		case TEST_IDLE:
			lora_send_packet(buf8, send_len); 
			vTaskDelay(10 / portTICK_PERIOD_MS); 			// Opoznienie zeby zlapac pik (LoRa jest wolniejsza)
			current_mA_peak = ina219_find_peak(&t_end_tx);
			// Obliczanie czasu nadawania (tx_time)
			if (t_end_tx != 0) 
			{
				tx_time = (uint32_t)(t_end_tx - t_start);
			} 
			else 
			{
				tx_time = (uint32_t)(esp_timer_get_time() - t_start);
			}
			break;
		
		case TEST_GENERAL:
			lora_send_packet(buf8, send_len); 
			vTaskDelay(10 / portTICK_PERIOD_MS); 			// Opoznienie zeby zlapac pik (LoRa jest wolniejsza)
			current_mA_peak = ina219_find_peak(&t_end_tx);
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
			sd_log_event("LORA",  true,  current_mA_peak, 0, gps_fix, gps_lat, gps_lon);
			break;
			
		case TEST_POWER:
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
			break;
			
		case TEST_SPEED:
			for (int i = 0; i < 50; i++)
		    {	
				lora_sent = false;
		      	lora_send_packet(buf8, 250);
		      	while(!lora_sent);
		    }
		    vTaskDelay(1);
			break;
			
		case TEST_PER:
			break;
			
		default:
			break;
	}	    		    
}

void lora_receive_sequence()
{
	switch (radio_cfg.test)
	{
		case TEST_IDLE:
			lora_receive();
			if (lora_received()) 
			{
				int rxLen = lora_receive_packet(buf8, sizeof(buf8));
				// Zmierz RSSI
				rssi = (int)lora_packet_rssi();				
				ESP_LOGI(pcTaskGetName(NULL), "%d byte packet received:[%.*s]", rxLen, rxLen, buf8);
							
			}
			break;
		
		case TEST_GENERAL:
			lora_receive();
			if (lora_received()) 
			{
				int rxLen = lora_receive_packet(buf8, sizeof(buf8));
				// Zmierz RSSI
				rssi = (int)lora_packet_rssi();
							
				// Wrzuc log do kolejki dla karty SD
				sd_log_event("LORA",  false, 0, rssi, gps_fix, gps_lat, gps_lon);
							
				ESP_LOGI(pcTaskGetName(NULL), "%d byte packet received:[%.*s]", rxLen, rxLen, buf8);
							
			}
			break;
			
		case TEST_POWER:
			lora_receive();
			if (lora_received()) 
			{
				int rxLen = lora_receive_packet(buf8, sizeof(buf8));
				// Zmierz RSSI
				rssi = (int)lora_packet_rssi();					
							
				vTaskDelay(200 / portTICK_PERIOD_MS);
				sd_capture_and_log_profile("LORA", 100, NULL, NULL, NULL);
							
				ESP_LOGI(pcTaskGetName(NULL), "%d byte packet received:[%.*s]", rxLen, rxLen, buf8);
							
			}
			break;
			
		case TEST_SPEED:
			if (packets_received == 0)
				speed_test_start = esp_timer_get_time();
			
			lora_receive();
			if (lora_received()) 
			{
				lora_receive_packet(buf8, sizeof(buf8));
				packets_received++;
			}
			
			if (esp_timer_get_time() - speed_test_start >= 1000000)
			{
				ESP_LOGI("TEST", "Speed Test:%u - %0.3fMbps", packets_received, (packets_received * 8.0f * 250.0f) / 1000000.0f);
				packets_received = 0;
        		speed_test_start = 0;
        	}
			break;
			
		case TEST_PER:
			break;
			
		default:
			break;
	}
}

void espnow_send_sequence()
{	
	int len = sprintf((char *)buf8, "Hello ESP-NOW!");
    int64_t t_end_tx = 0;
    int64_t t_start = esp_timer_get_time();
    
	switch (radio_cfg.test)
	{
		case TEST_IDLE:
			// Start nadawania ESP-NOW
		    espnow_send(buf8, len);
		                
			current_mA_peak = ina219_find_peak(&t_end_tx);
					    	
			// Obliczanie czasu nadawania (tx_time)
			if (t_end_tx != 0) 
			{
				tx_time = (uint32_t)(t_end_tx - t_start);
			} 
			else 
			{
				tx_time = (uint32_t)(esp_timer_get_time() - t_start);
		   	}		         
			break;
		
		case TEST_GENERAL:
			// Start nadawania ESP-NOW
		    espnow_send(buf8, len);
		                
			current_mA_peak = ina219_find_peak(&t_end_tx);
					    	
			// Obliczanie czasu nadawania (tx_time)
			if (t_end_tx != 0) 
			{
				tx_time = (uint32_t)(t_end_tx - t_start);
			} 
			else 
			{
				tx_time = (uint32_t)(esp_timer_get_time() - t_start);
		   	}
		                
		    // Zapis zdarzenia do ogólnego logu
		    sd_log_event("ESPNOW", true, current_mA_peak, 0,    gps_fix, gps_lat, gps_lon);
			break;
			
		case TEST_POWER:
			// Start nadawania ESP-NOW
		    espnow_send(buf8, len);
		                
		    // Agresywne próbkowanie prądu dla ESP-NOW. 
		    sd_capture_and_log_profile("ESPNOW", 5, &t_end_tx, t_start, &current_mA_peak);
					    	
			// Obliczanie czasu nadawania (tx_time)
			if (t_end_tx != 0) 
			{
				tx_time = (uint32_t)(t_end_tx - t_start);
			} 
			else 
			{
				tx_time = (uint32_t)(esp_timer_get_time() - t_start);
		   	}		            
			break;
			
		case TEST_SPEED:
			for (int i = 0; i < 500; i++)
		    {	
				espnow_sent = false;
		      	espnow_send(buf8, 250);
		      	while(!espnow_sent);
		    }
		    vTaskDelay(1);

			break;
			
		case TEST_PER:
			// Wysłanie 100 paczek w jednej sekwencji
		    for (int i = 0; i < 100; i++) {		        
		        // Wypełnienie ramki
		        uint8_t seq = (uint8_t)i;
		        espnow_send(&seq, 1);        		 
		        vTaskDelay(20 / portTICK_PERIOD_MS); 
		    }
			break;
			
		default:
			break;
	}            
}

void espnow_receive_sequence(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{	
	rssi = info->rx_ctrl->rssi;
	
	switch (radio_cfg.test)
	{
		case TEST_IDLE:
			break;
		
		case TEST_GENERAL:
			sd_log_event("ESPNOW", false, 0, rssi,   gps_fix, gps_lat, gps_lon);
			break;
			
		case TEST_POWER:
			vTaskDelay(200 / portTICK_PERIOD_MS);
			sd_capture_and_log_profile("ESPNOW", 100, NULL, NULL, NULL);
			break;
			
		case TEST_SPEED:
			if (packets_received == 0)
				speed_test_start = esp_timer_get_time();
				
			packets_received++;
			
			if (esp_timer_get_time() - speed_test_start >= 1000000)
			{
				ESP_LOGI("TEST", "Speed Test:%u - %0.3fMbps", packets_received, (packets_received * 8.0f * 250.0f) / 1000000.0f);
				packets_received = 0;
        		speed_test_start = 0;
        	}
			break;
			
		case TEST_PER:
        
	       	num = *data;

		    // Start nowej serii
		    if (num == 0) {
		        total_received = 0;
		        total_lost = 0;
		        speed_test_start = esp_timer_get_time(); // Zapisujemy czas startu serii
		    }
		
		    // Liczenie strat
		    if (num > (last_seq + 1) && last_seq != 0) {
		        total_lost += (num - last_seq - 1);
		    }
		    
		    last_seq = num;
		    total_received++;
		
		    // Sprawdzenie: Czy minęło za dużo czasu (np. 5s) LUB czy mamy już ostatni pakiet?
		    int64_t time_elapsed = (esp_timer_get_time() - speed_test_start) / 1000;
		    
		    if (num == 99 || (time_elapsed > 5000 && total_received > 0)) {
		        float per = (float)total_lost / (float)(total_received + total_lost) * 100.0f;
		        
		        ESP_LOGI("PER", "--- PODSUMOWANIE ---");
		        ESP_LOGI("PER", "Odebrano: %" PRIu32 " | Stracono: %" PRIu32 " | PER: %.2f%%", 
		                 total_received, total_lost, (double)per);
		        
		        // Reset po serii
		        last_seq = 0; // Ustawiamy 0, żeby kolejna seria była rozpoznana jako start
		    }
			break;
			
		default:
			break;
	}
	 
    ESP_LOGD(TAG, "RX %d B od " MACSTR, len, MAC2STR(info->src_addr));
    
}