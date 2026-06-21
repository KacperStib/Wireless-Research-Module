#include <stdio.h>
#include "spi.h"

// Inicjalizacja wspoldzielonej magistrali SPI2_HOST
esp_err_t spi_init(void)
{
    // Konfiguracja linii sygnalowych magistrali SPI
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = SPI_MOSI,
        .miso_io_num     = SPI_MISO,
        .sclk_io_num     = SPI_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,
    };
 
    // Inicjalizacja hosta SPI z automatyczna alokacja kanalu DMA
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG_INIT, "spi_bus_initialize() failed: %s", esp_err_to_name(ret));
        return ret;
    }
 
    ESP_LOGI(TAG_INIT, "Magistrala SPI2 gotowa (MOSI=%d MISO=%d SCK=%d)", SPI_MOSI, SPI_MISO, SPI_SCK);
    return ESP_OK;
}