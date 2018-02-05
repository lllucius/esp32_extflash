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

#if !defined(_WB_W25Q_BASE_H_)
#define _WB_W25Q_BASE_H_ 1

#include "extflash.h"

#define CMD_WRITE_STATUS_REG2               0x31
#define CMD_READ_STATUS_REG2                0x35
#define CMD_ENTER_QPI_MODE                  0x38
#define CMD_FAST_READ_DUAL_OUTPUT           0x3b
#define CMD_SR_WRITE_ENABLE                 0x50
#define CMD_BLOCK_ERASE_32K                 0x52
#define CMD_FAST_READ_QUAD_OUTPUT           0x6b
#define CMD_FAST_READ_DUAL_IO               0xbb
#define CMD_SET_READ_PARAMETERS             0xc0
#define CMD_BLOCK_ERASE_64K                 0xd8
#define CMD_OCTAL_WORD_READ_QUAD_IO         0xe3
#define CMD_WORD_READ_QUAD_IO               0xe7
#define CMD_FAST_READ_QUAD_IO               0xeb
#define CMD_EXIT_QPI_MODE                   0xff

class wb_w25q_base : public ExtFlash
{
public:
    wb_w25q_base();
    virtual ~wb_w25q_base();

    //
    // ExtFlash implementation
    //
    virtual void reset() override;

protected:
    uint8_t read_status_register2();
    void write_status_register2(uint8_t status);

protected:
    static const uint8_t crm_on = 0x20;
    static const uint8_t crm_off = 0x10;
    static const uint8_t sr2_quad_enable = 0x02;
};

#endif
