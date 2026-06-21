/**
 * @file shell_mng.h
 * @brief Moduł konsoli interaktywnej REPL (Shell) dla ESP32.
 *
 * Plik udostępnia funkcje inicjalizujące wbudowaną linię komend (domyślnie
 * przez USB-Serial-JTAG), umożliwiającą dynamiczną konfigurację radia, logów,
 * zarządzanie pamięcią NVS oraz diagnostykę zadań FreeRTOS.
 * 
 * @note W przypadku zmiany interfejsu fizycznego na klasyczny UART należy 
 * odpowiednio zmodyfikować konfigurację w menuconfig (esp_console).
 */

#ifndef SHELL_MNG_H
#define SHELL_MNG_H

#include "dev_config.h"
#include "esp_now_c.h"
#include "lora.h"
#include "nvs_storage.h"

/**
 * @brief Inicjalizuje podsystem konsoli i uruchamia pętlę REPL.
 *
 * Funkcja konfiguruje peryferia USB_SERIAL_JTAG, ustawia znak zachęty (prompt),
 * rejestruje wszystkie dostępne komendy użytkownika i uruchamia wątek konsoli.
 */
void shell_init(void);

/**
 * @brief Rejestruje komendy powiązane z systemem logowania (ESP_LOG).
 *
 * Dodaje komendę:
 *   - log <level> [tag] -> Zmienia poziom logowania (none, err, wrn, inf, dbg)
 */
void console_log_register(void);

/**
 * @brief Rejestruje komendy operacyjne układu radiowego i systemu.
 *
 * Dodaje komendy:
 *   - radio  -> Przełączanie technologii (lora / espnow)
 *   - dir    -> Zmiana kierunku pracy (rx / tx)
 *   - mac    -> Ustawienie zdalnego adresu MAC docelowego urządzenia
 *   - mymac  -> Odczyt własnego adresu MAC interfejsu Wi-Fi
 *   - config -> Zarządzanie konfiguracją (show / save / load) w NVS
 *   - lora   -> Zmiana parametrów RF (sf, bw, cr, pwr) dla LoRa
 *   - tasks  -> Podgląd listy uruchomionych zadań FreeRTOS
 *   - reset  -> Programowy restart układu ESP32
 */
void console_radio_register(void);

#endif // SHELL_MNG_H