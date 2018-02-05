// Copyright 2017-2018 Leland Lucius
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "extflash.h"
#include "wb_w25q_dual.h"
#include "wb_w25q_dio.h"
#include "wb_w25q_quad.h"
#include "wb_w25q_qio.h"
#include "wb_w25q_qpi.h"

#define PIN_SPI_MOSI    GPIO_NUM_23     // PIN 5 - IO0 - DI
#define PIN_SPI_MISO    GPIO_NUM_19     // PIN 2 - IO1 - DO
#define PIN_SPI_WP      GPIO_NUM_22     // PIN 3 - IO2 - /WP
#define PIN_SPI_HD      GPIO_NUM_21     // PIN 7 - IO3 - /HOLD - /RESET
#define PIN_SPI_SCK     GPIO_NUM_18     // PIN 6 - CLK - CLK
#define PIN_SPI_SS      GPIO_NUM_5      // PIN 1 - /CS - /CS

#define ENABLE_READ_TEST    0
#define ENABLE_WRITE_TEST   1

#if ENABLE_READ_TEST

void read_test(ExtFlash & flash, const char *name, const char *cycles)
{
    for (int mhz = 10; mhz <= 80; mhz <<= 1)
    {
        for (int qs = 1; qs <= 4; qs++)
        {
            ext_flash_config_t cfg =
            {
                .vspi = true,
                .sck_io_num = PIN_SPI_SCK,
                .miso_io_num = PIN_SPI_MISO,
                .mosi_io_num = PIN_SPI_MOSI,
                .ss_io_num = PIN_SPI_SS,
                .hd_io_num = PIN_SPI_HD,
                .wp_io_num = PIN_SPI_WP,
                .speed_mhz = (int8_t) mhz,
                .dma_channel = 1,
                .queue_size = (int8_t) qs,
                .sector_size = 0,
                .capacity = 0
            };

            esp_err_t err = flash.init(&cfg);
            if (err != ESP_OK)
            {
                printf("Flash initialization failed %d for %s\n", err, name);
            }
            else
            {
                int sector_sz = flash.sector_size();
                int cap = flash.chip_size();

                for (int bs = 256; bs <= sector_sz * 16; bs <<= 1)
                {
                    uint8_t *buf = (uint8_t *) malloc(bs);
                    int bc = cap / bs;

                    struct timeval start;
                    gettimeofday(&start, NULL);

                    for (int i = 0; i < bc; i++)
                    {
                        flash.read(i * bs, buf, bs);
                    }

                    struct timeval end;
                    gettimeofday(&end, NULL);

                    struct timeval elapsed;
                    timersub(&end, &start, &elapsed);

                    float ms = elapsed.tv_sec * 1000000.0 + elapsed.tv_usec;
                    float mbs = (((bs * bc) / ms) * 1000000.0) / 1048576.0;

                    printf("%-5.5s  %-6.6s  %3d  %5d  %5d  %5d  %5.2f  %6.2f\n",
                           name, cycles, mhz, qs, bs, bc, ms / 1000000.0, mbs);

                    free(buf);
                }
            }

            flash.term();
        }
    }
}

#endif
 
#if ENABLE_WRITE_TEST

void write_test(ExtFlash & flash, const char *name, const char *cycles)
{
    printf("%-5.5s  %-6.6s  ", name, cycles);

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
        printf("initialization failed %d\n", err);
    }
    else
    {
        int cap = flash.chip_size();
        int sector_sz = flash.sector_size();
        int step = cap / 16;
        uint8_t *rbuf = (uint8_t *) malloc(sector_sz);
        uint8_t *wbuf = (uint8_t *) malloc(sector_sz);
        
        for (int i = 0; i < sector_sz; i++)
        {
            wbuf[i] = i & 0xff;
            rbuf[i] = 0;
        }

        int sectors = 0;
        int addr;

        for (addr = 0; addr < cap; addr += step)
        {
            sectors++;
            flash.erase_sector(addr / sector_sz);
            flash.write(addr, wbuf, sector_sz);
            flash.read(addr, rbuf, sector_sz);
            bool good = true;
            for (int b = 0; b < sector_sz; b++)
            {
                if (wbuf[b] != rbuf[b])
                {
                    printf("erase/write/verify failed at block %d offset %d\n", addr / sector_sz, b);
                    for (int i = 0; i < sector_sz; i += 16)
                    {
                        printf("wbuf %08x: "
                               "%02x%02x%02x%02x "
                               "%02x%02x%02x%02x "
                               "%02x%02x%02x%02x "
                               "%02x%02x%02x%02x ",
                               addr + i,
                               wbuf[i + 0], wbuf[i + 1], wbuf[i + 2], wbuf[i + 3],
                               wbuf[i + 4], wbuf[i + 5], wbuf[i + 6], wbuf[i + 7],
                               wbuf[i + 8], wbuf[i + 9], wbuf[i + 10], wbuf[i + 11],
                               wbuf[i + 12], wbuf[i + 13], wbuf[i + 14], wbuf[i + 15]);
                        for (int j = i; j < i + 16; j++)
                        {
                            printf("%c", isprint(wbuf[j]) ? wbuf[j] : '.');
                        }
                        printf("\n");
                    }
       
                    for (int i = 0; i < sector_sz; i += 16)
                    {
                        printf("rbuf %08x: "
                               "%02x%02x%02x%02x "
                               "%02x%02x%02x%02x "
                               "%02x%02x%02x%02x "
                               "%02x%02x%02x%02x ",
                               addr + i,
                               rbuf[i + 0], rbuf[i + 1], rbuf[i + 2], rbuf[i + 3],
                               rbuf[i + 4], rbuf[i + 5], rbuf[i + 6], rbuf[i + 7],
                               rbuf[i + 8], rbuf[i + 9], rbuf[i + 10], rbuf[i + 11],
                               rbuf[i + 12], rbuf[i + 13], rbuf[i + 14], rbuf[i + 15]);
                        for (int j = i; j < i + 16; j++)
                        {
                            printf("%c", isprint(rbuf[j]) ? rbuf[j] : '.');
                        }
                        printf("\n");
                    }
                    good = false;
                    break;
                }
            } 

            if (!good)
            {
                break;
            }
        }

        if (addr >= cap)
        {
            printf("erase/write/verify successfully tested %d sectors\n", sectors);
        }

        free(wbuf);
        free(rbuf);
    }

    flash.term();
}
#endif

extern "C" void app_main(void *)
{

#if ENABLE_READ_TEST

#define READ_TEST(c, n, b)      \
    {                           \
        c flash;                \
        read_test(flash, n, b); \
    }

    printf("READ TEST...\n\n");
    printf("       Bus     Bus  Queue  Block  Block               \n");
    printf("Proto  Cycles  Mhz   Size   Size  Count   Secs    MB/s\n");

//    READ_TEST(ExtFlash,     "std",  "1-1-1");
//    READ_TEST(wb_w25q_dual, "dual", "1-1-2");
//    READ_TEST(wb_w25q_dio,  "dio",  "1-2-2");
    READ_TEST(wb_w25q_quad, "quad", "1-1-4");
//    READ_TEST(wb_w25q_qio,  "qio",  "1-4-4");
//    READ_TEST(wb_w25q_qpi,  "qpi",  "4-4-4");

#endif

#if ENABLE_WRITE_TEST

#define WRITE_TEST(c, n, b)      \
    {                            \
        c flash;                 \
        write_test(flash, n, b); \
    }

    printf("\n");

    printf("ERASE/WRITE/VERIFY Test...\n\n"); 
    printf("       Bus   \n");
    printf("Proto  Cycles\n");

    WRITE_TEST(ExtFlash,     "std",  "1-1-1");
    WRITE_TEST(wb_w25q_dual, "dual", "1-1-2");
    WRITE_TEST(wb_w25q_dio,  "dio",  "1-2-2");
    WRITE_TEST(wb_w25q_quad, "quad", "1-1-4");
    WRITE_TEST(wb_w25q_qio,  "qio",  "1-4-4");
    WRITE_TEST(wb_w25q_qpi,  "qpi",  "4-4-4");

#endif

    printf("\nDone...\n");

    vTaskDelay(portMAX_DELAY);
}
