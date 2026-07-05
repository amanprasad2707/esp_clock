#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

/* -----------------------------------------------------------------------
 * Timing / address
 * --------------------------------------------------------------------- */
#define DS3231_TIMEOUT_MS         100
#define DS3231_I2C_ADDRESS        0x68

/* -----------------------------------------------------------------------
 * Register map
 * --------------------------------------------------------------------- */
#define DS3231_ADDR_SEC           0x00
#define DS3231_ADDR_MIN           0x01
#define DS3231_ADDR_HRS           0x02
#define DS3231_ADDR_DAY           0x03
#define DS3231_ADDR_DATE          0x04
#define DS3231_ADDR_MONTH         0x05
#define DS3231_ADDR_YEAR          0x06

#define DS3231_ADDR_ALM1_SEC      0x07
#define DS3231_ADDR_ALM1_MIN      0x08
#define DS3231_ADDR_ALM1_HRS      0x09
#define DS3231_ADDR_ALM1_DAYDATE  0x0A

#define DS3231_ADDR_ALM2_MIN      0x0B
#define DS3231_ADDR_ALM2_HRS      0x0C
#define DS3231_ADDR_ALM2_DAYDATE  0x0D

#define DS3231_ADDR_CONTROL       0x0E
#define DS3231_ADDR_STATUS        0x0F
#define DS3231_ADDR_AGING         0x10
#define DS3231_ADDR_TEMP_MSB      0x11
#define DS3231_ADDR_TEMP_LSB      0x12

/* -----------------------------------------------------------------------
 * Status register bits
 * --------------------------------------------------------------------- */
#define DS3231_STATUS_A1F         0
#define DS3231_STATUS_A2F         1
#define DS3231_STATUS_BSY         2
#define DS3231_STATUS_EN32KHZ     3
#define DS3231_STATUS_OSF         7

/* -----------------------------------------------------------------------
 * Control register bits
 * --------------------------------------------------------------------- */
#define DS3231_CONTROL_A1IE       0
#define DS3231_CONTROL_A2IE       1
#define DS3231_CONTROL_INTCN      2
#define DS3231_CONTROL_RS1        3
#define DS3231_CONTROL_RS2        4
#define DS3231_CONTROL_CONV       5
#define DS3231_CONTROL_BBSQW      6
#define DS3231_CONTROL_EOSC       7

/* -----------------------------------------------------------------------
 * Alarm mask / mode bit positions inside each alarm register
 * --------------------------------------------------------------------- */
#define DS3231_AXMY               7   /* A1Mx / A2Mx mask bits */
#define DS3231_DYDT               6   /* DY/DT select bit in day/date reg */

/* -----------------------------------------------------------------------
 * Enumerations
 * --------------------------------------------------------------------- */
typedef enum {
    SUNDAY = 1,
    MONDAY,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY
} rtc_day_t;

typedef enum {
    HOUR_FORMAT_24 = 0,
    HOUR_FORMAT_12
} rtc_hour_format_t;

typedef enum {
    AM = 0,
    PM
} rtc_meridiem_t;

typedef enum {
    DS3231_DISABLED = 0,
    DS3231_ENABLED
} ds3231_state_t;

typedef enum {
    DS3231_FREQ_1HZ    = 0,
    DS3231_FREQ_1024HZ = 1,
    DS3231_FREQ_4096HZ = 2,
    DS3231_FREQ_8192HZ = 3
} ds3231_sqwave_freq_t;

typedef enum {
    DS3231_OUTPUT_SQUARE_WAVE = 0,
    DS3231_ALARM_INTERRUPT    = 1
} ds3231_output_mode_t;

typedef enum {
    DS3231_ALM1_EVERY_SEC              = 0x0F,
    DS3231_ALM1_MATCH_SEC              = 0x0E,
    DS3231_ALM1_MATCH_SEC_MIN          = 0x0C,
    DS3231_ALM1_MATCH_SEC_MIN_HRS      = 0x08,
    DS3231_ALM1_MATCH_SEC_MIN_HRS_DATE = 0x00,
    DS3231_ALM1_MATCH_SEC_MIN_HRS_DAY  = 0x10,
} ds3231_alarm1_mode_t;

typedef enum {
    DS3231_ALM2_EVERY_MIN          = 0x07,
    DS3231_ALM2_MATCH_MIN          = 0x06,
    DS3231_ALM2_MATCH_MIN_HRS      = 0x04,
    DS3231_ALM2_MATCH_MIN_HRS_DATE = 0x00,
    DS3231_ALM2_MATCH_MIN_HRS_DAY  = 0x08,
} ds3231_alarm2_mode_t;

/* -----------------------------------------------------------------------
 * Data structures
 * --------------------------------------------------------------------- */
typedef struct {
    uint8_t  date;
    uint8_t  month;
    uint16_t year;
    uint8_t  day;   /* rtc_day_t value */
} rtc_date_t;

typedef struct {
    uint8_t           seconds;
    uint8_t           minutes;
    uint8_t           hours;
    rtc_hour_format_t hour_format;
    rtc_meridiem_t    meridiem;   /* ignored in HOUR_FORMAT_24 */
} rtc_time_t;


typedef struct {
    rtc_time_t time;           /* H:M:S, format, meridiem */
    ds3231_alarm1_mode_t mode; /* hardware match mode */
} ds3231_alarm1_t;

typedef struct {
    rtc_time_t time;
    ds3231_alarm2_mode_t mode;
} ds3231_alarm2_t;


/**
 * @brief Opaque driver handle.
 *
 * Allocate on the stack or in BSS; initialise with ds3231_init().
 */
typedef struct {
    i2c_master_dev_handle_t dev_handle;
} ds3231_handle_t;

/* -----------------------------------------------------------------------
 * Initialisation
 * --------------------------------------------------------------------- */

/**
 * @brief Attach a DS3231 to an existing I2C master bus.
 *
 * The bus must already be initialised with i2c_new_master_bus().
 *
 * @param bus_handle  Existing I2C master bus handle.
 * @param address     7-bit I2C address (usually DS3231_I2C_ADDRESS = 0x68).
 * @param handle      Output: populated driver handle.
 * @return            ESP_OK on success.
 *
 * @example
 *   i2c_master_bus_handle_t bus;
 *   i2c_master_bus_config_t bus_cfg = {
 *       .i2c_port    = I2C_NUM_0,
 *       .sda_io_num  = GPIO_NUM_21,
 *       .scl_io_num  = GPIO_NUM_22,
 *       .clk_source  = I2C_CLK_SRC_DEFAULT,
 *       .glitch_ignore_cnt = 7,
 *       .flags.enable_internal_pullup = true,
 *   };
 *   ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));
 *
 *   ds3231_handle_t rtc;
 *   ESP_ERROR_CHECK(ds3231_init(bus, DS3231_I2C_ADDRESS, &rtc));
 */
esp_err_t ds3231_init(i2c_master_bus_handle_t bus_handle, uint8_t address, ds3231_handle_t *handle);

/**
 * @brief Release the I2C device handle created in ds3231_init().
 */
esp_err_t ds3231_deinit(ds3231_handle_t *handle);

/* -----------------------------------------------------------------------
 * Time & date
 * --------------------------------------------------------------------- */
esp_err_t ds3231_set_time(ds3231_handle_t *handle, const rtc_time_t *time);
esp_err_t ds3231_get_time(ds3231_handle_t *handle, rtc_time_t *time);

esp_err_t ds3231_set_date(ds3231_handle_t *handle, const rtc_date_t *date);
esp_err_t ds3231_get_date(ds3231_handle_t *handle, rtc_date_t *date);

/* -----------------------------------------------------------------------
 * Temperature
 * --------------------------------------------------------------------- */

/**
 * @brief Read internal temperature (°C).
 *
 * @param handle  Driver handle.
 * @param out_c   Output temperature in degrees Celsius.
 */
esp_err_t ds3231_get_temperature(ds3231_handle_t *handle, float *out_c);

/* -----------------------------------------------------------------------
 * 32 kHz output
 * --------------------------------------------------------------------- */
esp_err_t ds3231_set_32khz_output(ds3231_handle_t *handle, ds3231_state_t state);
esp_err_t ds3231_is_32khz_output_enabled(ds3231_handle_t *handle, bool *enabled);

/* -----------------------------------------------------------------------
 * Oscillator
 * --------------------------------------------------------------------- */
esp_err_t ds3231_is_oscillator_stopped(ds3231_handle_t *handle, bool *stopped);
esp_err_t ds3231_set_oscillator(ds3231_handle_t *handle, ds3231_state_t state);

/* -----------------------------------------------------------------------
 * Square-wave / interrupt output
 * --------------------------------------------------------------------- */
esp_err_t ds3231_set_battery_backed_sqwave(ds3231_handle_t *handle, ds3231_state_t state);
esp_err_t ds3231_set_sqwave_freq(ds3231_handle_t *handle, ds3231_sqwave_freq_t freq);
esp_err_t ds3231_set_output_mode(ds3231_handle_t *handle, ds3231_output_mode_t mode);

/* -----------------------------------------------------------------------
 * Alarm 1
 * --------------------------------------------------------------------- */
esp_err_t ds3231_set_alarm1_time(ds3231_handle_t *handle, const rtc_time_t *time);
esp_err_t ds3231_set_alarm1_date(ds3231_handle_t *handle, uint8_t date);
esp_err_t ds3231_set_alarm1_day(ds3231_handle_t *handle, rtc_day_t day);
esp_err_t ds3231_set_alarm1_mode(ds3231_handle_t *handle, ds3231_alarm1_mode_t mode);
esp_err_t ds3231_enable_alarm1(ds3231_handle_t *handle, ds3231_state_t enable);
esp_err_t ds3231_is_alarm1_triggered(ds3231_handle_t *handle, bool *triggered);
esp_err_t ds3231_clear_alarm1_flag(ds3231_handle_t *handle);

/* -----------------------------------------------------------------------
 * Alarm 2
 * --------------------------------------------------------------------- */
esp_err_t ds3231_set_alarm2_time(ds3231_handle_t *handle, const rtc_time_t *time);
esp_err_t ds3231_set_alarm2_date(ds3231_handle_t *handle, uint8_t date);
esp_err_t ds3231_set_alarm2_day(ds3231_handle_t *handle, rtc_day_t day);
esp_err_t ds3231_set_alarm2_mode(ds3231_handle_t *handle, ds3231_alarm2_mode_t mode);
esp_err_t ds3231_enable_alarm2(ds3231_handle_t *handle, ds3231_state_t enable);
esp_err_t ds3231_is_alarm2_triggered(ds3231_handle_t *handle, bool *triggered);
esp_err_t ds3231_clear_alarm2_flag(ds3231_handle_t *handle);
esp_err_t ds3231_get_alarm1(ds3231_handle_t *handle, ds3231_alarm1_t *alarm);
esp_err_t ds3231_get_alarm2(ds3231_handle_t *handle, ds3231_alarm2_t *alarm);

#ifdef __cplusplus
}
#endif