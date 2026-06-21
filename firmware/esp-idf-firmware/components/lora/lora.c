#include <stdio.h>
#include "include/lora.h"

#define TAG "LORA"

// Zmienne globalne modulu
static spi_device_handle_t _spi;       // Uchwyt magistrali SPI ESP-IDF
static int _implicit;                  // Tryb naglowka (0 - explicit, 1 - implicit)
static long _frequency;                // Czestotliwosc pracy w Hz
static int _send_packet_lost = 0;      // Licznik nieudanych transmisji
static int _sbw = 0;                   // Indeks szerokosci pasma 

volatile bool lora_sent = false;
volatile bool rx_done = false;

static void IRAM_ATTR dio0_isr_handler(void* arg) {
	if(radio_cfg.dir == RADIO_DIR_TX)
    	lora_sent = true;
    else if(radio_cfg.dir == RADIO_DIR_RX)
    	rx_done = true;
}

static void setup_lora_interrupt() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LORA_IO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE // Reagujemy na stan wysoki
    };
    gpio_config(&io_conf);
    
    // Włącz obsługę ISR w systemie
    gpio_install_isr_service(0);
    gpio_isr_handler_add(LORA_IO, dio0_isr_handler, NULL);
}

// Zapisz pojedynczy bajt do rejestru SX127x
void 
lora_write_reg(int reg, int val)
{
   // Bit 7 adresu na 1 = zapis SPI
   uint8_t out[2] = { 0x80 | reg, val };
   spi_transaction_t t = {
      .flags = 0,
      .length = 8 * sizeof(out),
      .tx_buffer = out,
      .rx_buffer = NULL
   };
   spi_device_transmit(_spi, &t);
}

// Zapisz bufor danych do rejestru 
void
lora_write_reg_buffer(int reg, uint8_t *val, int len)
{
   uint8_t local_out[256];
   if (len > 255) len = 255;
   
   local_out[0] = 0x80 | reg; 
   for (int i = 0; i < len; i++) {
      local_out[i + 1] = val[i];
   }

   spi_transaction_t t = {
      .flags = 0,
      .length = 8 * (len + 1),
      .tx_buffer = local_out,
      .rx_buffer = NULL
   };
   spi_device_transmit(_spi, &t);
}

// Odczytaj bajt z rejestru SX127x
int
lora_read_reg(int reg)
{
   // Bit 7 adresu na 0 = odczyt SPI
   uint8_t out[2] = { reg, 0xff };
   uint8_t in[2];

   spi_transaction_t t = {
      .flags = 0,
      .length = 8 * sizeof(out),
      .tx_buffer = out,
      .rx_buffer = in
   };
   spi_device_transmit(_spi, &t);
   return in[1]; // Zwracamy odebrany bajt danych
}

// Odczytaj bufor danych z rejestru
void
lora_read_reg_buffer(int reg, uint8_t *val, int len)
{
   uint8_t local_out[256];
   uint8_t local_in[256];
   if (len > 255) len = 255;

   local_out[0] = reg; // Odczyt SPI
   for (int i = 0; i < len; i++) {
      local_out[i + 1] = 0xff; // Taktowanie linii MISO
   }

   spi_transaction_t t = {
      .flags = 0,
      .length = 8 * (len + 1),
      .tx_buffer = local_out,
      .rx_buffer = local_in
   };

   spi_device_transmit(_spi, &t);
   for (int i = 0; i < len; i++) {
      val[i] = local_in[i + 1];
   }
}

// Reset sprzetowy modulu radiowego przez pin RST
void 
lora_reset(void)
{
   gpio_set_level(LORA_RST, 0);       // Stan niski resetuje uklad
   vTaskDelay(pdMS_TO_TICKS(1));      
   gpio_set_level(LORA_RST, 1);       // Stan wysoki - praca normalna
   vTaskDelay(pdMS_TO_TICKS(10));     // Czas na start oscylatora
}

// Wlacz jawny tryb naglowka 
void 
lora_explicit_header_mode(void)
{
   _implicit = 0;
   lora_write_reg(REG_MODEM_CONFIG_1, lora_read_reg(REG_MODEM_CONFIG_1) & 0xfe);
}

// Wlacz niejawny tryb naglowka 
void 
lora_implicit_header_mode(int size)
{
   _implicit = 1;
   lora_write_reg(REG_MODEM_CONFIG_1, lora_read_reg(REG_MODEM_CONFIG_1) | 0x01);
   lora_write_reg(REG_PAYLOAD_LENGTH, size);
}

// Tryb Standby 
void 
lora_idle(void)
{
   lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

// Tryb glebokiego uspienia 
void 
lora_sleep(void)
{ 
   lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

// Tryb ciaglego nasluchu odbiornika
void 
lora_receive(void)
{
   lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
}

// Ustaw moc nadajnika
void 
lora_set_tx_power(int level)
{
   if (level < 2) level = 2;
   else if (level > 17) level = 17;
   lora_write_reg(REG_PA_CONFIG, PA_BOOST | (level - 2));
}

// Ustaw czestotliwosc pracy
void 
lora_set_frequency(long frequency)
{
   _frequency = frequency;
   uint64_t frf = ((uint64_t)frequency << 19) / 32000000;
   lora_write_reg(REG_FRF_MSB, (uint8_t)(frf >> 16));
   lora_write_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
   lora_write_reg(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

// Ustaw wspolczynnik rozproszenia Spreading Factor 
void 
lora_set_spreading_factor(int sf)
{
   if (sf < 6) sf = 6;
   else if (sf > 12) sf = 12;

   // Specjalna optymalizacja rejestrow dla SF=6 wymagana przez Semtech
   if (sf == 6) {
      lora_write_reg(REG_DETECTION_OPTIMIZE, 0xc5);
      lora_write_reg(REG_DETECTION_THRESHOLD, 0x0c);
   } else {
      lora_write_reg(REG_DETECTION_OPTIMIZE, 0xc3);
      lora_write_reg(REG_DETECTION_THRESHOLD, 0x0a);
   }
   lora_write_reg(REG_MODEM_CONFIG_2, (lora_read_reg(REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
}

// Mapowanie zachowania pinow przerwan DIO
void 
lora_set_dio_mapping(int dio, int mode)
{
   if (dio < 4) {
      int _mode = lora_read_reg(REG_DIO_MAPPING_1);
      if (dio == 0) _mode = (_mode & 0x3F) | (mode << 6);
      else if (dio == 1) _mode = (_mode & 0xCF) | (mode << 4);
      else if (dio == 2) _mode = (_mode & 0xF3) | (mode << 2);
      else if (dio == 3) _mode = (_mode & 0xFC) | mode;
      lora_write_reg(REG_DIO_MAPPING_1, _mode);
   } else if (dio < 6) {
      int _mode = lora_read_reg(REG_DIO_MAPPING_2);
      if (dio == 4) _mode = (_mode & 0x3F) | (mode << 6);
      else if (dio == 5) _mode = (_mode & 0xCF) | (mode << 4);
      lora_write_reg(REG_DIO_MAPPING_2, _mode);
   }
}

// Ustaw szerokosc pasma 
void 
lora_set_bandwidth(int sbw)
{
   if (sbw < 10) {
      lora_write_reg(REG_MODEM_CONFIG_1, (lora_read_reg(REG_MODEM_CONFIG_1) & 0x0f) | (sbw << 4));
      _sbw = sbw;
   }
}

// Ustaw stopien kodowania korekcyjnego 
void 
lora_set_coding_rate(int cr)
{
   if (cr < 1) cr = 1;
   else if (cr > 4) cr = 4;
   lora_write_reg(REG_MODEM_CONFIG_1, (lora_read_reg(REG_MODEM_CONFIG_1) & 0xf1) | (cr << 1));
}

// Ustaw dlugosc preambuly pakietu
void 
lora_set_preamble_length(long length)
{
   lora_write_reg(REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
   lora_write_reg(REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
}

// Ustaw slowo synchronizacyjne 
void 
lora_set_sync_word(int sw)
{
   lora_write_reg(REG_SYNC_WORD, sw);
}

// Wlacz sprawdzanie sumy kontrolnej CRC dla odbiornika
void 
lora_enable_crc(void)
{
   lora_write_reg(REG_MODEM_CONFIG_2, lora_read_reg(REG_MODEM_CONFIG_2) | 0x04);
}

// Wylacz sprawdzanie sumy kontrolnej CRC
void 
lora_disable_crc(void)
{
   lora_write_reg(REG_MODEM_CONFIG_2, lora_read_reg(REG_MODEM_CONFIG_2) & 0xfb);
}

// Inicjalizacja GPIO, rejestracja SPI i podstawowa konfiguracja startowa radia
int 
lora_init(void)
{
   esp_err_t ret;
   
   // Dodano oblusge wlaczenia zasilania przez mosfet
   gpio_reset_pin(LORA_PWR);
   gpio_set_direction(LORA_PWR, GPIO_MODE_OUTPUT);
   gpio_set_level(LORA_PWR, 0);

   gpio_reset_pin(LORA_RST);
   gpio_set_direction(LORA_RST, GPIO_MODE_OUTPUT);
   
   gpio_reset_pin(LORA_CS);
   gpio_set_direction(LORA_CS, GPIO_MODE_OUTPUT);
   gpio_set_level(LORA_CS, 1);

   // Konfiguracja interfejsu SPI w ESP-IDF
   spi_device_interface_config_t dev = {
      .clock_speed_hz = 9000000,      // Czestotliwosc SPI (9 MHz)
      .mode = 0,                      // Tryb SPI 0
      .spics_io_num = LORA_CS,        // Sprzetowy CS
      .queue_size = 7,
      .flags = 0,
      .pre_cb = NULL
   };
   
   ret = spi_bus_add_device(SPI2_HOST, &dev, &_spi);
   if (ret != ESP_OK) return 0;

   lora_reset();

   // Sprawdzenie komunikacji 
   uint8_t version;
   uint8_t i = 0;
   while(i++ < TIMEOUT_RESET) {
      version = lora_read_reg(REG_VERSION);
      if(version == 0x12) break;
      vTaskDelay(pdMS_TO_TICKS(2));
   }
   if (i == TIMEOUT_RESET + 1) return 0;

   // Bazowa konfiguracja rejestrow po starcie
   lora_sleep();
   lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0);  
   lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0);  
   lora_write_reg(REG_LNA, lora_read_reg(REG_LNA) | 0x03); // Maksymalne wzmocnienie LNA
   lora_write_reg(REG_MODEM_CONFIG_3, 0x04);  // Wlacz AGC
   //lora_set_tx_power(17);                      
   lora_idle();                               
   
   setup_lora_interrupt();
   
   return 1;
}

// Wyslij pakiet danych 
int
lora_send_packet(uint8_t *buf, int size)
{
   lora_idle();
   lora_write_reg(REG_FIFO_ADDR_PTR, 0);       
   lora_write_reg_buffer(REG_FIFO, buf, size); 
   lora_write_reg(REG_PAYLOAD_LENGTH, size);   
   
   // Start nadawania radiowego
   lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

   int loop = 0;
   int max_retry = (_sbw < 2) ? 500 : (_sbw < 4) ? 250 : (_sbw < 6) ? 125 : (_sbw < 8) ? 60 : 30;

   // Oczekiwanie na flage TX_DONE
   while(1) {
      int irq = lora_read_reg(REG_IRQ_FLAGS);
      if ((irq & IRQ_TX_DONE_MASK) == IRQ_TX_DONE_MASK) break;
      
      loop++;
      if (loop == max_retry) break;
      
      // Zakomentowane aby szybciej odczytac pik pradowy przy TX
      //vTaskDelay(pdMS_TO_TICKS(1)); // Zapobiega bledom Task Watchdoga
   }
   
   if (loop == max_retry) {
      _send_packet_lost++;
      ESP_LOGE(TAG, "lora_send_packet Fail (Timeout)");
      return -1;
   }
   lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK); // Skasuj flage przerwania
   return 0;
}

// Pobierz odebrany pakiet z FIFO (nieblokujace)
int 
lora_receive_packet(uint8_t *buf, int size)
{	
   spi_device_acquire_bus(_spi, portMAX_DELAY);
   
   int len = 0;
   int irq = lora_read_reg(REG_IRQ_FLAGS);
   lora_write_reg(REG_IRQ_FLAGS, irq); // Kasuj flagi przerwan
   
   if((irq & IRQ_RX_DONE_MASK) == 0) return 0;   
   if(irq & IRQ_PAYLOAD_CRC_ERROR_MASK) return 0; // Odrzuc jesli CRC jest uszkodzone

   if (_implicit) len = lora_read_reg(REG_PAYLOAD_LENGTH);
   else len = lora_read_reg(REG_RX_NB_BYTES);

   lora_idle();   
   lora_write_reg(REG_FIFO_ADDR_PTR, lora_read_reg(REG_FIFO_RX_CURRENT_ADDR));
   
   if(len > size) len = size; 
   lora_read_reg_buffer(REG_FIFO, buf, len); 
	
   spi_device_release_bus(_spi);
   return len;
}

// Sprawdz czy przyszedl nowy pakiet
int
lora_received(void)
{
   if(lora_read_reg(REG_IRQ_FLAGS) & IRQ_RX_DONE_MASK) return 1;
   return 0;
}

// Zwraca liczbe zgubionych pakietow przy nadawaniu
int 
lora_packet_lost(void)
{
   return (_send_packet_lost);
}

// Oblicz RSSI 
int 
lora_packet_rssi(void)
{
   return (lora_read_reg(REG_PKT_RSSI_VALUE) - (_frequency < 868E6 ? 164 : 157));
}

// Oblicz stosunek sygnalu do szumu SNR w dB
float 
lora_packet_snr(void)
{
   return ((int8_t)lora_read_reg(REG_PKT_SNR_VALUE)) * 0.25;
}

// Zamknij modul i uspij radio
void 
lora_close(void)
{
   lora_sleep();
}