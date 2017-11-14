/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __IOT_LED_H__
#define __IOT_LED_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_DARK_LOW = 0,       /**< choose this parameter if the led is dark when the level of gpio is low */
    LED_DARK_HIGH = 1,      /**< choose this parameter if the led is dark when the level of gpio is high */
} led_dark_level_t;

typedef enum {
    LED_NORMAL_OFF,
    LED_NORMAL_ON,
    LED_QUICK_BLINK,
    LED_SLOW_BLINK,
    LED_BLINK_CUSTOM_FREQ,
} led_status_t;

typedef enum {
    LED_NORMAL_MODE,        /**< led normal */
    LED_NIGHT_MODE,         /**< led night mode, always turn off led */ 
} led_mode_t;

typedef void* led_handle_t;

/**
  * @brief  led set blink freq.
  *
  * @param  quick_blink_fre 
  * @param  slow_blink_fre
  *
  * @return
  *     - true: succeed
  *     - false: fail
  */
bool led_setup(uint8_t quick_blink_fre, uint8_t slow_blink_fre);

/**
  * @brief  create new led.
  *
  * @param  io_mux
  * @param  io_func
  * @param  io_num
  * @param  dark_level on whick level the led is dark
  *
  * @return the handle of the led
  */
led_handle_t led_create(uint8_t io_num, led_dark_level_t dark_level);

/**
  * @brief  free the memory of led
  *
  * @param  led_handle
  *
  * @return
  *     - true: succeed
  *     - false: fail
  */
bool led_delete(led_handle_t led_handle);

/**
  * @brief  set state of led
  *
  * @param  led_handle
  * @param  state refer to enum led_status_t
  *
  * @return
  *     - true: succeed
  *     - false: fail
  */
bool led_state_write(led_handle_t led_handle, led_status_t state);

/**
  * @brief  set mode of led
  *
  * @param  led_handle
  * @param  mode refer to enum led_mode_t
  *
  * @return
  *     - true: succeed
  *     - false: fail
  */
bool led_mode_write(led_handle_t led_handle, led_mode_t mode);

/**
  * @brief  set led blink with custom freqency
  *
  * @param  led_handle
  * @param  period_ms the custom period of led blink
  *
  * @return
  *     - true: succeed
  *     - false: fail
  */
bool led_blink_custom_freq(led_handle_t led_handle, uint32_t period_ms);

/**
  * @brief  get state of led
  *
  * @param  led_handle
  *
  * @return state of led
  */
led_status_t led_state_read(led_handle_t led_handle);

/**
  * @brief  get mode of led
  *
  * @param  led_handle
  *
  * @return mode of led
  */
led_mode_t led_mode_read(led_handle_t led_handle);

#ifdef __cplusplus
}
#endif

#endif

