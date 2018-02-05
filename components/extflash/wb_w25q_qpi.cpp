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

#include "wb_w25q_qpi.h"

static const char *TAG = "wb_w25q_qpi";

wb_w25q_qpi::wb_w25q_qpi()
{
}

wb_w25q_qpi::~wb_w25q_qpi()
{
}

// ============================================================================
// ExtFlash implementation
// ============================================================================

void wb_w25q_qpi::mode_begin()
{
    ESP_LOGD(TAG, "%s", __func__);

    write_status_register2(read_status_register2() | sr2_quad_enable);
    wait_for_device_idle();

    cmd(CMD_ENTER_QPI_MODE);
    qpi_enable();
    wait_for_device_idle();
}

void wb_w25q_qpi::mode_end()
{
    ESP_LOGD(TAG, "%s", __func__);

    cmd(CMD_EXIT_QPI_MODE);
    qpi_disable();
    wait_for_device_idle();

    write_status_register2(read_status_register2() & (~sr2_quad_enable));
    wait_for_device_idle();
}

esp_err_t wb_w25q_qpi::read(size_t addr, void *dest, size_t size)
{
    ESP_LOGD(TAG, "%s - addr=0x%08x size=%d", __func__, addr, size);

    return read_crm(CMD_FAST_READ_QUAD_IO, crm_on, crm_off, 0, addr, dest, size);
}

