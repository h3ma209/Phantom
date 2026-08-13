#pragma once

#include "driver/spi_master.h"

/* ILI9341 SPI TFT */
#define PIN_MOSI 23
#define PIN_SCLK 18
#define PIN_CS   15
#define PIN_DC   2
#define PIN_RST  4
#define PIN_BL   21

#define SPI_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_HOST SPI2_HOST
