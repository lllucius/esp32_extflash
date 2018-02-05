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

#include "esp_err.h"
#include "esp_log.h"

#include "extflash.h"

static const char *TAG = "extflash";

ExtFlash::ExtFlash()
{
    spi = NULL;

    capacity = 0;
    sector_sz = 0;

    trans = NULL;
    queued = 0;
    qnext = 0;
}

ExtFlash::~ExtFlash()
{
    if (spi)
    {
        spi_bus_remove_device(spi);
        spi_bus_free(bus);
    }

    if (trans)
    {
        delete [] trans;
    }
}

esp_err_t ExtFlash::init(const ext_flash_config_t *config)
{
    ESP_LOGD(TAG, "%s", __func__);

    esp_err_t err;

    cfg = *config;

    if ((cfg.sector_size != 0) + (cfg.capacity != 0) == 1)
    {
        ESP_LOGE(TAG, "sector_size and capacticy config values must both be set (or neither)");
        return ESP_ERR_INVALID_ARG;
    }

    if (cfg.queue_size < 1)
    {
        ESP_LOGE(TAG, "queue_size config value must be greather than 0");
        return ESP_ERR_INVALID_ARG;
    }

    if (cfg.speed_mhz <= 0)
    {
        ESP_LOGE(TAG, "speed_mhz config value must be greater than 0");
        return ESP_ERR_INVALID_ARG;
    }

    if (cfg.dma_channel != 1 && cfg.dma_channel != 2)
    {
        ESP_LOGE(TAG, "dma_channel config value must either 1 or 2");
        return ESP_ERR_INVALID_ARG;
    }

    if ((cfg.hd_io_num != -1) + (cfg.wp_io_num != -1) == 1)
    {
        ESP_LOGE(TAG, "hd_io_num and wp_io_num config values must both be set");
        return ESP_ERR_INVALID_ARG;
    }

    spi_bus_config_t buscfg =
    {
        .mosi_io_num = cfg.mosi_io_num,
        .miso_io_num = cfg.miso_io_num,
        .sclk_io_num = cfg.sck_io_num,
        .quadwp_io_num = cfg.wp_io_num,
        .quadhd_io_num = cfg.hd_io_num,
        .max_transfer_sz = SPI_MAX_DMA_LEN
    };

    spi_device_interface_config_t devcfg =
    {
        .command_bits = 8,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,
        .duty_cycle_pos = 0,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = cfg.speed_mhz * 1000 * 1000,
        .spics_io_num = cfg.ss_io_num,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .queue_size = cfg.queue_size,
        .pre_cb = NULL,
        .post_cb = NULL
    };

    bus = cfg.vspi ? VSPI_HOST : HSPI_HOST;

    trans = new spi_transaction_ext_t[cfg.queue_size];
    if (trans == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    err = spi_bus_initialize(bus, &buscfg, cfg.dma_channel);
    if (err != ESP_OK)
    {
        return err;
    }

    err = spi_bus_add_device(bus, &devcfg, &spi);
    if (err != ESP_OK)
    {
        spi_bus_free(bus);
        return err;
    }

    set_1_1_1();

    err = begin();
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

void ExtFlash::term()
{
    ESP_LOGD(TAG, "%s", __func__);

    if (spi)
    {
        reset();

        end();

        spi_bus_remove_device(spi);
        spi = NULL;

        spi_bus_free(bus);
    }

    if (trans)
    {
        delete [] trans;
        trans = NULL;
    }
}

spi_transaction_ext_t *ExtFlash::cmd_prolog()
{
    spi_transaction_ext_t *t = NULL;

    if (queued == cfg.queue_size)
    {
        spi_transaction_t *done;
        ESP_ERROR_CHECK(spi_device_get_trans_result(spi, &done, portMAX_DELAY));
        queued--;
    }
    qnext = (qnext + 1) % cfg.queue_size;

    t = &trans[qnext];
    *t = {};

    return t;
}

void ExtFlash::cmd_epilog(spi_transaction_ext_t *t, uint8_t *buf, size_t size, bool isread)
{
    if (isread)
    {
        t->base.rx_buffer = buf;
        t->base.rxlength = size * 8;
    }
    else
    {
        t->base.tx_buffer = buf;
        t->base.length = size * 8;
    }

    queued++;
    ESP_ERROR_CHECK(spi_device_queue_trans(spi, &t->base, portMAX_DELAY));
}

void ExtFlash::cmd_epilog(spi_transaction_ext_t *t)
{
    queued++;
    ESP_ERROR_CHECK(spi_device_queue_trans(spi, &t->base, portMAX_DELAY));
}

void ExtFlash::cmd(bool isread, uint8_t cmd, int64_t addr, uint8_t mode, uint8_t dummy, uint8_t *buf, size_t size)
{
    ESP_LOGV(TAG, "%s - isread=%d cmd=0x%02x addr=0x%08llx mode=0x%02x dummy=%d size=%d", __func__, isread, cmd, addr, mode, dummy, size);

    spi_transaction_ext_t *t = cmd_prolog();

    if (is_qpi)
    {
        t->base.flags = SPI_TRANS_VARIABLE_CMD |
                        SPI_TRANS_VARIABLE_ADDR |
                        SPI_TRANS_MODE_QIO |
                        SPI_TRANS_MODE_DIOQIO_ADDR;
        t->base.addr = ((((cmd << 24) | addr) << 8) | mode) << dummy;
        t->address_bits = (cmd ? 8 : 0) + 24 + 8 + dummy;
    }
    else
    {
        t->base.flags = tflags;
        t->base.cmd = cmd;
        t->base.addr = ((addr << 8) | mode) << dummy;
        t->address_bits = 24 + 8 + dummy;
        t->command_bits = (cmd ? 8 : 0);
    }

    cmd_epilog(t, buf, size, isread);
};

void ExtFlash::cmd(bool isread, uint8_t cmd, int64_t addr, uint8_t dummy, uint8_t *buf, size_t size)
{
    ESP_LOGV(TAG, "%s - isread=%d cmd=0x%02x addr=0x%08llx dummy=%d size=%d", __func__, isread, cmd, addr, dummy, size);

    spi_transaction_ext_t *t = cmd_prolog();

    if (is_qpi)
    {
        t->base.flags = SPI_TRANS_VARIABLE_CMD |
                        SPI_TRANS_VARIABLE_ADDR |
                        SPI_TRANS_MODE_QIO |
                        SPI_TRANS_MODE_DIOQIO_ADDR;
        t->base.addr = ((cmd << 24) | addr) << dummy;
        t->address_bits = 8 + 24 + dummy;
    }
    else
    {
        t->base.flags = tflags;
        t->base.cmd = cmd;
        t->base.addr = addr << dummy;
        t->address_bits = 24 + dummy;
    }

    cmd_epilog(t, buf, size, isread);
}

void ExtFlash::cmd(bool isread, uint8_t cmd, int64_t addr, uint8_t *buf, size_t size)
{
    ESP_LOGV(TAG, "%s - isread=%d cmd=0x%02x addr=0x%08llx size=%d", __func__, isread, cmd, addr, size);

    spi_transaction_ext_t *t = cmd_prolog();

    if (is_qpi)
    {
        t->base.flags = SPI_TRANS_VARIABLE_CMD |
                        SPI_TRANS_VARIABLE_ADDR |
                        SPI_TRANS_MODE_QIO |
                        SPI_TRANS_MODE_DIOQIO_ADDR;
        t->base.addr = (cmd << 24) | addr;
        t->address_bits = 8 + 24;
    }
    else
    {
        t->base.flags = tflags;
        t->base.cmd = cmd;
        t->base.addr = addr;
        t->address_bits = 24;
    }

    cmd_epilog(t, buf, size, isread);
}

void ExtFlash::cmd(bool isread, uint8_t cmd, uint8_t *buf, size_t size)
{
    ESP_LOGV(TAG, "%s - isread=%d cmd=0x%02x size=%d", __func__, isread, cmd, size);

    spi_transaction_ext_t *t = cmd_prolog();

    if (is_qpi)
    {
        t->base.flags = SPI_TRANS_VARIABLE_CMD |
                        SPI_TRANS_VARIABLE_ADDR |
                        SPI_TRANS_MODE_QIO |
                        SPI_TRANS_MODE_DIOQIO_ADDR;
        t->base.addr = cmd;
        t->address_bits = 8;
    }
    else
    {
        t->base.flags = tflags;
        t->base.cmd = cmd;
    }

    cmd_epilog(t, buf, size, isread);
}

void ExtFlash::cmd(uint8_t cmd, int64_t addr)
{
    ESP_LOGV(TAG, "%s - cmd=0x%02x addr=0x%08llx", __func__, cmd, addr);

    spi_transaction_ext_t *t = cmd_prolog();

    if (is_qpi)
    {
        t->base.flags = SPI_TRANS_VARIABLE_CMD |
                        SPI_TRANS_VARIABLE_ADDR |
                        SPI_TRANS_MODE_QIO |
                        SPI_TRANS_MODE_DIOQIO_ADDR;
        uint8_t *buf = (uint8_t *) &t->base.addr;
        buf[0] = cmd;
        buf[1] = (addr >> 16) & 0xff;
        buf[2] = (addr >> 8) & 0xff;
        buf[3] = (addr >> 0) & 0xff;
        cmd_epilog(t, buf, 4, false);
    }
    else
    {
        t->base.flags = tflags;
        t->base.cmd = cmd;
        t->base.addr = addr;
        t->address_bits = 24;
        cmd_epilog(t);
    }
}

void ExtFlash::cmd(uint8_t cmd)
{
    ESP_LOGV(TAG, "%s - cmd=0x%02x", __func__, cmd);

    spi_transaction_ext_t *t = cmd_prolog();

    if (is_qpi)
    {
        t->base.flags = SPI_TRANS_VARIABLE_CMD |
                        SPI_TRANS_VARIABLE_ADDR |
                        SPI_TRANS_MODE_QIO |
                        SPI_TRANS_MODE_DIOQIO_ADDR;
        t->base.tx_buffer = &cmd;
        t->base.length = 8;
    }
    else
    {
        t->base.flags = tflags;
        t->base.cmd = cmd;
    }

    cmd_epilog(t);
}

void ExtFlash::wait_for_command_completion()
{
    while (queued > 0)
    {
        spi_transaction_t *t;
  
        ESP_ERROR_CHECK(spi_device_get_trans_result(spi, &t, portMAX_DELAY));
        queued--;
    }
}

void ExtFlash::set_1_1_1()
{
    tflags = SPI_TRANS_VARIABLE_ADDR;
}

void ExtFlash::set_1_1_2()
{
    tflags = SPI_TRANS_VARIABLE_ADDR |
             SPI_TRANS_MODE_DIO;
}

void ExtFlash::set_1_1_4()
{
    tflags = SPI_TRANS_VARIABLE_ADDR |
             SPI_TRANS_MODE_QIO;
}

void ExtFlash::set_1_2_2()
{
    tflags = SPI_TRANS_VARIABLE_ADDR |
             SPI_TRANS_VARIABLE_CMD |
             SPI_TRANS_MODE_DIO |
             SPI_TRANS_MODE_DIOQIO_ADDR;
}

void ExtFlash::set_1_4_4()
{
    tflags = SPI_TRANS_VARIABLE_ADDR |
             SPI_TRANS_VARIABLE_CMD |
             SPI_TRANS_MODE_QIO |
             SPI_TRANS_MODE_DIOQIO_ADDR;
}

void ExtFlash::qpi_enable()
{
    is_qpi = true;
}

void ExtFlash::qpi_disable()
{
    is_qpi = false;
}

esp_err_t ExtFlash::begin()
{
    ESP_LOGD(TAG, "%s", __func__);

    reset();

    if (cfg.sector_size != 0 && cfg.capacity != 0)
    {
        sector_sz = cfg.sector_size;
        capacity = cfg.capacity;
    }
    else
    {
        read_sfdp();

        if (capacity == 0)
        {
            WORD_ALIGNED_ATTR uint8_t id[3];

            cmd(true, CMD_READ_JEDEC_ID, id, sizeof(id));
            wait_for_command_completion();

            if (id[2] >= 0x10 && id[2] <= 0x17)
            {
                capacity = 1 << id[2];
            }
        }

        if (capacity == 0 || sector_sz == 0)
        {
            return ESP_FAIL;
        }
    }

    mode_begin();

    return ESP_OK;
}

void ExtFlash::end()
{
    ESP_LOGD(TAG, "%s", __func__);

    mode_end();

    reset();
}

void ExtFlash::mode_begin()
{
}

void ExtFlash::mode_end()
{
}

void ExtFlash::write_enable()
{
    cmd(CMD_WRITE_ENABLE);
}

uint8_t ExtFlash::read_status_register1()
{
    uint8_t status;

    cmd(true, CMD_READ_STATUS_REG1, &status, 1);
    wait_for_command_completion();

    return status;
}

void ExtFlash::write_status_register1(uint8_t status)
{
    cmd(CMD_WRITE_ENABLE);
    cmd(false, CMD_WRITE_STATUS_REG1, &status, 1);
    wait_for_device_idle();
}

void ExtFlash::wait_for_device_idle()
{
    wait_for_command_completion();

    int i = 0;
    while (read_status_register1() & sr1_wip)
    {
        i++;
        if (i == 1000)
        {
            i = 0;
            vTaskDelay(1);
        }
    }
}

void ExtFlash::reset()
{
    ESP_LOGD(TAG, "%s", __func__);

    set_1_1_1();
    cmd(CMD_ENABLE_RESET);
    cmd(CMD_RESET_DEVICE);
    wait_for_device_idle();
}

bool ExtFlash::read_sfdp()
{
    ESP_LOGD(TAG, "%s", __func__);

    uint8_t sfdp[256] = {};
    uint32_t *p = (uint32_t *) sfdp;

    cmd(true, CMD_READ_SFDP, 0, 8, sfdp, 8);
    wait_for_command_completion();

    if (p[0] != ('S' | 'F' << 8 | 'D' << 16 | 'P' << 24))
    {
        return false;
    }

    int8_t nph = (p[1] >> 16) & 0xff;

    uint32_t ptp = 0;
    uint8_t ptl = 0;
    uint32_t off = 8;
    while (nph-- >= 0)
    {
        cmd(true, CMD_READ_SFDP, off, 8, sfdp, 8);
        wait_for_command_completion();

        uint32_t dword1 = p[0];
        uint32_t dword2 = p[1];

        if (((dword2 >> 24) & 0xff) == 0xff)
        {
            ptp = dword2 & 0x00ffffff;
            ptl = (dword1 >> 24) & 0xff; 
        }
    }

    if (ptp == 0 || ptl == 0 || ptl > 32)
    {
        return false;
    }

    cmd(true, CMD_READ_SFDP, ptp, 8, sfdp, ptl << 3);
    wait_for_command_completion();

    for (int i = 0; i < ptl; i++)
    {
        uint32_t dword = p[i];
        switch (i)
        {
            case 0:
                sector_sz = (dword & 0x03) == 1 ? 4096 : 0;
            break;

            case 1:
                if (dword & 0x80000000)
                {
                    capacity = 1 << (dword & 0x7fffffff);
                }
                else
                {
                    capacity = (dword + 1) / 8;
                }
            break;

            case 7:
                sector_sz = 1 << (dword & 0xff);
            break;
        }
    }

    return true;
}

esp_err_t ExtFlash::read_nocrm(uint8_t inst, uint8_t dummy, size_t addr, void *dest, size_t size)
{
    ESP_LOGD(TAG, "%s - inst=0x%02x dummy=%d addr=0x%08x size=%d", __func__, inst, dummy, addr, size);

    uint8_t *bytes = (uint8_t *) dest;
    size_t len = SPI_MAX_DMA_LEN;

    while (size > 0)
    {
        if (len >= size)
        {
            len = size;
        }

        cmd(true, inst, addr, dummy, bytes, len);

        addr += len;
        bytes += len;
        size -= len;
    }

    wait_for_command_completion();

    return ESP_OK;
}

esp_err_t ExtFlash::read_crm(uint8_t inst, uint8_t on, uint8_t off, uint8_t dummy, size_t addr, void *dest, size_t size)
{
    ESP_LOGD(TAG, "%s - inst=0x%02x on=0x%02x off=0x%02x dummy=%d addr=0x%08x size=%d", __func__, inst, on, off, dummy, addr, size);

    uint8_t *bytes = (uint8_t *) dest;
    size_t len = SPI_MAX_DMA_LEN;
    uint8_t mode = on;

    while (size > 0)
    {
        if (len >= size)
        {
            len = size;
            mode = off;
        }

        cmd(true, inst, addr, mode, dummy, bytes, len);
        inst = 0;

        addr += len;
        bytes += len;
        size -= len;
    }

    wait_for_command_completion();

    return ESP_OK;
}
size_t ExtFlash::sector_size()
{
    ESP_LOGD(TAG, "%s - %d", __func__, sector_sz);

    return sector_sz;
}

size_t ExtFlash::chip_size()
{
    ESP_LOGD(TAG, "%s - %d", __func__, capacity);

    return capacity;
}

esp_err_t ExtFlash::erase_sector(size_t sector)
{
    ESP_LOGD(TAG, "%s - sector=0x%08x", __func__, sector);

    write_enable();
    cmd(CMD_SECTOR_ERASE, sector * sector_sz);
    wait_for_device_idle();

    return ESP_OK;
}

esp_err_t ExtFlash::erase_range(size_t addr, size_t size)
{
    ESP_LOGD(TAG, "%s - add=0x%08x size=%d", __func__, addr, size);

    while (size > 0)
    {
        write_enable();
        cmd(CMD_SECTOR_ERASE, addr);
        wait_for_device_idle();

        addr += sector_sz;
        size -= sector_sz;
    }

    return ESP_OK;
}

esp_err_t ExtFlash::erase_chip()
{
    ESP_LOGD(TAG, "%s", __func__);

    write_enable();

    cmd(CMD_CHIP_ERASE);

    wait_for_device_idle();

    return ESP_OK;
}

esp_err_t ExtFlash::write(size_t addr, const void *src, size_t size)
{
    ESP_LOGD(TAG, "%s - addr=0x%08x size=%d", __func__, addr, size);

    uint8_t *bytes = (uint8_t *) src;
    size_t len = pagesize;

    while (size > 0)
    {
        if (len > size)
        {
            len = size;
        }

        write_enable();
        cmd(false, CMD_PAGE_PROGRAM, addr, bytes, len);
        wait_for_device_idle();

        addr += len;
        bytes += len;
        size -= len;

        len = pagesize;
    }

    return ESP_OK;
}

esp_err_t ExtFlash::read(size_t addr, void *dest, size_t size)
{
    ESP_LOGD(TAG, "%s - addr=0x%08x size=%d", __func__, addr, size);

    return read_nocrm(CMD_FAST_READ, 8, addr, dest, size);
}

