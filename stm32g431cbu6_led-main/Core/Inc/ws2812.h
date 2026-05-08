#ifndef __WS2812_H
#define __WS2812_H

#include "stm32g4xx_hal.h"
#include <string.h>

/* ---- WS2812 timing constants (timer period = 100 counts @ 80 MHz) ---- */
#define WS2812_MAX_LEDS       250
#define WS2812_LED_DATA_SIZE  24      /* 24 bits per LED (GRB)            */
#define WS2812_RESET_LEN      50      /* 50 * 1.25 us = 62.5 us reset    */
#define WS2812_BIT_HIGH       66      /* 2/3 of 100  (logic 1)           */
#define WS2812_BIT_LOW        33      /* 1/3 of 100  (logic 0)           */

#define WS2812_BUF_SIZE  (WS2812_MAX_LEDS * WS2812_LED_DATA_SIZE + WS2812_RESET_LEN)

/* ---- Single unified type using uint16_t buffer (works for both TIM15 and TIM2) ---- */
typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t  channel;
    uint8_t   num_led;
    uint8_t   R, G, B;
    uint16_t  ledbuffer[WS2812_BUF_SIZE];
} WS2812_t;

/* ---- Public API ---- */
void WS2812_Init(WS2812_t *ws, TIM_HandleTypeDef *htim,
                 uint32_t channel, uint8_t num_led);
void WS2812_SetColor(WS2812_t *ws, uint8_t R, uint8_t G, uint8_t B);
void WS2812_Update(WS2812_t *ws);

void WS2812_DMACompleteCallback(TIM_HandleTypeDef *htim);

#endif /* __WS2812_H */
