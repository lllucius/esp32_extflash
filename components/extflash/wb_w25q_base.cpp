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

#include "wb_w25q_base.h"

static const char *TAG = "wb_w25q_base";

wb_w25q_base::wb_w25q_base()
{
}

wb_w25q_base::~wb_w25q_base()
{
}

uint8_t wb_w25q_base::read_status_register2()
{
    uint8_t status;

    cmd(true, CMD_READ_STATUS_REG2, &status, 1);
    wait_for_command_completion();

    return status;
}

void wb_w25q_base::write_status_register2(uint8_t status)
{
    cmd(CMD_SR_WRITE_ENABLE);
    cmd(false, CMD_WRITE_STATUS_REG2, &status, 1);
    wait_for_device_idle();
}

// ============================================================================
// ExtFlash implementation
// ============================================================================

void wb_w25q_base::reset()
{
    ESP_LOGD(TAG, "%s", __func__);

    mode_begin();
    mode_end();

    // XXX - FIXME - How to ensure CRM is reset???
    cmd(0xff);

    wait_for_device_idle();
    cmd(CMD_ENABLE_RESET);
    cmd(CMD_RESET_DEVICE);
    wait_for_device_idle();
}

