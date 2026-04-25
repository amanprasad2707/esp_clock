#include "esp_idf_version.h"
#include "ssd1306.h"


#define SDA_GPIO 21
#define SCL_GPIO 22



void app_main(void){
    SSD1306_t dev;

    // Initialize display (I2C mode)
    i2c_master_init(&dev, SDA_GPIO, SCL_GPIO, -1);

    // Initialize SSD1306
    ssd1306_init(&dev, 128, 64);

    // Clear screen
    ssd1306_clear_screen(&dev, false);

    // Display text
    ssd1306_display_text(&dev, 0, "Hello", 5, false);
    ssd1306_display_text(&dev, 1, "World", 5, false);


}