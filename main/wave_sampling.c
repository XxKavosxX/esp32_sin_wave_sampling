/* ADC1 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "math.h"
#include <driver/dac.h>

#define DEFAULT_VREF 1050

//Due to fragmentation and to prevent heap overflow, maximum values used is 540

#define F 60

#define Fs 32400

#define N (Fs / F)

#define Ts (1.0 / Fs)

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6; //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc_unit_t unit = ADC_UNIT_1;

static void check_efuse()
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
    {
        printf("eFuse Two Point: Supported\n");
    }
    else
    {
        printf("eFuse Two Point: NOT supported\n");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
    {
        printf("eFuse Vref: Supported\n");
    }
    else
    {
        printf("eFuse Vref: NOT supported\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
    {
        printf("Characterized using Two Point Value\n");
    }
    else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        printf("Characterized using eFuse Vref\n");
    }
    else
    {
        printf("Characterized using Default Vref\n");
    }
}

void app_main()
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    //Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(channel, atten);

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    //Pull-Down enabled
    gpio_set_pull_mode(GPIO_NUM_34, GPIO_PULLDOWN_ENABLE);

    while (1)
    {
        uint32_t values[N] = {0};
        int i;

        //Wait signal cross 0
        uint32_t start_sample = 0;

        portMUX_TYPE my_spinlock = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&my_spinlock); // timing critical start
        {
            //Function <for> + <esp_adc_cal_get_voltage> delay is about 25us
            for (i = 0; i < N; i++)
            {
                esp_adc_cal_get_voltage(channel, adc_chars, &values[i]);
                //Sampling interval considering delays
                ets_delay_us(Ts - 30);
            }
        }
        portEXIT_CRITICAL(&my_spinlock); // timing critical end

        printf("F: %d, Fs: %d, N: %d \n\n", F, Fs, N);
        printf("Samples:[\n\n");
        double V_rms = 0, V_dc = 0;
        for (i = 0; i < N; i++)
        {
            printf("%d,", values[i]);
            V_rms += pow(values[i], 2);// * values[i];
            V_dc += values[i];
        }

        V_rms = sqrt(V_rms / N);
        V_dc = V_dc / N;
        printf("\n\n V_rms: %f mV, V_dc: %f mV, Diff: %f mV\n\n", V_rms, V_dc, V_rms - V_dc);
        vTaskDelay(250);
    }
}
