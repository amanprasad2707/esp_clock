#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

esp_err_t i2c_hal_init(void);

i2c_master_bus_handle_t i2c_hal_get_bus(void);

esp_err_t i2c_hal_add_device(uint16_t addr,uint32_t speed, i2c_master_dev_handle_t *dev);