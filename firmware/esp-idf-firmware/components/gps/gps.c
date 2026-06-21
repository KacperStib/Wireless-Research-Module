#include "gps.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// PRZEPLYW DANYCH W PARSERZE NMEA:
// [UART] -> (Znak '\n') -> esp_handle_uart_pattern() -> gps_decode() -> parse_item()
//                                                          │
//   ┌──────────────────────────┬───────────────────────────┴──────────────────────────┐
//   ▼                          ▼                                                      ▼
// parse_gga()                parse_rmc()                                            parse_gll()
//   │                          │                                                      │
//   └──────────────────────────┴───────────────────────────┬──────────────────────────┘
//                                                          ▼
//                                                  parse_lat_long() -> [Stopnie dziesietne]
//                                                          │
//                                                          ▼
//                                                 esp_event_post_to() -> [Event Loop]
//                                                                               │
//                                                                               ▼
//                                                                      gps_event_handler() -> [Zmienne globalne]

// Konfiguracja buforow i kolejek petli zdarzen parsera NMEA
#define NMEA_PARSER_RUNTIME_BUFFER_SIZE (CONFIG_NMEA_PARSER_RING_BUFFER_SIZE / 2)
#define NMEA_MAX_STATEMENT_ITEM_LENGTH  (16)
#define NMEA_EVENT_LOOP_QUEUE_SIZE      (16)

// Definicja bazy zdarzen ESP Event dla podsystemu GPS
ESP_EVENT_DEFINE_BASE(ESP_NMEA_EVENT);

// Zmienne globalne - zasilaja bezposrednio system logowania SD oraz ekran OLED
volatile uint8_t gps_sats = 0;
volatile bool    gps_fix   = false;
volatile float   gps_lat   = 0.0f;
volatile float   gps_lon   = 0.0f;

static const char *GPS_TAG = "nmea_parser";

// Wewnetrzny kontekst pracy parsera GPS (glowny obiekt stanu)
typedef struct {
    uint8_t item_pos;                              // Biezacy indeks w parsowanym polu tekstowym
    uint8_t item_num;                              // Numer aktualnie przetwarzanego pola w ramce
    uint8_t asterisk;                              // Flaga wykrycia znaku '*' (poczatek sumy kontrolnej)
    uint8_t crc;                                   // Obliczona w locie suma kontrolna XOR ramki
    uint8_t parsed_statement;                      // Maska bitowa przetworzonych linii NMEA
    uint8_t cur_statement;                         // ID aktualnie rozpoznanej instrukcji (GGA/RMC/GLL)
    uint32_t all_statements;                       // Maska oczekiwanych instrukcji do pelnego update'u
    char item_str[NMEA_MAX_STATEMENT_ITEM_LENGTH]; // Bufor tekstowy na pojedyncze pole danych
    gps_t parent;                                  // Struktura wyjsciowa z czystymi danymi pozycji
    uart_port_t uart_port;                         // Przypisany port sprzetowy UART
    uint8_t *buffer;                               // Bufor roboczy na surowe linie tekstowe z UART
    esp_event_loop_handle_t event_loop_hdl;        // Uchwyt petli zdarzen obslugujacej GPS
    TaskHandle_t tsk_hdl;                          // Uchwyt zadania FreeRTOS parsera
    QueueHandle_t event_queue;                     // Kolejka zdarzen sterownika UART
} esp_gps_t;

// Konwertuje format wspolrzednych NMEA (ddmm.ssss / dddmm.ssss) na stopnie dziesietne
// Przyklad: 5212.4571 oznacza 52 stopnie i 12.4571 minut -> 52 + (12.4571 / 60)
static float parse_lat_long(esp_gps_t *esp_gps)
{
    float ll = strtof(esp_gps->item_str, NULL);
    int deg = ((int)ll) / 100;                 // Wyciagniecie samych stopni
    float min = ll - (deg * 100);              // Wyciagniecie minut i ich czesci ulamkowej
    return deg + min / 60.0f;                  // Zwrocenie pozycji w stopniach dziesietnych
}

#if CONFIG_NMEA_STATEMENT_GGA
// Parsowanie ramki $XXGGA (Glowna ramka danych fix i liczby satelitow)
static void parse_gga(esp_gps_t *esp_gps)
{
    switch (esp_gps->item_num) {
    case 2: // Szerokosc geograficzna (Latitude)
        esp_gps->parent.latitude = parse_lat_long(esp_gps);
        break;
    case 3: // Kierunek N (polnoc) / S (poludnie)
        if (esp_gps->item_str[0] == 'S' || esp_gps->item_str[0] == 's') {
            esp_gps->parent.latitude *= -1;
        }
        break;
    case 4: // Dlugosc geograficzna (Longitude)
        esp_gps->parent.longitude = parse_lat_long(esp_gps);
        break;
    case 5: // Kierunek E (wschod) / W (zachod)
        if (esp_gps->item_str[0] == 'W' || esp_gps->item_str[0] == 'w') {
            esp_gps->parent.longitude *= -1;
        }
        break;
    case 6: // Status FIX (0 = brak, 1 = GPS, 2 = DGPS)
        esp_gps->parent.fix_status = (uint8_t)strtol(esp_gps->item_str, NULL, 10);
        break;
    case 7: // Liczba satelitow aktualnie uzywanych do kalkulacji pozycji
        esp_gps->parent.sats_in_use = (uint8_t)strtol(esp_gps->item_str, NULL, 10);
        break;
    default:
        break;
    }
}
#endif

#if CONFIG_NMEA_STATEMENT_RMC
// Parsowanie ramki $XXRMC (Zalecane minimum danych - backup pozycji)
static void parse_rmc(esp_gps_t *esp_gps)
{
    switch (esp_gps->item_num) {
    case 2: // Status poprawnosci danych: 'A' = Aktywne (Ok), 'V' = Warning (Brak pozycji)
        esp_gps->parent.valid = (esp_gps->item_str[0] == 'A');
        break;
    case 3: // Szerokosc geograficzna
        esp_gps->parent.latitude = parse_lat_long(esp_gps);
        break;
    case 4: // Kierunek N/S
        if (esp_gps->item_str[0] == 'S' || esp_gps->item_str[0] == 's') {
            esp_gps->parent.latitude *= -1;
        }
        break;
    case 5: // Dlugosc geograficzna
        esp_gps->parent.longitude = parse_lat_long(esp_gps);
        break;
    case 6: // Kierunek E/W
        if (esp_gps->item_str[0] == 'W' || esp_gps->item_str[0] == 'w') {
            esp_gps->parent.longitude *= -1;
        }
        break;
    default:
        break;
    }
}
#endif

#if CONFIG_NMEA_STATEMENT_GLL
// Parsowanie ramki $XXGLL (Geograficzne polozenie - opcjonalny backup)
static void parse_gll(esp_gps_t *esp_gps)
{
    switch (esp_gps->item_num) {
    case 1: // Szerokosc geograficzna
        esp_gps->parent.latitude = parse_lat_long(esp_gps);
        break;
    case 2: // Kierunek N/S
        if (esp_gps->item_str[0] == 'S' || esp_gps->item_str[0] == 's') {
            esp_gps->parent.latitude *= -1;
        }
        break;
    case 3: // Dlugosc geograficzna
        esp_gps->parent.longitude = parse_lat_long(esp_gps);
        break;
    case 4: // Kierunek E/W
        if (esp_gps->item_str[0] == 'W' || esp_gps->item_str[0] == 'w') {
            esp_gps->parent.longitude *= -1;
        }
        break;
    case 6: // Status waznosci pomiaru (A/V)
        esp_gps->parent.valid = (esp_gps->item_str[0] == 'A');
        break;
    default:
        break;
    }
}
#endif

// Analizuje wyodrebniony token. Pierwszy token (item_num == 0) identyfikuje typ ramki.
static esp_err_t parse_item(esp_gps_t *esp_gps)
{
    // Identyfikacja naglowka ramki NMEA
    if (esp_gps->item_num == 0 && esp_gps->item_str[0] == '$') {
#if CONFIG_NMEA_STATEMENT_GGA
        if (strstr(esp_gps->item_str, "GGA")) esp_gps->cur_statement = STATEMENT_GGA;
#endif
#if CONFIG_NMEA_STATEMENT_RMC
        else if (strstr(esp_gps->item_str, "RMC")) esp_gps->cur_statement = STATEMENT_RMC;
#endif
#if CONFIG_NMEA_STATEMENT_GLL
        else if (strstr(esp_gps->item_str, "GLL")) esp_gps->cur_statement = STATEMENT_GLL;
#endif
        else esp_gps->cur_statement = STATEMENT_UNKNOWN;
        return ESP_OK;
    }

    // Jesli typ ramki nie jest wspierany, pomin parsowanie jej kolejnych pol
    if (esp_gps->cur_statement == STATEMENT_UNKNOWN) return ESP_OK;

    // Przekierowanie danych pola do odpowiedniego parsera szczegolowego
#if CONFIG_NMEA_STATEMENT_GGA
    if (esp_gps->cur_statement == STATEMENT_GGA) parse_gga(esp_gps);
#endif
#if CONFIG_NMEA_STATEMENT_RMC
    else if (esp_gps->cur_statement == STATEMENT_RMC) parse_rmc(esp_gps);
#endif
#if CONFIG_NMEA_STATEMENT_GLL
    else if (esp_gps->cur_statement == STATEMENT_GLL) parse_gll(esp_gps);
#endif
    return ESP_OK;
}

// Maszyna stanow dekodujaca strumien bajtow NMEA. Wylicza sume kontrolna XOR.
static esp_err_t gps_decode(esp_gps_t *esp_gps, size_t len)
{
    const uint8_t *d = esp_gps->buffer;
    while (*d) {
        if (*d == '$') { // Poczatek nowej ramki NMEA - reset stanow roboczych
            esp_gps->asterisk = 0;
            esp_gps->item_num = 0;
            esp_gps->item_pos = 0;
            esp_gps->cur_statement = 0;
            esp_gps->crc = 0;
            esp_gps->item_str[esp_gps->item_pos++] = *d;
            esp_gps->item_str[esp_gps->item_pos] = '\0';
        }
        else if (*d == ',') { // Separator pol - parsuj zebrany string i przejdz do nastepnego pola
            parse_item(esp_gps);
            esp_gps->crc ^= (uint8_t)(*d); // Przecinek wchodzi w sklad sumy kontrolnej
            esp_gps->item_pos = 0;
            esp_gps->item_str[0] = '\0';
            esp_gps->item_num++;
        }
        else if (*d == '*') { // Koniec danych tekstowych, zaraz pojawia sie bajty sumy kontrolnej
            parse_item(esp_gps);
            esp_gps->asterisk = 1;
            esp_gps->item_pos = 0;
            esp_gps->item_str[0] = '\0';
            esp_gps->item_num++;
        }
        else if (*d == '\r') { // Koniec linii - weryfikacja sumy kontrolnej ramki
            uint8_t crc = (uint8_t)strtol(esp_gps->item_str, NULL, 16);
            if (esp_gps->crc == crc) { // Suma kontrolna prawidlowa
                switch (esp_gps->cur_statement) {
                case STATEMENT_GGA: esp_gps->parsed_statement |= 1 << STATEMENT_GGA; break;
                case STATEMENT_RMC: esp_gps->parsed_statement |= 1 << STATEMENT_RMC; break;
                case STATEMENT_GLL: esp_gps->parsed_statement |= 1 << STATEMENT_GLL; break;
                default: break;
                }
                
                // Jesli zebrano komplet oczekiwanych ramek, wyslij event z danymi
                if (((esp_gps->parsed_statement) & esp_gps->all_statements) == esp_gps->all_statements) {
                    esp_gps->parsed_statement = 0;
                    esp_event_post_to(esp_gps->event_loop_hdl, ESP_NMEA_EVENT, GPS_UPDATE,
                                      &(esp_gps->parent), sizeof(gps_t), 100 / portTICK_PERIOD_MS);
                }
            }
        }
        else { // Agregacja zwyklych znakow tekstowych pola do bufora podrecznego
            if (!(esp_gps->asterisk)) esp_gps->crc ^= (uint8_t)(*d); // Znaki sumy kontrolnej nie sa czescia obliczen CRC
            if (esp_gps->item_pos < NMEA_MAX_STATEMENT_ITEM_LENGTH - 1) {
                esp_gps->item_str[esp_gps->item_pos++] = *d;
                esp_gps->item_str[esp_gps->item_pos] = '\0';
            }
        }
        d++;
    }
    return ESP_OK;
}

// Wywolywana w momencie wykrycia znaku konca linii ('\n') przez mechanizm Pattern Detect w UART
static void esp_handle_uart_pattern(esp_gps_t *esp_gps)
{
    int pos = uart_pattern_pop_pos(esp_gps->uart_port);
    if (pos != -1) {
        // Odczyt pelnej linii tekstu (wraz ze znakiem konca linii)
        int read_len = uart_read_bytes(esp_gps->uart_port, esp_gps->buffer, pos + 1, 100 / portTICK_PERIOD_MS);
        esp_gps->buffer[read_len] = '\0'; // Zabezpieczenie stringu znakiem null-terminatora
        gps_decode(esp_gps, read_len + 1);
    } else {
        uart_flush_input(esp_gps->uart_port);
    }
}

// Glowny Task FreeRTOS - nasluchuje zdarzen sprzetowych z UART i zarzadza petla parsera
static void nmea_parser_task_entry(void *arg)
{
    esp_gps_t *esp_gps = (esp_gps_t *)arg;
    uart_event_t event;
    while (1) {
        // Oczekiwanie na zdarzenia ze sterownika UART (np. przepelnienie bufora, wykrycie znaku nowej linii)
        if (xQueueReceive(esp_gps->event_queue, &event, pdMS_TO_TICKS(200))) {
            if (event.type == UART_PATTERN_DET) {
                esp_handle_uart_pattern(esp_gps);
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                uart_flush(esp_gps->uart_port);
                xQueueReset(esp_gps->event_queue);
            }
        }
        // Uruchomienie petli przetwarzania zdarzen ESP Event Loop zwiazanych z GPS
        esp_event_loop_run(esp_gps->event_loop_hdl, pdMS_TO_TICKS(50));
    }
    vTaskDelete(NULL);
}

// Funkcja inicjalizacyjna podsystemu GPS. Alokuje pamiec, ustawia UART oraz tworzy Task FreeRTOS.
nmea_parser_handle_t nmea_parser_init(const nmea_parser_config_t *config)
{
    esp_gps_t *esp_gps = calloc(1, sizeof(esp_gps_t));
    if (!esp_gps) return NULL;

    esp_gps->buffer = calloc(1, NMEA_PARSER_RUNTIME_BUFFER_SIZE);
    if (!esp_gps->buffer) { free(esp_gps); return NULL; }

    // Ustawienie oczekiwanych typow ramek, ktore musza splynac do pelnego uaktualnienia pozycji
#if CONFIG_NMEA_STATEMENT_GGA
    esp_gps->all_statements |= (1 << STATEMENT_GGA);
#endif
#if CONFIG_NMEA_STATEMENT_RMC
    esp_gps->all_statements |= (1 << STATEMENT_RMC);
#endif
#if CONFIG_NMEA_STATEMENT_GLL
    esp_gps->all_statements |= (1 << STATEMENT_GLL);
#endif

    esp_gps->uart_port = config->uart.uart_port;

    // Specyfikacja niskopoziomowej konfiguracji magistrali UART
    uart_config_t uart_config = {
        .baud_rate = config->uart.baud_rate,
        .data_bits = config->uart.data_bits,
        .parity = config->uart.parity,
        .stop_bits = config->uart.stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Instalacja drajwera UART oraz przypisanie pinow GPIO
    if (uart_driver_install(esp_gps->uart_port, CONFIG_NMEA_PARSER_RING_BUFFER_SIZE, 0,
                            config->uart.event_queue_size, &esp_gps->event_queue, 0) != ESP_OK) {
        free(esp_gps->buffer); free(esp_gps); return NULL;
    }
    uart_param_config(esp_gps->uart_port, &uart_config);
    uart_set_pin(esp_gps->uart_port, UART_PIN_NO_CHANGE, config->uart.rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
    // Konfiguracja przerwania sprzetowego wykrywajacego znak konca linii '\n' (Pattern Detect)
    uart_enable_pattern_det_baud_intr(esp_gps->uart_port, '\n', 1, 9, 0, 0);
    uart_pattern_queue_reset(esp_gps->uart_port, config->uart.event_queue_size);
    uart_flush(esp_gps->uart_port);

    // Utworzenie lokalnej petli zdarzen dla asynchronicznego powiadamiania aplikacji o odczytach GPS
    esp_event_loop_args_t loop_args = { .queue_size = NMEA_EVENT_LOOP_QUEUE_SIZE, .task_name = NULL };
    esp_event_loop_create(&loop_args, &esp_gps->event_loop_hdl);

    // Powolanie dedykowanego zadania przetwarzajacego ramki NMEA w tle
    xTaskCreate(nmea_parser_task_entry, "nmea_parser", CONFIG_NMEA_PARSER_TASK_STACK_SIZE, esp_gps, CONFIG_NMEA_PARSER_TASK_PRIORITY, &esp_gps->tsk_hdl);
    
    ESP_LOGI(GPS_TAG, "Zoptymalizowany NMEA Parser gotowy");
    return esp_gps;
}

// Bezpieczne zwalnianie zasobow i usuwanie tasku parsera GPS
esp_err_t nmea_parser_deinit(nmea_parser_handle_t nmea_hdl)
{
    esp_gps_t *esp_gps = (esp_gps_t *)nmea_hdl;
    vTaskDelete(esp_gps->tsk_hdl);
    esp_event_loop_delete(esp_gps->event_loop_hdl);
    uart_driver_delete(esp_gps->uart_port);
    free(esp_gps->buffer);
    free(esp_gps);
    return ESP_OK;
}

// Rejestracja handlera uzytkownika w petli zdarzen parsera GPS
esp_err_t nmea_parser_add_handler(nmea_parser_handle_t nmea_hdl, esp_event_handler_t event_handler, void *handler_args)
{
    esp_gps_t *esp_gps = (esp_gps_t *)nmea_hdl;
    return esp_event_handler_register_with(esp_gps->event_loop_hdl, ESP_NMEA_EVENT, ESP_EVENT_ANY_ID, event_handler, handler_args);
}

// Glowny handler zdarzen GPS. Przepisuje sparsowane dane strukturalne do zmiennych globalnych aplikacji.
void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == GPS_UPDATE) {
        gps_t *gps = (gps_t *)event_data;
        
        gps_sats = gps->sats_in_use;
        gps_fix  = (gps->fix_status > 0); 
        gps_lat  = gps->latitude;
        gps_lon  = gps->longitude;

        ESP_LOGD(GPS_TAG, "Satelity: %d | Fix: %s | Lat: %.05f | Lon: %.05f", 
                 gps_sats, gps_fix ? "TAK" : "BRAK", gps_lat, gps_lon);
    }
}