#include "rtc_ds3231.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ds3231";

/* -----------------------------------------------------------------------
 * Low-level register helpers
 * --------------------------------------------------------------------- */

static esp_err_t reg_write(ds3231_handle_t *h, uint8_t reg, uint8_t data){
    uint8_t buf[2] = { reg, data };
    return i2c_master_transmit(h->dev_handle, buf, sizeof(buf), DS3231_TIMEOUT_MS);
}

static esp_err_t reg_read(ds3231_handle_t *h, uint8_t reg, uint8_t *data){
    return i2c_master_transmit_receive(h->dev_handle, &reg, 1, data, 1, DS3231_TIMEOUT_MS);
}

/* -----------------------------------------------------------------------
 * BCD helpers
 * --------------------------------------------------------------------- */
static inline uint8_t bin2bcd(uint8_t val) { return ((val / 10) << 4) | (val % 10); }
static inline uint8_t bcd2bin(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }

/* -----------------------------------------------------------------------
 * Init / deinit
 * --------------------------------------------------------------------- */

esp_err_t ds3231_init(i2c_master_bus_handle_t bus_handle, uint8_t address, ds3231_handle_t *handle){
    if (!handle) return ESP_ERR_INVALID_ARG;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = address,
        .scl_speed_hz    = 400000,   /* DS3231 supports up to 400 kHz */
    };

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &handle->dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add DS3231 device: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t ds3231_deinit(ds3231_handle_t *handle){
    if (!handle) return ESP_ERR_INVALID_ARG;
    return i2c_master_bus_rm_device(handle->dev_handle);
}

/* -----------------------------------------------------------------------
 * Private time/date setters
 * --------------------------------------------------------------------- */

static esp_err_t set_hour_reg(ds3231_handle_t *h, uint8_t reg_addr, uint8_t hour, rtc_hour_format_t fmt, rtc_meridiem_t mer){
    uint8_t reg = 0;
    if (fmt == HOUR_FORMAT_12) {
        reg |= (1 << 6);                          /* bit6 → 12 h mode */
        if (mer == PM) reg |= (1 << 5);
        reg |= bin2bcd(hour) & 0x1F;
    } else {
        /* bit6 already 0 → 24 h mode */
        reg |= bin2bcd(hour) & 0x3F;
    }
    return reg_write(h, reg_addr, reg);
}

/* -----------------------------------------------------------------------
 * Time
 * --------------------------------------------------------------------- */

esp_err_t ds3231_set_time(ds3231_handle_t *handle, const rtc_time_t *time){
    if (!handle || !time) return ESP_ERR_INVALID_ARG;

    esp_err_t ret;
    ret = reg_write(handle, DS3231_ADDR_SEC, bin2bcd(time->seconds));
    if (ret != ESP_OK) return ret;
    ret = reg_write(handle, DS3231_ADDR_MIN, bin2bcd(time->minutes));
    if (ret != ESP_OK) return ret;
    return set_hour_reg(handle, DS3231_ADDR_HRS, time->hours, time->hour_format, time->meridiem);
}

esp_err_t ds3231_get_time(ds3231_handle_t *handle, rtc_time_t *time){
    if (!handle || !time) return ESP_ERR_INVALID_ARG;

    uint8_t hr, mn, sc;
    esp_err_t ret;

    ret = reg_read(handle, DS3231_ADDR_HRS, &hr); if (ret != ESP_OK) return ret;
    ret = reg_read(handle, DS3231_ADDR_MIN, &mn); if (ret != ESP_OK) return ret;
    ret = reg_read(handle, DS3231_ADDR_SEC, &sc); if (ret != ESP_OK) return ret;

    time->hour_format = (hr & (1 << 6)) ? HOUR_FORMAT_12 : HOUR_FORMAT_24;

    if (time->hour_format == HOUR_FORMAT_12) {
        time->meridiem = (hr & (1 << 5)) ? PM : AM;
        time->hours    = bcd2bin(hr & 0x1F);
    } else {
        time->meridiem = AM;
        time->hours    = bcd2bin(hr & 0x3F);
    }
    time->minutes = bcd2bin(mn & 0x7F);
    time->seconds = bcd2bin(sc & 0x7F);
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * Date
 * --------------------------------------------------------------------- */

esp_err_t ds3231_set_date(ds3231_handle_t *handle, const rtc_date_t *date){
    if (!handle || !date) return ESP_ERR_INVALID_ARG;
    if (date->date  < 1 || date->date  > 31) return ESP_ERR_INVALID_ARG;
    if (date->month < 1 || date->month > 12) return ESP_ERR_INVALID_ARG;
    if (date->year  < 2000 || date->year > 2199) return ESP_ERR_INVALID_ARG;

    esp_err_t ret;

    ret = reg_write(handle, DS3231_ADDR_DATE, bin2bcd(date->date));
    if (ret != ESP_OK) return ret;

    /* Month register also holds the century bit (bit 7) */
    uint8_t century  = (date->year >= 2100) ? 1 : 0;
    uint8_t monthReg = 0;
    ret = reg_read(handle, DS3231_ADDR_MONTH, &monthReg);
    if (ret != ESP_OK) return ret;
    monthReg &= 0x7F;                       /* clear century bit */
    monthReg |= (century << 7);
    monthReg  = (monthReg & 0x80) | (bin2bcd(date->month) & 0x1F);
    ret = reg_write(handle, DS3231_ADDR_MONTH, monthReg);
    if (ret != ESP_OK) return ret;

    ret = reg_write(handle, DS3231_ADDR_YEAR, bin2bcd((uint8_t)(date->year % 100)));
    if (ret != ESP_OK) return ret;

    ret = reg_write(handle, DS3231_ADDR_DAY, bin2bcd(date->day));
    return ret;
}

esp_err_t ds3231_get_date(ds3231_handle_t *handle, rtc_date_t *date){
    if (!handle || !date) return ESP_ERR_INVALID_ARG;

    uint8_t d, mo, yr, dy;
    esp_err_t ret;

    ret = reg_read(handle, DS3231_ADDR_DATE,  &d);  if (ret != ESP_OK) return ret;
    ret = reg_read(handle, DS3231_ADDR_MONTH, &mo); if (ret != ESP_OK) return ret;
    ret = reg_read(handle, DS3231_ADDR_YEAR,  &yr); if (ret != ESP_OK) return ret;
    ret = reg_read(handle, DS3231_ADDR_DAY,   &dy); if (ret != ESP_OK) return ret;

    date->date  = bcd2bin(d  & 0x3F);
    date->month = bcd2bin(mo & 0x1F);
    date->day   = bcd2bin(dy & 0x07);

    uint8_t  century = (mo & (1 << 7)) ? 1 : 0;
    date->year = 2000 + (century * 100) + bcd2bin(yr);
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * Temperature
 * --------------------------------------------------------------------- */

esp_err_t ds3231_get_temperature(ds3231_handle_t *handle, float *out_c){
    if (!handle || !out_c) return ESP_ERR_INVALID_ARG;

    uint8_t msb, lsb;
    esp_err_t ret;

    ret = reg_read(handle, DS3231_ADDR_TEMP_MSB, &msb); if (ret != ESP_OK) return ret;
    ret = reg_read(handle, DS3231_ADDR_TEMP_LSB, &lsb); if (ret != ESP_OK) return ret;

    int16_t raw = (int16_t)((msb << 2) | (lsb >> 6));
    *out_c = raw / 4.0f;
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * 32 kHz output
 * --------------------------------------------------------------------- */

esp_err_t ds3231_set_32khz_output(ds3231_handle_t *handle, ds3231_state_t state){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_STATUS, &reg);
    if (ret != ESP_OK) return ret;

    if (state == DS3231_ENABLED)
        reg |=  (1 << DS3231_STATUS_EN32KHZ);
    else
        reg &= ~(1 << DS3231_STATUS_EN32KHZ);

    return reg_write(handle, DS3231_ADDR_STATUS, reg);
}

esp_err_t ds3231_is_32khz_output_enabled(ds3231_handle_t *handle, bool *enabled){
    if (!handle || !enabled) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_STATUS, &reg);
    if (ret != ESP_OK) return ret;

    *enabled = (reg & (1 << DS3231_STATUS_EN32KHZ)) != 0;
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * Oscillator
 * --------------------------------------------------------------------- */

esp_err_t ds3231_is_oscillator_stopped(ds3231_handle_t *handle, bool *stopped){
    if (!handle || !stopped) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_STATUS, &reg);
    if (ret != ESP_OK) return ret;

    *stopped = (reg & (1 << DS3231_STATUS_OSF)) != 0;
    return ESP_OK;
}

esp_err_t ds3231_set_oscillator(ds3231_handle_t *handle, ds3231_state_t state){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_CONTROL, &reg);
    if (ret != ESP_OK) return ret;

    /* EOSC is active-LOW: clear to run, set to stop */
    if (state == DS3231_ENABLED)
        reg &= ~(1 << DS3231_CONTROL_EOSC);
    else
        reg |=  (1 << DS3231_CONTROL_EOSC);

    return reg_write(handle, DS3231_ADDR_CONTROL, reg);
}

/* -----------------------------------------------------------------------
 * Square-wave / interrupt output
 * --------------------------------------------------------------------- */

esp_err_t ds3231_set_battery_backed_sqwave(ds3231_handle_t *handle, ds3231_state_t   state){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_CONTROL, &reg);
    if (ret != ESP_OK) return ret;

    if (state == DS3231_ENABLED)
        reg |=  (1 << DS3231_CONTROL_BBSQW);
    else
        reg &= ~(1 << DS3231_CONTROL_BBSQW);

    return reg_write(handle, DS3231_ADDR_CONTROL, reg);
}

esp_err_t ds3231_set_sqwave_freq(ds3231_handle_t *handle, ds3231_sqwave_freq_t freq){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_CONTROL, &reg);
    if (ret != ESP_OK) return ret;

    reg &= ~(0x03 << DS3231_CONTROL_RS1);
    reg |=  ((freq & 0x03) << DS3231_CONTROL_RS1);

    return reg_write(handle, DS3231_ADDR_CONTROL, reg);
}

esp_err_t ds3231_set_output_mode(ds3231_handle_t *handle, ds3231_output_mode_t mode){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_CONTROL, &reg);
    if (ret != ESP_OK) return ret;

    if (mode == DS3231_OUTPUT_SQUARE_WAVE)
        reg &= ~(1 << DS3231_CONTROL_INTCN);
    else
        reg |=  (1 << DS3231_CONTROL_INTCN);

    return reg_write(handle, DS3231_ADDR_CONTROL, reg);
}

/* -----------------------------------------------------------------------
 * Alarm 1
 * --------------------------------------------------------------------- */

esp_err_t ds3231_set_alarm1_time(ds3231_handle_t *handle, const rtc_time_t *time){
    if (!handle || !time) return ESP_ERR_INVALID_ARG;

    esp_err_t ret;
    ret = reg_write(handle, DS3231_ADDR_ALM1_SEC, bin2bcd(time->seconds));
    if (ret != ESP_OK) return ret;
    ret = reg_write(handle, DS3231_ADDR_ALM1_MIN, bin2bcd(time->minutes));
    if (ret != ESP_OK) return ret;
    return set_hour_reg(handle, DS3231_ADDR_ALM1_HRS, time->hours, time->hour_format, time->meridiem);
}

esp_err_t ds3231_set_alarm1_date(ds3231_handle_t *handle, uint8_t date){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_ALM1_DAYDATE, &reg);
    if (ret != ESP_OK) return ret;

    reg &= ~(0x3F | (1 << DS3231_DYDT));  /* clear date bits + DY/DT */
    reg |= bin2bcd(date) & 0x3F;
    return reg_write(handle, DS3231_ADDR_ALM1_DAYDATE, reg);
}

esp_err_t ds3231_set_alarm1_day(ds3231_handle_t *handle, rtc_day_t day){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_ALM1_DAYDATE, &reg);
    if (ret != ESP_OK) return ret;

    reg &= ~0x3F;
    reg |= (1 << DS3231_DYDT);          /* set DY/DT → day mode */
    reg |= bin2bcd((uint8_t)day) & 0x3F;
    return reg_write(handle, DS3231_ADDR_ALM1_DAYDATE, reg);
}

esp_err_t ds3231_set_alarm1_mode(ds3231_handle_t *handle, ds3231_alarm1_mode_t mode){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret;

    /* SEC register: A1M1 */
    ret = reg_read(handle, DS3231_ADDR_ALM1_SEC, &reg); if (ret != ESP_OK) return ret;
    reg &= ~(1 << DS3231_AXMY);
    reg |=  (((mode >> 0) & 0x01) << DS3231_AXMY);
    ret = reg_write(handle, DS3231_ADDR_ALM1_SEC, reg); if (ret != ESP_OK) return ret;

    /* MIN register: A1M2 */
    ret = reg_read(handle, DS3231_ADDR_ALM1_MIN, &reg); if (ret != ESP_OK) return ret;
    reg &= ~(1 << DS3231_AXMY);
    reg |=  (((mode >> 1) & 0x01) << DS3231_AXMY);
    ret = reg_write(handle, DS3231_ADDR_ALM1_MIN, reg); if (ret != ESP_OK) return ret;

    /* HRS register: A1M3 */
    ret = reg_read(handle, DS3231_ADDR_ALM1_HRS, &reg); if (ret != ESP_OK) return ret;
    reg &= ~(1 << DS3231_AXMY);
    reg |=  (((mode >> 2) & 0x01) << DS3231_AXMY);
    ret = reg_write(handle, DS3231_ADDR_ALM1_HRS, reg); if (ret != ESP_OK) return ret;

    /* DAY/DATE register: A1M4 + DY/DT */
    ret = reg_read(handle, DS3231_ADDR_ALM1_DAYDATE, &reg); if (ret != ESP_OK) return ret;
    reg &= ~((1 << DS3231_AXMY) | (1 << DS3231_DYDT));
    reg |=  (((mode >> 3) & 0x01) << DS3231_AXMY);
    reg |=  (((mode >> 4) & 0x01) << DS3231_DYDT);
    return reg_write(handle, DS3231_ADDR_ALM1_DAYDATE, reg);
}


esp_err_t ds3231_enable_alarm1(ds3231_handle_t *handle, ds3231_state_t enable){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_CONTROL, &reg);
    if (ret != ESP_OK) return ret;

    if (enable == DS3231_ENABLED)
        reg |=  (1 << DS3231_CONTROL_A1IE);
    else
        reg &= ~(1 << DS3231_CONTROL_A1IE);

    ret = reg_write(handle, DS3231_ADDR_CONTROL, reg);
    if (ret != ESP_OK) return ret;

    /* Switch INT/SQW pin to interrupt mode when enabling */
    if (enable == DS3231_ENABLED) {
        ret = ds3231_set_output_mode(handle, DS3231_ALARM_INTERRUPT);
    }
    return ret;
}

esp_err_t ds3231_is_alarm1_triggered(ds3231_handle_t *handle, bool *triggered){
    if (!handle || !triggered) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_STATUS, &reg);
    if (ret != ESP_OK) return ret;

    *triggered = (reg & (1 << DS3231_STATUS_A1F)) != 0;
    return ESP_OK;
}

esp_err_t ds3231_clear_alarm1_flag(ds3231_handle_t *handle){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_STATUS, &reg);
    if (ret != ESP_OK) return ret;

    reg &= ~(1 << DS3231_STATUS_A1F);
    return reg_write(handle, DS3231_ADDR_STATUS, reg);
}


/* -----------------------------------------------------------------------
 * Alarm 2
 * --------------------------------------------------------------------- */

esp_err_t ds3231_set_alarm2_time(ds3231_handle_t  *handle, const rtc_time_t *time){
    if (!handle || !time) return ESP_ERR_INVALID_ARG;

    esp_err_t ret;
    ret = reg_write(handle, DS3231_ADDR_ALM2_MIN, bin2bcd(time->minutes));
    if (ret != ESP_OK) return ret;
    return set_hour_reg(handle, DS3231_ADDR_ALM2_HRS, time->hours, time->hour_format, time->meridiem);
}


esp_err_t ds3231_set_alarm2_date(ds3231_handle_t *handle, uint8_t date){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_ALM2_DAYDATE, &reg);
    if (ret != ESP_OK) return ret;

    reg &= ~(0x3F | (1 << DS3231_DYDT));
    reg |= bin2bcd(date) & 0x3F;
    return reg_write(handle, DS3231_ADDR_ALM2_DAYDATE, reg);
}


esp_err_t ds3231_set_alarm2_day(ds3231_handle_t *handle, rtc_day_t day){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_ALM2_DAYDATE, &reg);
    if (ret != ESP_OK) return ret;

    reg &= ~0x3F;
    reg |= (1 << DS3231_DYDT);
    reg |= bin2bcd((uint8_t)day) & 0x3F;
    return reg_write(handle, DS3231_ADDR_ALM2_DAYDATE, reg);
}


esp_err_t ds3231_set_alarm2_mode(ds3231_handle_t *handle, ds3231_alarm2_mode_t mode){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret;

    /* MIN register: A2M2 */
    ret = reg_read(handle, DS3231_ADDR_ALM2_MIN, &reg); if (ret != ESP_OK) return ret;
    reg &= ~(1 << DS3231_AXMY);
    reg |=  (((mode >> 0) & 0x01) << DS3231_AXMY);
    ret = reg_write(handle, DS3231_ADDR_ALM2_MIN, reg); if (ret != ESP_OK) return ret;

    /* HRS register: A2M3 */
    ret = reg_read(handle, DS3231_ADDR_ALM2_HRS, &reg); if (ret != ESP_OK) return ret;
    reg &= ~(1 << DS3231_AXMY);
    reg |=  (((mode >> 1) & 0x01) << DS3231_AXMY);
    ret = reg_write(handle, DS3231_ADDR_ALM2_HRS, reg); if (ret != ESP_OK) return ret;

    /* DAY/DATE register: A2M4 + DY/DT */
    ret = reg_read(handle, DS3231_ADDR_ALM2_DAYDATE, &reg); if (ret != ESP_OK) return ret;
    reg &= ~((1 << DS3231_AXMY) | (1 << DS3231_DYDT));
    reg |=  (((mode >> 2) & 0x01) << DS3231_AXMY);
    reg |=  (((mode >> 3) & 0x01) << DS3231_DYDT);
    return reg_write(handle, DS3231_ADDR_ALM2_DAYDATE, reg);
}

esp_err_t ds3231_enable_alarm2(ds3231_handle_t *handle, ds3231_state_t enable){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_CONTROL, &reg);
    if (ret != ESP_OK) return ret;

    if (enable == DS3231_ENABLED)
        reg |=  (1 << DS3231_CONTROL_A2IE);
    else
        reg &= ~(1 << DS3231_CONTROL_A2IE);

    ret = reg_write(handle, DS3231_ADDR_CONTROL, reg);
    if (ret != ESP_OK) return ret;

    if (enable == DS3231_ENABLED) {
        ret = ds3231_set_output_mode(handle, DS3231_ALARM_INTERRUPT);
    }
    return ret;
}

esp_err_t ds3231_is_alarm2_triggered(ds3231_handle_t *handle, bool *triggered){
    if (!handle || !triggered) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_STATUS, &reg);
    if (ret != ESP_OK) return ret;

    *triggered = (reg & (1 << DS3231_STATUS_A2F)) != 0;
    return ESP_OK;
}


esp_err_t ds3231_clear_alarm2_flag(ds3231_handle_t *handle){
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t ret = reg_read(handle, DS3231_ADDR_STATUS, &reg);
    if (ret != ESP_OK) return ret;

    reg &= ~(1 << DS3231_STATUS_A2F);
    return reg_write(handle, DS3231_ADDR_STATUS, reg);
}