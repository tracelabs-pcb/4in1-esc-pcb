#ifndef SYSTEM_CLOCK_H
#define SYSTEM_CLOCK_H

/* HSE crystal frequency fitted on the board. Adjust to match the
 * actual oscillator before building (common values: 8 MHz, 12 MHz,
 * 16 MHz). Wrong value here means every timing figure in this
 * firmware (PWM frequency, 1 MHz timestamp base, eRPM) is wrong. */
#define HSE_VALUE_HZ    8000000UL

#define SYSCLK_HZ       168000000UL
#define AHB_HZ          168000000UL   /* HPRE  = /1 */
#define APB1_HZ         42000000UL    /* PPRE1 = /4 */
#define APB2_HZ         84000000UL    /* PPRE2 = /2 */
#define APB1_TIM_HZ     (APB1_HZ * 2) /* TIM2/3/4 kernel clock: x2 when APBx presc != 1 */
#define APB2_TIM_HZ     (APB2_HZ * 2) /* TIM1 kernel clock */

void system_clock_init(void);

#endif /* SYSTEM_CLOCK_H */
