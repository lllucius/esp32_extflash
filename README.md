# External flash component for the ESP32
ExtFlash provides an interface to NOR flash chips connected
to either the HSPI or VSPI SPI bus.

It has only been tested on the Winbond W25Q128FV chip for now,
but should provide generic support to just about any chip that
supports the SPI protocol.

However, adding support for your specific chip can greatly
improve the speed.  See the differences between the "std" and "qio"
lines in the sample [results](RESULTS.md).

To use, just add the "extflash" component to your components directory
and initialize it with something like:

```
#include "extflash.h"

void app_main()
{
    ExtFlash flash;

    ext_flash_config_t cfg =
    {
        .vspi = true,
        .sck_io_num = PIN_SPI_SCK,
        .miso_io_num = PIN_SPI_MISO,
        .mosi_io_num = PIN_SPI_MOSI,
        .ss_io_num = PIN_SPI_SS,
        .hd_io_num = PIN_SPI_HD,
        .wp_io_num = PIN_SPI_WP,
        .speed_mhz = 40,
        .dma_channel = 1,
        .queue_size = 2,
        .sector_size = 0,
        .capacity = 0
    };

    esp_err_t err = flash.init(&cfg);
    if (err != ESP_OK)
    {
        ...
    }

    ...
}
```

The configuration options are:

```
typedef struct
{
    bool vspi;                  // true=VSPI, false=HSPI
    int8_t sck_io_num;          // any GPIO or VSPICLK = 18, HSPICLK = 14
    int8_t miso_io_num;         // any GPIO or VSPIQ   = 19, HSPIQ   = 12
    int8_t mosi_io_num;         // any GPIO or VSPID   = 23, HSPID   = 13
    int8_t ss_io_num;           // any GPIO or VSPICS0 = 5,  HSPICS0 = 15
    int8_t hd_io_num;           // any GPIO or VSPIHD  = 21, HSPIHD  = 4
    int8_t wp_io_num;           // any GPIO or VSPIWP  = 22, HSPIWP  = 2
    int8_t speed_mhz;           // ex. 10, 20, 40, 80
    int8_t dma_channel;         // must be 1 or 2
    int8_t queue_size;          // size of transaction queue, 1 - n
    size_t sector_size;         // sector size or 0 for detection
    size_t capacity;            // number of bytes on flash or 0 for detection
} ext_flash_config_t;
```

More documentation to follow.

