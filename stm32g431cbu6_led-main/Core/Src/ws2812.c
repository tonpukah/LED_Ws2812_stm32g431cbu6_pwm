#include "ws2812.h"

/* Track active channels for DMA complete callback */
static WS2812_t *ws_active[4] = {0};

/* ------------------------------------------------------------------ */
/*  Internal: encode GRB color data into uint16_t PWM buffer          */
/* ------------------------------------------------------------------ */
static void encode_color(uint16_t *buf, uint8_t num_led,
                         uint8_t R, uint8_t G, uint8_t B)
{
    uint16_t idx = 0;
    uint32_t color_raw = ((uint32_t)G << 16) | ((uint32_t)R << 8) | B;

    for (uint8_t led = 0; led < num_led; led++)
    {
        for (int8_t bit = 23; bit >= 0; bit--)
        {
            buf[idx++] = (color_raw & (1u << bit)) ? WS2812_BIT_HIGH : WS2812_BIT_LOW;
        }
    }
    /* Reset pulse: hold low for >= 50 us */
    for (uint8_t i = 0; i < WS2812_RESET_LEN; i++)
    {
        buf[idx++] = 0;
    }
}

/* ================================================================== */
/*  Public API                                                        */
/* ================================================================== */

void WS2812_Init(WS2812_t *ws, TIM_HandleTypeDef *htim,
                 uint32_t channel, uint8_t num_led)
{
    ws->htim    = htim;
    ws->channel = channel;
    ws->num_led = (num_led > WS2812_MAX_LEDS) ? WS2812_MAX_LEDS : num_led;
    ws->R = 0;
    ws->G = 0;
    ws->B = 0;
    memset(ws->ledbuffer, 0, sizeof(ws->ledbuffer));

    /* Register in active array for callback lookup */
    for (int i = 0; i < 4; i++)
    {
        if (ws_active[i] == NULL)
        {
            ws_active[i] = ws;
            break;
        }
    }

    /* Kick one dummy DMA transfer (1 sample) to initialise the DMA link */
    HAL_TIM_PWM_Start_DMA(htim, channel, (const uint32_t *)ws->ledbuffer, 1);
}

void WS2812_SetColor(WS2812_t *ws, uint8_t R, uint8_t G, uint8_t B)
{
    ws->R = R;
    ws->G = G;
    ws->B = B;
    encode_color(ws->ledbuffer, ws->num_led, R, G, B);
}

void WS2812_Update(WS2812_t *ws)
{
    uint16_t len = (uint16_t)ws->num_led * WS2812_LED_DATA_SIZE + WS2812_RESET_LEN;
    HAL_TIM_PWM_Start_DMA(ws->htim, ws->channel,
                          (const uint32_t *)ws->ledbuffer, len);
}

/* ================================================================== */
/*  DMA transfer complete callback                                    */
/* ================================================================== */

void WS2812_DMACompleteCallback(TIM_HandleTypeDef *htim)
{
    for (int i = 0; i < 4; i++)
    {
        if (ws_active[i] && ws_active[i]->htim == htim)
        {
            HAL_TIM_PWM_Stop_DMA(htim, ws_active[i]->channel);
            return;
        }
    }
}
