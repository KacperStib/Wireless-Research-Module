/*
 * ina219.h
 *
 * Modul obslugi czujnika pomiaru pradu i napiecia INA219 po magistrali I2C.
 * Zawiera rejestry ukladu, zmienne globalne oraz funkcje kalibracji i odczytu.
 */

#ifndef INA219_H
#define INA219_H

#include "i2c.h"
#include "sd_card.h"
#include <math.h>

#include <esp_timer.h>

#define INA219_ADDR             (0x40)
#define INA219_REG_CONFIG       (0x00)
#define INA219_REG_SHUNTVOLTAGE (0x01)
#define INA219_REG_BUSVOLTAGE   (0x02)
#define INA219_REG_POWER        (0x03)
#define INA219_REG_CURRENT      (0x04)
#define INA219_REG_CALIBRATION  (0x05)

#define TAG_PWR                 "PWR"

// Globalne zmienne pomiarowe
extern float current_mA;
extern float current_mA_peak;

// Rejestry konfiguracyjne czujnika
extern uint8_t ina_range;
extern uint8_t ina_gain;
extern uint8_t ina_b_res; 
extern uint8_t ina_s_res;
extern uint8_t ina_mode;

// Wartosci LSB do obliczen pradu i mocy
extern float currentLSB, powerLSB;

// Inicjalizacja i kalibracja ukladu INA219
esp_err_t ina219_power_on(float shuntVAL, float iMAX);

// Funkcje odczytu parametrow elektrycznych
float ina219_read_voltage(void);
float ina219_read_current(void);
float ina219_read_power(void);

// Szukanie najwyzszego piku pradowego
float ina219_find_peak(int64_t *rx_end);
// Uchwycenie profilu pradowego a'la power profiler kit (nordic)
float ina219_capture_profile(power_profile_t *prof, uint32_t duration_ms, int64_t *tx_end_time);

#endif // INA219_H