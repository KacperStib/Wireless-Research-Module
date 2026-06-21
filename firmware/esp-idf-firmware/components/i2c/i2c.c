#include "include/i2c.h"

// Informacja czy odczyt czy zapis przez mastera jest w LSB adresu slavea

esp_err_t err = ESP_OK;

SemaphoreHandle_t i2c_mutex = NULL;

// Inicjalizacja
esp_err_t i2c_master_init(void)
{	
	// Utworz Mutex
	i2c_mutex = xSemaphoreCreateMutex();
    // Konfiguracja magistrali
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
	
    // Sprawdzenie errorow
    err = i2c_param_config(i2c_master_port, &conf);
    if (err != ESP_OK) 
    {
        return err;
    }
	
    // Instalacja sterownika magistrali
    err = i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    err = i2c_filter_enable(I2C_MASTER_NUM, 7);
    return err;
}

// Zapis jednego bajtu adresowego (wybór rejestru)
esp_err_t i2c_write_reg(uint8_t ADDR, uint8_t REG)
{	
	if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    // Zabranie zasobow
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // Bit startu
    i2c_master_start(cmd);
    
    // Adres slave'a z bitem zapisu
    i2c_master_write_byte(cmd, (ADDR << 1) | I2C_MASTER_WRITE, ACK_EN);
    
    // Rejestr
    i2c_master_write_byte(cmd, REG, ACK_EN);
    
    // Bit stopu
    i2c_master_stop(cmd);
    
    // Wykonanie komendy i zwolnienie zasobow
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    xSemaphoreGive(i2c_mutex);
    return err;
}

// Zapis jednego bajtu wartosci do rejestru
esp_err_t i2c_write_val(uint8_t ADDR, uint8_t REG, uint8_t VAL)
{	
	if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
        
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADDR << 1) | I2C_MASTER_WRITE, ACK_EN);
    i2c_master_write_byte(cmd, REG, ACK_EN);
    
    // Dane
    i2c_master_write_byte(cmd, VAL, ACK_EN);
    
    i2c_master_stop(cmd);
    
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    xSemaphoreGive(i2c_mutex);
    return err;
}

// Odczyt danych do bufora
esp_err_t i2c_read(uint8_t ADDR, uint8_t *buf, uint8_t bytesToReceive)
{	
	if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
        
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    
    // Adres slave'a z bitem odczytu
    i2c_master_write_byte(cmd, (ADDR << 1) | I2C_MASTER_READ, ACK_EN);
    
    // Odczytaj do bufora z zakonczeniem NACK dla ostatniego bajtu
    i2c_master_read(cmd, buf, bytesToReceive, I2C_MASTER_LAST_NACK);
    
    i2c_master_stop(cmd);
    
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    xSemaphoreGive(i2c_mutex);
    return err;
}

// Zapis dwoch bajtow 
esp_err_t i2c_write_2byte(uint8_t ADDR, uint8_t REG, uint16_t VAL)
{	
	if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
        
    // Podzial 2 bajtow uint16_t do przeslania na 2 oddzielne bajty uint8_t
    uint8_t bytes[2];
    bytes[0] = VAL >> 8;
    bytes[1] = VAL & 0xFF;
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADDR << 1) | I2C_MASTER_WRITE, ACK_EN);
    i2c_master_write_byte(cmd, REG, ACK_EN);
    
    // Wyslanie starszego i mlodszego bajtu
    i2c_master_write_byte(cmd, bytes[0], ACK_EN);
    i2c_master_write_byte(cmd, bytes[1], ACK_EN);
    
    i2c_master_stop(cmd);
    
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    xSemaphoreGive(i2c_mutex);
    return err;
}

// Zapis - odczyt w jednym linku
esp_err_t i2c_write_read(uint8_t ADDR, uint8_t REG, uint8_t *buf, uint8_t bytesToReceive)
{	
	if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
        
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // Zapis adresu rejestru 
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADDR << 1) | I2C_MASTER_WRITE, ACK_EN);
    i2c_master_write_byte(cmd, REG, ACK_EN);
    
    // Ponowny start i odczyt danych 
    i2c_master_start(cmd); // REPEATED START
    i2c_master_write_byte(cmd, (ADDR << 1) | I2C_MASTER_READ, ACK_EN);
    i2c_master_read(cmd, buf, bytesToReceive, I2C_MASTER_LAST_NACK);
    
    i2c_master_stop(cmd);
    
    // Wykonanie całej transakcji sprzętowo za jednym zamachem
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    
    xSemaphoreGive(i2c_mutex);
    return err;
}

// Super szybki zapis i odczyt w jednej sekwencji
esp_err_t i2c_write_read_fast(uint8_t ADDR, uint8_t REG, uint8_t *buf, uint8_t len)
{	
	if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
        
    // Makro I2C_LINK_RECOMMENDED_SIZE(n) — n = liczba komend w linku
    // Mamy: start, write, write, start, write, read, stop = 7 komend
    uint8_t i2c_cmd_buf[I2C_LINK_RECOMMENDED_SIZE(7)];
	
	// Static przyspiesza cala operacja o ~ 100 us !
    i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(i2c_cmd_buf, sizeof(i2c_cmd_buf));

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADDR << 1) | I2C_MASTER_WRITE, ACK_EN);
    i2c_master_write_byte(cmd, REG, ACK_EN);
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADDR << 1) | I2C_MASTER_READ, ACK_EN);
    i2c_master_read(cmd, buf, len, I2C_MASTER_LAST_NACK);
    
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete_static(cmd);
	
	xSemaphoreGive(i2c_mutex);
    return ret;
}

/*---------- Funkcje dla ekranu OLED ----------*/
// Zapozyczone z biblioteki "ssd1306.h""

// Inicjalizacja ekranu poprzez I2C
void i2c_init(SSD1306_t * dev, int width, int height) 
{	
	if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return;
        
    dev->_address = I2C_ADDRESS;
    dev->_flip = false;
    dev->_i2c_num = I2C_MASTER_NUM;
    dev->_width = width;
    dev->_height = height;
    dev->_pages = 8;
    
    if (dev->_height == 32) 
    {
        dev->_pages = 4;
    }
	
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_OFF, true); // AE
    i2c_master_write_byte(cmd, OLED_CMD_SET_MUX_RATIO, true); // A8
    
    if (dev->_height == 64) 
    {
        i2c_master_write_byte(cmd, 0x3F, true);
    }
    if (dev->_height == 32) 
    {
        i2c_master_write_byte(cmd, 0x1F, true);
    }
    
    i2c_master_write_byte(cmd, OLED_CMD_SET_DISPLAY_OFFSET, true); // D3
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_write_byte(cmd, OLED_CMD_SET_DISPLAY_START_LINE, true); // 40
    
    if (dev->_flip) 
    {
        i2c_master_write_byte(cmd, OLED_CMD_SET_SEGMENT_REMAP_0, true); // A0
    } 
    else 
    {
        i2c_master_write_byte(cmd, OLED_CMD_SET_SEGMENT_REMAP_1, true); // A1
    }
    
    i2c_master_write_byte(cmd, OLED_CMD_SET_COM_SCAN_MODE, true); // C8
    i2c_master_write_byte(cmd, OLED_CMD_SET_DISPLAY_CLK_DIV, true); // D5
    i2c_master_write_byte(cmd, 0x80, true);
    i2c_master_write_byte(cmd, OLED_CMD_SET_COM_PIN_MAP, true); // DA
    
    if (dev->_height == 64) 
    {
        i2c_master_write_byte(cmd, 0x12, true);
    }
    if (dev->_height == 32) 
    {
        i2c_master_write_byte(cmd, 0x02, true);
    }
    
    i2c_master_write_byte(cmd, OLED_CMD_SET_CONTRAST, true); // 81
    i2c_master_write_byte(cmd, 0xFF, true);
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_RAM, true); // A4
    i2c_master_write_byte(cmd, OLED_CMD_SET_VCOMH_DESELCT, true); // DB
    i2c_master_write_byte(cmd, 0x40, true);
    i2c_master_write_byte(cmd, OLED_CMD_SET_MEMORY_ADDR_MODE, true); // 20
    i2c_master_write_byte(cmd, OLED_CMD_SET_PAGE_ADDR_MODE, true); // 02
    
    // Set Lower Column Start Address for Page Addressing Mode
    i2c_master_write_byte(cmd, 0x00, true);
    // Set Higher Column Start Address for Page Addressing Mode
    i2c_master_write_byte(cmd, 0x10, true);
    i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP, true); // 8D
    i2c_master_write_byte(cmd, 0x14, true);
    i2c_master_write_byte(cmd, OLED_CMD_DEACTIVE_SCROLL, true); // 2E
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_NORMAL, true); // A6
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_ON, true); // AF

    i2c_master_stop(cmd);

    esp_err_t res = i2c_master_cmd_begin(dev->_i2c_num, cmd, I2C_TICKS_TO_WAIT);
    if (res == ESP_OK) 
    {
        ESP_LOGI(TAG, "OLED configured successfully");
    } 
    else 
    {
        ESP_LOGE(TAG, "OLED configuration failed. code: 0x%.2X", res);
    }
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(i2c_mutex);
}

// Wyswietlanie grafiki na ekranie poprzez I2C
void i2c_display_image(SSD1306_t * dev, int page, int seg, const uint8_t * images, int width) 
{	
	if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return;
        
    if (page >= dev->_pages) 
    {
        return;
    }
    if (seg >= dev->_width) 
    {
        return;
    }

    int _seg = seg + CONFIG_OFFSETX;
    uint8_t columLow = _seg & 0x0F;
    uint8_t columHigh = (_seg >> 4) & 0x0F;

    int _page = page;
    if (dev->_flip) 
    {
        _page = (dev->_pages - page) - 1;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->_address << 1) | I2C_MASTER_WRITE, true);

    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
    // Set Lower Column Start Address for Page Addressing Mode
    i2c_master_write_byte(cmd, (0x00 + columLow), true);
    // Set Higher Column Start Address for Page Addressing Mode
    i2c_master_write_byte(cmd, (0x10 + columHigh), true);
    // Set Page Start Address for Page Addressing Mode
    i2c_master_write_byte(cmd, 0xB0 | _page, true);

    i2c_master_stop(cmd);
    esp_err_t res = i2c_master_cmd_begin(dev->_i2c_num, cmd, I2C_TICKS_TO_WAIT);
    if (res != ESP_OK) 
    {
        ESP_LOGE(TAG, "Image command failed. code: 0x%.2X", res);
    }
    i2c_cmd_link_delete(cmd);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
    i2c_master_write(cmd, images, width, true);
    i2c_master_stop(cmd);

    res = i2c_master_cmd_begin(dev->_i2c_num, cmd, I2C_TICKS_TO_WAIT);
    if (res != ESP_OK) 
    {
        ESP_LOGE(TAG, "Image command failed. code: 0x%.2X", res);
    }
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(i2c_mutex);
}

// Ustawienie kontrastu wyswietlacza poprzez I2C
void i2c_contrast(SSD1306_t * dev, int contrast) 
{	
	if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return;
        
    int _contrast = contrast;
    if (contrast < 0x0) 
    {
        _contrast = 0;
    }
    if (contrast > 0xFF) 
    {
        _contrast = 0xFF;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true); // 00
    i2c_master_write_byte(cmd, OLED_CMD_SET_CONTRAST, true); // 81
    i2c_master_write_byte(cmd, _contrast, true);
    i2c_master_stop(cmd);

    esp_err_t res = i2c_master_cmd_begin(dev->_i2c_num, cmd, I2C_TICKS_TO_WAIT);
    if (res != ESP_OK) 
    {
        ESP_LOGE(TAG, "Contrast command failed. code: 0x%.2X", res);
    }
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(i2c_mutex);
}