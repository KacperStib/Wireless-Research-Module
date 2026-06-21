#include "include/shell_mng.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "dev_config.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_console.h"

// ─────────────────────────────────────────────────────────────────────────────
// Handler komendy: log <poziom> [tag]
// ─────────────────────────────────────────────────────────────────────────────
static int cmd_log(int argc, char **argv)
{
    if (argc < 2) 
    {
        printf("Uzycie: log <poziom> [tag]\n");
        printf("Poziomy: none  err  wrn  inf  dbg\n");
        printf("Tag:     opcjonalny, domyslnie * (wszystkie)\n");
        return 1;
    }

    esp_log_level_t level;

    if      (strcmp(argv[1], "none") == 0) level = ESP_LOG_NONE;
    else if (strcmp(argv[1], "err")  == 0) level = ESP_LOG_ERROR;
    else if (strcmp(argv[1], "wrn")  == 0) level = ESP_LOG_WARN;
    else if (strcmp(argv[1], "inf")  == 0) level = ESP_LOG_INFO;
    else if (strcmp(argv[1], "dbg")  == 0) level = ESP_LOG_DEBUG;
    else 
    {
        printf("Nieznany poziom: '%s'\n", argv[1]);
        printf("Dostepne: none  err  wrn  inf  dbg\n");
        return 1;
    }

    // Jeśli podano tag — użyj go, w przeciwnym razie "*" (wszystkie tagi)
    const char *tag = (argc >= 3) ? argv[2] : "*";

    esp_log_level_set(tag, level);
    printf("Log [%s] ustawiony na: %s\n", tag, argv[1]);
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler komendy: radio [lora|espnow]
// ─────────────────────────────────────────────────────────────────────────────
static int cmd_radio(int argc, char **argv)
{
    if (argc < 2) 
    {
        printf("Technologia: %s\n", radio_cfg.tech == RADIO_TECH_LORA ? "lora" : "espnow");
        return 0;
    }

    if (strcmp(argv[1], "lora") == 0) 
    {
        radio_cfg.tech = RADIO_TECH_LORA;
        printf("Technologia: LORA\n");
    } 
    else if (strcmp(argv[1], "espnow") == 0) 
    {
        radio_cfg.tech = RADIO_TECH_ESPNOW;
        printf("Technologia: ESP-NOW\n");
    } 
    else 
    {
        printf("Nieznana: '%s'  (dostepne: lora  espnow)\n", argv[1]);
        return 1;
    }
    
    radio_apply_config();
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler komendy: dir [rx|tx]
// ─────────────────────────────────────────────────────────────────────────────
static int cmd_dir(int argc, char **argv)
{
    if (argc < 2) 
    {
        printf("Kierunek: %s\n", radio_cfg.dir == RADIO_DIR_TX ? "tx" : "rx");
        return 0;
    }

    if (strcmp(argv[1], "tx") == 0) 
    {
        radio_cfg.dir = RADIO_DIR_TX;
        radio_apply_config();
        printf("Kierunek: TX\n");
    } 
    else if (strcmp(argv[1], "rx") == 0) 
    {
        radio_cfg.dir = RADIO_DIR_RX;
        radio_apply_config();
        printf("Kierunek: RX\n");
    } 
    else 
    {
        printf("Nieznany: '%s'  (dostepne: rx  tx)\n", argv[1]);
        return 1;
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler komendy: mac [XX:XX:XX:XX:XX:XX]
// ─────────────────────────────────────────────────────────────────────────────
static int cmd_mac(int argc, char **argv)
{
    if (argc < 2) 
    {
        printf("MAC: " MACSTR "\n", MAC2STR(radio_cfg.peer_mac));
        return 0;
    }

    // Parsowanie adresu MAC ze stringu
    uint8_t mac[6];
    int n = sscanf(argv[1], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &mac[0], &mac[1], &mac[2],
                   &mac[3], &mac[4], &mac[5]);
    if (n != 6) 
    {
        printf("Zly format. Uzycie: mac XX:XX:XX:XX:XX:XX\n");
        return 1;
    }

    memcpy((void*)radio_cfg.peer_mac, mac, 6);
    
    if (radio_cfg.tech == RADIO_TECH_ESPNOW)
    {
        espnow_set_peer(mac);
    }
    	
    printf("MAC ustawiony: " MACSTR "\n", MAC2STR(mac));
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler komendy: mymac
// ─────────────────────────────────────────────────────────────────────────────
static int cmd_mymac(int argc, char **argv)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    printf("My MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler komendy: config <show|save|load>
// ─────────────────────────────────────────────────────────────────────────────
// Funkcja pomocnicza do wyswietlenia testu
const char* test_to_str(test_scenario_t test) {
	switch (test) {
	    case TEST_IDLE:    return "idle";
	    case TEST_GENERAL: return "general";
	    case TEST_POWER:   return "power";
	    case TEST_SPEED:   return "speed";
	    case TEST_PER:     return "per";
	    case TEST_RTT:     return "rtt";
	    default:           return "unknown";
	}
}
static int cmd_config(int argc, char **argv)
{
    if (argc < 2) 
    {
        printf("Uzycie: config <show|save|load>\n");
        return 1;
    }

    // Wyświetlanie bieżącej konfiguracji
    if (strcmp(argv[1], "show") == 0) 
    {
        printf("=== Konfiguracja ===\n");
        printf("  tech:     %s\n", radio_cfg.tech == RADIO_TECH_LORA ? "lora" : "espnow");
        printf("  dir:      %s\n", radio_cfg.dir == RADIO_DIR_TX ? "tx" : "rx");
        printf("  peer_mac: " MACSTR "\n", MAC2STR(radio_cfg.peer_mac));
        printf("  test:     %s\n", test_to_str(radio_cfg.test));
        return 0;
    }

    // Zapis do pamięci nieulotnej NVS
    if (strcmp(argv[1], "save") == 0) 
    {
        radio_config_t snap;
        memcpy(&snap, (const void *)&radio_cfg, sizeof(snap));

        if (nvs_cfg_save(&snap) == ESP_OK)
        {
            printf("Config zapisany do NVS.\n");
        }
        else
        {
            printf("Blad zapisu do NVS!\n");
        }
        return 0;
    }

    // Odczyt z pamięci nieulotnej NVS
    if (strcmp(argv[1], "load") == 0) 
    {
        radio_config_t tmp = {0};
        if (nvs_cfg_load(&tmp) != ESP_OK) 
        {
            printf("Blad odczytu z NVS (brak zapisu lub blad flash).\n");
            return 1;
        }
        memcpy((void *)&radio_cfg, &tmp, sizeof(radio_config_t));
        radio_apply_config();

        printf("Config zaladowany z NVS.\n");
        printf("  tech:     %s\n", radio_cfg.tech == RADIO_TECH_LORA ? "lora" : "espnow");
        printf("  dir:      %s\n", radio_cfg.dir == RADIO_DIR_TX ? "tx" : "rx");
        printf("  peer_mac: " MACSTR "\n", MAC2STR(radio_cfg.peer_mac));
        printf("  test:     %s\n", test_to_str(radio_cfg.test));
        return 0;
    }

    printf("Nieznana akcja: '%s'  (dostepne: show  save  load)\n", argv[1]);
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler komendy: reset
// ─────────────────────────────────────────────────────────────────────────────
static int cmd_reset(int argc, char **argv)
{
    printf("Restartuje...\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler komendy: lora [sf|bw|cr|pwr] [wartosc]
// ─────────────────────────────────────────────────────────────────────────────
static int cmd_lora(int argc, char **argv)
{
    if (argc < 3) 
    {
        printf("=== Konfiguracja LoRa ===\n");
        printf("  sf:  %d  (7-12)\n",  radio_cfg.lora.sf);
        printf("  bw:  %d  (0-9)\n",   radio_cfg.lora.bw);
        printf("  cr:  %d  (1-4)\n",   radio_cfg.lora.cr);
        printf("  pwr: %d  (2-17)\n",  radio_cfg.lora.pwr);
        printf("Uzycie: lora <sf|bw|cr|pwr> <wartosc>\n");
        return 0;
    }

    int val = atoi(argv[2]);

    if (strcmp(argv[1], "sf") == 0) 
    {
        if (val < 7 || val > 12) { printf("SF: 7-12\n"); return 1; }
        radio_cfg.lora.sf = val;
    }
    else if (strcmp(argv[1], "bw") == 0) 
    {
        if (val < 0 || val > 9)  { printf("BW: 0-9\n");  return 1; }
        radio_cfg.lora.bw = val;
    }
    else if (strcmp(argv[1], "cr") == 0) 
    {
        if (val < 1 || val > 4)  { printf("CR: 1-4\n");  return 1; }
        radio_cfg.lora.cr = val;
    }
    else if (strcmp(argv[1], "pwr") == 0) 
    {
        if (val < 2 || val > 17) { printf("PWR: 2-17 dBm\n"); return 1; }
        radio_cfg.lora.pwr = val;
    }
    else 
    {
        printf("Nieznany parametr: '%s'\n", argv[1]);
        return 1;
    }

    radio_apply_config();
    printf("OK\n");
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler komendy: espnow [mode|pwr] [wartosc]
// ─────────────────────────────────────────────────────────────────────────────
static int cmd_espnow(int argc, char **argv)
{
    if (argc < 3) 
    {
        printf("=== Konfiguracja ESP-NOW ===\n");
        printf("  mode: %s  (0: Normal, 1: Long Range)\n", radio_cfg.espnow.lr_mode ? "Long Range" :"Normal");
        printf("  pwr:  %d  (0-80)\n", radio_cfg.espnow.pwr);
        printf("Uzycie: espnow <mode|pwr> <wartosc>\n");
        return 0;
    }

    int val = atoi(argv[2]);

    if (strcmp(argv[1], "mode") == 0) 
    {
        // Zakładam, że lr_mode to bool, więc akceptujemy 0 lub 1
        if (val < 0 || val > 1) { printf("Tryb: 0 (Long Range), 1 (Normal)\n"); return 1; }
        radio_cfg.espnow.lr_mode = (bool)val;
    }
    else if (strcmp(argv[1], "pwr") == 0) 
    {
        // Walidacja dla pwr 0-80
        if (val < 0 || val > 80) { printf("Moc (pwr): 0-80\n"); return 1; }
        radio_cfg.espnow.pwr = (uint8_t)val;
    }
    else 
    {
        printf("Nieznany parametr: '%s'\n", argv[1]);
        return 1;
    }

    // Aplikacja zmian
    radio_apply_config();
    printf("OK\n");
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler komendy: test[idle|general|power|per] 
// ─────────────────────────────────────────────────────────────────────────────
static int cmd_test(int argc, char **argv)
{
    if (argc < 2) 
    {
        printf("Aktualny test: %s\n", test_to_str(radio_cfg.test));
        printf("Dostepne: idle, general, power, per\n");
        return 0;
    }

    if      (strcmp(argv[1], "idle")    == 0) radio_cfg.test = TEST_IDLE;
    else if (strcmp(argv[1], "general") == 0) radio_cfg.test = TEST_GENERAL;
    else if (strcmp(argv[1], "power")   == 0) radio_cfg.test = TEST_POWER;
    else if (strcmp(argv[1], "speed")   == 0) radio_cfg.test = TEST_SPEED;
    else if (strcmp(argv[1], "per")     == 0) radio_cfg.test = TEST_PER;
    else if (strcmp(argv[1], "rtt")     == 0) radio_cfg.test = TEST_RTT;
    else 
    {
        printf("Nieznany test: '%s'\n", argv[1]);
        return 1;
    }

    printf("Test ustawiony na: %s\n", argv[1]);
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler komendy: tasks
// ─────────────────────────────────────────────────────────────────────────────
static int cmd_tasks(int argc, char **argv)
{
    char *buf = malloc(1024);
    if (!buf) 
    {
        printf("Brak pamieci\n");
        return 1;
    }
    vTaskList(buf);
    printf("%-20s %5s %5s %8s %3s\n", "Nazwa", "Stan", "Prio", "StkMin", "Nr");
    printf("----------------------------------------------------\n");
    printf("%s", buf);
    free(buf);
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Glowna inicjalizacja konsoli
// ─────────────────────────────────────────────────────────────────────────────
void shell_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "esp> ";
    repl_cfg.max_cmdline_length = 64;

    // Domyslna konfiguracja dla wbudowanego mostka USB-Serial-JTAG
    esp_console_dev_usb_serial_jtag_config_t usb_cfg = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usb_cfg, &repl_cfg, &repl));

    // Rejestracja grup polecen
    console_log_register();
    console_radio_register();

    // Start interaktywnej konsoli
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

// ─────────────────────────────────────────────────────────────────────────────
// Rejestracja komend w silniku esp_console
// ─────────────────────────────────────────────────────────────────────────────
void console_log_register(void)
{
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "log",
        .help    = "Ustaw poziom logow: log <none|err|wrn|inf|dbg> [tag]",
        .hint    = "<none|err|wrn|inf|dbg> [tag]",
        .func    = cmd_log,
    }));
}

void console_radio_register(void)
{
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "radio",
        .help    = "Ustaw technologie: radio <lora|espnow>",
        .hint    = "<lora|espnow>",
        .func    = cmd_radio,
    }));

    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "dir",
        .help    = "Ustaw kierunek: dir <rx|tx>",
        .hint    = "<rx|tx>",
        .func    = cmd_dir,
    }));
    
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
	    .command = "mac",
	    .help    = "Ustaw MAC peer ESP-NOW: mac XX:XX:XX:XX:XX:XX",
	    .hint    = "XX:XX:XX:XX:XX:XX",
	    .func    = cmd_mac,
	}));
	
	ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
	    .command = "mymac",
	    .help    = "Pokaz MAC tego urzadzenia",
	    .func    = cmd_mymac,
	}));
	
	ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
	    .command = "config",
	    .help    = "Zarzadzaj configiem: config <show|save|load>",
	    .hint    = "<show|save|load>",
	    .func    = cmd_config,
	}));
	
	ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
	    .command = "reset",
	    .help    = "Zresetuj urzadzenie",
	    .func    = cmd_reset,
	}));
	
	ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
	    .command = "lora",
	    .help    = "Parametry LoRa: lora <sf|bw|cr|pwr> <wartosc>",
	    .hint    = "<sf|bw|cr|pwr> <wartosc>",
	    .func    = cmd_lora,
	}));
	
	ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
	    .command = "espnow",
	    .help    = "Parametry ESP-NOW: espnow <mode|pwr> <wartosc>",
	    .hint    = "<mode|pwr> <wartosc>",
	    .func    = cmd_espnow,
	}));

	ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
	    .command = "tasks",
	    .help    = "Lista taskow FreeRTOS z watermarkiem stosu",
	    .func    = cmd_tasks,
	}));
	
	ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "test",
        .help    = "Ustaw scenariusz testowy: test <idle|general|power|per>",
        .hint    = "<idle|general|power|per>",
        .func    = cmd_test,
    }));
}