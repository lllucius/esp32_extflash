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

#if !defined(_WB_W25Q_DIO_H_)
#define _WB_W25Q_DIO_H_ 1

#include "wb_w25q_base.h"

class wb_w25q_dio : public wb_w25q_base
{
public:
    wb_w25q_dio();
    virtual ~wb_w25q_dio();

    //
    // ExtFlash implementaion
    //
    virtual esp_err_t read(size_t src_addr, void *dest, size_t size) final;
};

#endif
