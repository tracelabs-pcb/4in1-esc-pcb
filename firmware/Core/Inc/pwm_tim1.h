#ifndef PWM_TIM1_H
#define PWM_TIM1_H

#include <stdint.h>

/* TIM1 channel <-> gate driver INHx mapping (see gpio_config.h):
 *   CH1 (PA8)  -> INHC
 *   CH2 (PA9)  -> INHB
 *   CH3 (PA10) -> INHA
 */
typedef enum {
    PWM_CH_INHC = 1,
    PWM_CH_INHB = 2,
    PWM_CH_INHA = 3,
} pwm_channel_t;

#define PWM_FREQ_HZ   20000UL
#define PWM_ARR_TICKS 8399UL   /* 168 MHz / (PWM_ARR_TICKS + 1) = 20 kHz */

void pwm_tim1_init(void);

/* duty_ticks in [0, PWM_ARR_TICKS]. 0 = INHx permanently low (phase off). */
void pwm_tim1_set_duty(pwm_channel_t ch, uint32_t duty_ticks);

#endif /* PWM_TIM1_H */
