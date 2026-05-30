#include "i2c_hal.h"

static i2c_master_bus_handle_t bus_handle = NULL;

esp_err_t i2c_hal_init(void){
    if (bus_handle != NULL){
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    return i2c_new_master_bus(&bus_cfg, &bus_handle);
}

i2c_master_bus_handle_t i2c_hal_get_bus(void){
    return bus_handle;
}