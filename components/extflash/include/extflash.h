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

#if !defined(_EXTFLASH_H_)
#define _EXTFLASH_H_ 1

#include "esp_err.h"
#include "esp_log.h"
#include "driver/spi_master.h"

#define CMD_WRITE_STATUS_REG1               0x01
#define CMD_PAGE_PROGRAM                    0x02
#define CMD_READ_DATA                       0x03
#define CMD_READ_STATUS_REG1                0x05
#define CMD_WRITE_ENABLE                    0x06
#define CMD_FAST_READ                       0x0b
#define CMD_SECTOR_ERASE                    0x20
#define CMD_READ_SFDP                       0x5a
#define CMD_ENABLE_RESET                    0x66
#define CMD_RESET_DEVICE                    0x99
#define CMD_READ_JEDEC_ID                   0x9f
#define CMD_CHIP_ERASE                      0xc7

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

class ExtFlash
{
public:
    ExtFlash();
    virtual ~ExtFlash();

    esp_err_t init(const ext_flash_config_t *config);
    void term();

    virtual esp_err_t begin();
    virtual void end();

    virtual void mode_begin();
    virtual void mode_end();

    virtual size_t sector_size();
    virtual size_t chip_size();
    virtual esp_err_t erase_sector(size_t sector);
    virtual esp_err_t erase_range(size_t addr, size_t size);
    virtual esp_err_t erase_chip();
    virtual esp_err_t write(size_t addr, const void *src, size_t size);
    virtual esp_err_t read(size_t addr, void *dest, size_t size);

protected:
    void cmd(bool isread, uint8_t cmd, int64_t addr, uint8_t mode, uint8_t dummy, uint8_t *buf, size_t size);
    void cmd(bool isread, uint8_t cmd, int64_t addr, uint8_t dummy, uint8_t *buf, size_t size);
    void cmd(bool isread, uint8_t cmd, int64_t addr, uint8_t *buf, size_t size);
    void cmd(bool isread, uint8_t cmd, uint8_t *buf, size_t size);
    void cmd(uint8_t cmd, int64_t addr);
    void cmd(uint8_t cmd);

    void wait_for_command_completion();

    void set_1_1_1();
    void set_1_1_2();
    void set_1_1_4();
    void set_1_2_2();
    void set_1_4_4();

    void qpi_enable();
    void qpi_disable();

    virtual void write_enable();
    virtual uint8_t read_status_register1();
    virtual void write_status_register1(uint8_t status);
    virtual void wait_for_device_idle();
    virtual void reset();
    virtual bool read_sfdp();

    virtual esp_err_t read_nocrm(uint8_t inst, uint8_t dummy, size_t addr, void *dest, size_t size);
    virtual esp_err_t read_crm(uint8_t inst, uint8_t on, uint8_t off, uint8_t dummy, size_t addr, void *dest, size_t size);
 
protected:
    spi_device_handle_t spi;
    size_t sector_sz;
    size_t capacity;

    static const uint8_t sr1_wip = 0x01;
    static const int pagesize = 256;

private:
    spi_transaction_ext_t *cmd_prolog();
    void cmd_epilog(spi_transaction_ext_t *t, uint8_t *buf, size_t size, bool isread);
    void cmd_epilog(spi_transaction_ext_t *t);

private:
    ext_flash_config_t cfg;
    spi_host_device_t bus;

    uint32_t tflags;
    bool is_qpi;

    spi_transaction_ext_t *trans;
    int queued;
    int qnext;
};

#endif
