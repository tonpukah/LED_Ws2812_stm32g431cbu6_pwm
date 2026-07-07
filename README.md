WS2812 Timing Analysis for This Project
1. Clock chain: how you get to 80 MHz
SystemClock_Config() in main.c:365-406 configures the PLL from the internal 16 MHz HSI:


VCO_in  = HSI / PLLM   = 16 MHz / 1   = 16 MHz
VCO_out = VCO_in × PLLN = 16 MHz × 20  = 320 MHz
SYSCLK  = VCO_out / PLLR = 320 MHz / 2 = 160 MHz
AHB/APB1/APB2 dividers are all DIV1, so:


HCLK = PCLK1 = PCLK2 = 160 MHz
Timer clock rule: when the APB prescaler = 1, the timer kernel clock equals PCLK (no ×2 multiplier is applied). So:


TIM2CLK  = PCLK1 = 160 MHz   (TIM2 is on APB1)
TIM15CLK = PCLK2 = 160 MHz   (TIM15 is on APB2)
Both MX_TIM2_Init (main.c:428) and MX_TIM15_Init (main.c:526) set Prescaler = 1. The prescaler register divides by PSC+1:


Counter clock = TIMxCLK / (PSC+1) = 160 MHz / 2 = 80 MHz
This matches the comment in ws2812.h:7 — "timer period = 100 counts @ 80 MHz." This is correct.

2. Bit period (PWM frequency)
Period = 99 (ARR) → counts per cycle = ARR+1 = 100 counts.


T_bit = 100 / 80 MHz = 1.25 µs   →  PWM frequency = 800 kHz
This is exactly the WS2812/WS2812B bit-period spec (1.25 µs ± 600 ns). ✅

3. Duty cycle for logic 0/1 — the important part
Mode is TIM_OCMODE_PWM1 with OCPOLARITY_HIGH: the output is high while CNT < CCR, then goes low for the rest of the period. So the value of CCR sets the high time.

Symbol	CCR	High time	Low time
WS2812_BIT_LOW (logic 0)	33	33/80MHz = 0.4125 µs	1.25−0.4125 = 0.8375 µs
WS2812_BIT_HIGH (logic 1)	66	66/80MHz = 0.825 µs	1.25−0.825 = 0.425 µs
Compare to the WS2812B datasheet:


T0H = 0.40 µs, T0L = 0.85 µs
T1H = 0.85 µs, T1L = 0.40 µs   (±150 ns tolerance)
Computed values (0.4125/0.8375 and 0.825/0.425) are within ~12–25 ns of the datasheet nominal — comfortably inside the ±150 ns tolerance. This timing is correctly tuned, and the 12.5 ns resolution (1/80 MHz) is fine-grained enough that there's no meaningful quantization error.

4. Reset/latch pulse
WS2812_RESET_LEN = 50 samples of value 0 (CCR=0 → line held low for the whole period):


T_reset = 50 × 1.25 µs = 62.5 µs
The original WS2812 spec only requires ≥50 µs low to latch — satisfied. Caveat: many WS2812B datasheet revisions specify ≥280 µs reset. 62.5 µs works on most WS2812B chips in practice (it's a widely used value), but if you see the first few LEDs flicker/glitch on refresh with a particular reel of LEDs, bumping WS2812_RESET_LEN to ~240 (→300 µs) is the standard fix.

5. Bit encoding correctness
In encode_color() (ws2812.c:9-27):


color_raw = (G << 16) | (R << 8) | B;
for (bit = 23; bit >= 0; bit--) ...
This sends G, then R, then B, each MSB-first — i.e., GRB byte order with MSB-first bit order. That matches the WS2812/WS2812B protocol exactly (it is not RGB — a common bug in naive implementations, but this project has it right).

6. DMA delivery of the waveform
TIM15_CH1 → DMA1_Channel1, TIM2_CH4 → DMA1_Channel2 (stm32g4xx_hal_msp.c:109,156), each with MemDataAlignment = HALFWORD matching the uint16_t ledbuffer[].
TIM2's PeriphDataAlignment = WORD while TIM15's is HALFWORD — this looks inconsistent at first glance, but it's actually correct: TIM2 is a 32-bit timer (CCR4 is a 32-bit register) while TIM15 is a 16-bit timer (CCR1 is 16-bit). CubeMX sized each DMA's peripheral-side width to match the real register width; the DMA hardware zero-extends the 16-bit source into the 32-bit CCR4 write automatically, so no bug there.
The cast (const uint32_t *)ws->ledbuffer in WS2812_Update() (ws2812.c:70) only matches HAL_TIM_PWM_Start_DMA's signature — internally the address is just handed to the DMA's source-address register, and the actual transfer width is governed by MemDataAlignment (halfword), so the buffer is read correctly as uint16_t elements despite the pointer cast.
On DMA transfer-complete, HAL_TIM_PWM_PulseFinishedCallback → WS2812_DMACompleteCallback() stops PWM+DMA (ws2812.c:77-87), so the line is left low between updates (TIM15 explicitly forces OCIdleState = RESET on stop; TIM2 relies on the last buffered samples already being 0).
7. Frame time
For 200 LEDs/channel: 200 × 24 + 50 = 4850 samples → 4850 × 1.25 µs ≈ 6.06 ms per full-strip update, driven entirely by DMA (no CPU busy-wait). Channel A (TIM15) and Channel B (TIM2) run on independent timers/DMA channels, so both strips update concurrently.

Summary
The clock math is internally consistent (160 MHz → /2 prescaler → 80 MHz counter → 100-count period = 1.25 µs bit time), and the chosen CCR values (33/66) reproduce WS2812B's 0.4/0.85 µs and 0.85/0.4 µs high/low times almost exactly. The only thing worth double-checking on real hardware is the 62.5 µs reset pulse — increase WS2812_RESET_LEN if you see glitches on a strip that needs the longer ≥280 µs latch.
