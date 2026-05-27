#include "ina219.h"
#include "esp_task_wdt.h"

// Globalne zmienne pomiarowe
float current_mA = 0;
float current_mA_peak = 0;

// Podstawowe parametry konfiguracyjne ukladu
uint8_t ina_range = 0b1;     // Zakres napiecia bus: 32 V
uint8_t ina_gain = 0b11;     // Wzmocnienie PGA: 320 mV
uint8_t ina_b_res = 0b0000;  // Szybko - 9 bit
uint8_t ina_s_res = 0b0000;  // Szybko - 9 bit
uint8_t ina_mode = 0b101;    // Wylaczamy pomiar napiecia !

float currentLSB = 0, powerLSB = 0;

// Inicjalizacja i kalibracja ukladu INA219
esp_err_t ina219_power_on(float shuntVAL, float iMAX)
{
    // Konfiguracja rejestru CONFIG
    uint16_t config = 0;
    config |= (ina_range << 13 | ina_gain << 11 | ina_b_res << 7 | ina_s_res << 3 | ina_mode);
    
    // Zapis konfiguracji do ukladu
    err = i2c_write_2byte(INA219_ADDR, INA219_REG_CONFIG, config);
	
    // Obliczenia kalibracyjne
    uint16_t calibrationValue;
    float iMaxPossible, minimumLSB;
    
    // Maksymalny mozliwy prad dla podanego bocznikowa (shunt)
    iMaxPossible = 0.32f / shuntVAL;
    minimumLSB = iMAX / 32767;
	
    // Zaokraglenie wartosci Current_LSB do najblizszej "rownej" liczby
    currentLSB = ceil(minimumLSB / 0.0001f) * 0.0001f;
    powerLSB = currentLSB * 20;
    
    // Wyznaczenie wartosci rejestru kalibracji zgodnie ze wzorem z noty katalogowej
    calibrationValue = (uint16_t)((0.04096) / (currentLSB * shuntVAL));
    
    // Zapis wartosci kalibracyjnej do ukladu
    err = i2c_write_2byte(INA219_ADDR, INA219_REG_CALIBRATION, calibrationValue);

    return err;
}

// Odczyt napiecia z szyny 
float ina219_read_voltage(void)
{
    uint8_t buf[2];
    i2c_write_reg(INA219_ADDR, INA219_REG_BUSVOLTAGE);
    i2c_read(INA219_ADDR, buf, 2);
    
    // Konwersja dwoch bajtow na uint16_t (MSB * 256 + LSB)
    uint16_t voltage = (uint16_t)buf[0] * 256 + (uint16_t)buf[1];
    
    // Przesuniecie o 3 bity w prawo (odrzucenie flag CNVR i OVF) oraz skalowanie do woltow
    return ((voltage >> 3) * 4 * 0.001);
}

// Odczyt aktualnego pradu
float ina219_read_current(void)
{
    uint8_t buf[2];
    //i2c_write_reg(INA219_ADDR, INA219_REG_CURRENT);
    //i2c_read(INA219_ADDR, buf, 2);
    
    i2c_write_read(INA219_ADDR, INA219_REG_CURRENT, buf, 2);
    
    // Konwersja bajtow na surowa wartosc pradu
    float current = (uint16_t)buf[0] * 256 + (uint16_t)buf[1];
    current = current * currentLSB;
    
    ESP_LOGD(TAG_PWR, "CURRENT: %.2f", current);
    return current;
}

// Odczyt aktualnego poboru mocy
float ina219_read_power(void)
{
    uint8_t buf[2];
    i2c_write_reg(INA219_ADDR, INA219_REG_POWER);
    i2c_read(INA219_ADDR, buf, 2);
    
    // Konwersja bajtow na surowa wartosc mocy
    float power = (uint16_t)buf[0] * 256 + (uint16_t)buf[1];
    power = power * powerLSB;
    
    ESP_LOGD(TAG_PWR, "POWER: %.2f", power);
    return power;
}

// Przeszukanie profilu pradowego w poszukiwaniu najwyzszego piku
// Predkosc magistrali I2C podniesiona ze 100 kHz do 400 kHz w celu zvwiekszenia czestotliwosci probkowania
float ina219_find_peak(int64_t *rx_end)
{
    float peak = 0, sample;
    TickType_t t_end = xTaskGetTickCount() + pdMS_TO_TICKS(100);
	
    while (xTaskGetTickCount() < t_end) 
    {
        sample = ina219_read_current();
        if (sample > peak)
        { 
            peak = sample; 
        }
        
        if (*rx_end == 0 && sample < 0.120f)
        	*rx_end = esp_timer_get_time();
        esp_rom_delay_us(100);
        //vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    return peak;
}

// Profilowanie energetyczne - przekaz strukture, czas trwania i wskaznik do zwrotu czasu piku
float ina219_capture_profile(power_profile_t *prof, uint32_t duration_ms, int64_t *tx_end_time)
{	
	// Zmienne
    float peak_mA = 0;
    uint32_t idx = 0;
    *tx_end_time = 0;

    const float scale = currentLSB * 1000.0f;
    const int64_t start_time = esp_timer_get_time();
    const int64_t end_time   = start_time + ((int64_t)duration_ms * 1000);
	
	// Petla odczytu probek
    while (idx < PROFILE_SAMPLES)
    {	
		// jedno wywołanie na iterację zeby nie zamulac
        int64_t now = esp_timer_get_time(); 
        if (now >= end_time) break;
		
		// Super szybki odczyt po i2c zeby nie zamulac
        uint8_t buf[2];
        esp_err_t ret = i2c_write_read_fast(INA219_ADDR, INA219_REG_CURRENT, buf, 2);
        
        // --- ZMIANA: Break całej funkcji przy błędzie ---
        if (ret != ESP_OK) {
            ESP_LOGE("RADIO", "Błąd I2C (0x%x), przerywam zbieranie profilu.", ret);
            break; // Całkowite wyjście z pętli i funkcji
        }
        
        int16_t raw       = (int16_t)((buf[0] << 8) | buf[1]);
        float   sample_mA = raw * scale;
		
		// Detekcja piku
        if (sample_mA > peak_mA) peak_mA = sample_mA;
		
		// Timestamp jesli to pik
        if (*tx_end_time == 0 && sample_mA < 120.0f && idx > 10)
            *tx_end_time = now;

        prof->samples[idx].rel_time_us = (uint32_t)(now - start_time);
        prof->samples[idx].current_mA  = sample_mA;
        idx++;
        // zero delay — I2C samo w sobie zajmuje ~90µs (nawet po 1 MHz)
        if (idx % 50 == 0) {
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }
    }
	taskYIELD();
    prof->total_samples = idx;
    return peak_mA;
}