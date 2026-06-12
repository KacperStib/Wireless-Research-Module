#include "include/oled.h"
#include "include/font8x8_basic.h"
#include "i2c.h"

SSD1306_t dev;

// Inicjalizacja struktury i wyczyszczenie bufora
void ssd1306_init(SSD1306_t * dev, int width, int height)
{
    i2c_init(dev, width, height);

    // Inicjalizacja wewnetrznego bufora pamieci ekranu zerami
    for (int i = 0; i < dev->_pages; i++) 
    {
        memset(dev->_page[i]._segs, 0, 128);
    }
}

// Czyszczenie calego ekranu poprzez zapisanie go spacjami
void ssd1306_clear_screen(SSD1306_t * dev, bool invert)
{
    char space[16];
    memset(space, 0x00, sizeof(space));
    
    for (int page = 0; page < dev->_pages; page++) 
    {
        ssd1306_display_text(dev, page, space, sizeof(space), invert);
    }
}

// Zmiana kontrastu wyswietlacza
void ssd1306_contrast(SSD1306_t * dev, int contrast)
{
    i2c_contrast(dev, contrast);
}

// Rysowanie grafiki na ekranie oraz zapis do wewnetrznego bufora
void ssd1306_display_image(SSD1306_t * dev, int page, int seg, const uint8_t * images, int width)
{
    i2c_display_image(dev, page, seg, images, width);
	
    // Zapis przeslanej grafiki do wewnetrznego bufora ramki
    memcpy(&dev->_page[page]._segs[seg], images, width);
}

// Renderowanie tekstu przy uzyciu czcionki matrycowej 8x8
void ssd1306_display_text(SSD1306_t * dev, int page, const char * text, int text_len, bool invert)
{
    if (page >= dev->_pages) 
    {
        return;
    }
    
    int _text_len = text_len;
    if (_text_len > 16) 
    {
        _text_len = 16;
    }

    int seg = 0;
    uint8_t image[8];
    
    for (int i = 0; i < _text_len; i++) 
    {
        // Kopiowanie wzorca znaku z tablicy czcionek i wyslanie na ekran
        memcpy(image, font8x8_basic_tr[(uint8_t)text[i]], 8);
        ssd1306_display_image(dev, page, seg, image, 8);
        seg = seg + 8;
    }
}

void ssd1306_update(float current_mA, int gps_fix, int gps_sats, float gps_lat, float gps_lon, 
					radio_config_t radio, float current_mA_peak, int rssi, uint32_t tx_time)
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
}