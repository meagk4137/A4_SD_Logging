#include <stdio.h>
#include <string.h>
#include <math.h>
 
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "ff.h"
#include "tf_card.h"
 
#define PHOTO_CELL_PIN 27
#define PHOTO_CELL_ADC 1
#define TEMP_PIN 26
#define TEMP_ADC 0
#define LED_PIN 15
 
#define R2_OHMS 7500.0f
#define LUX_B 7.27f
#define ADC_VREF 3.3f
#define ADC_RESOLUTION (1 << 12)
 
static pico_fatfs_spi_config_t sd_config = {
    spi0,
    CLK_SLOW_DEFAULT,
    12500 * KHZ,
    20,
    21,
    18,
    19,
    true
};
 
int main(void) {
    stdio_init_all();
    sleep_ms(2000);
 
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
 
    adc_init();
    adc_gpio_init(PHOTO_CELL_PIN);
    adc_gpio_init(TEMP_PIN);
 
    const float conversion_factor = ADC_VREF / ADC_RESOLUTION;
 
    pico_fatfs_set_config(&sd_config);
 
    FATFS fs;
    FIL fil;
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        printf("f_mount failed: %d\r\n", fr);
        while (true) {
            sleep_ms(1000);
        }
    }
 
    fr = f_open(&fil, "log.csv", FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("f_open failed: %d\r\n", fr);
        while (true) {
            sleep_ms(1000);
        }
    }
 
    UINT bw = 0;
    const char *header = "timestamp_s,lux,temp_c\r\n";
    f_write(&fil, header, strlen(header), &bw);
    f_sync(&fil);
 
    printf("Logging started.\r\n");
 
    while (true) {
        adc_select_input(PHOTO_CELL_ADC);
        uint32_t sum = 0;
        for (int i = 0; i < 16; i++) {
            sum += adc_read();
        }
 
        float avg_raw = sum / 16.0f;
        float volts = avg_raw * conversion_factor;
 
        adc_select_input(TEMP_ADC);
        uint16_t temp_raw = adc_read();
        float t_volts = temp_raw * conversion_factor;
        float temp_c = -1481.96f + sqrtf(2.1962e6f + (1.8639f - t_volts) / 3.88e-6f);
 
        float lux = 0.0f;
        if (volts > 0.0f) {
            float r1 = (ADC_VREF * R2_OHMS) / volts - R2_OHMS;
            if (r1 > 0.0f) {
                lux = powf(10.0f, -1.67f * log10f(r1) + LUX_B);
            }
        }
 
        gpio_put(LED_PIN, lux < 5.0f ? 1 : 0);
 
        float timestamp_s = to_ms_since_boot(get_absolute_time()) / 1000.0f;
        char line[64];
        int len = snprintf(line, sizeof(line), "%.3f, %.1f, %.1f\r\n", timestamp_s, lux, temp_c);
 
        printf("%s", line);
        f_write(&fil, line, len, &bw);
        f_sync(&fil);
 
        sleep_ms(1000);
    }
 
    f_close(&fil);
    f_unmount("");
    return 0;
}
