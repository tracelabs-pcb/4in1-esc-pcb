#include "stm32f405_regs.h"
#include "pwm_tim1.h"

#define TIM_OCM_PWM1 (0x6UL) /* OCxM = 110: PWM mode 1 */

void pwm_tim1_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

    TIM1->PSC = 0;
    TIM1->ARR = PWM_ARR_TICKS;
    TIM1->CCR1 = 0;
    TIM1->CCR2 = 0;
    TIM1->CCR3 = 0;

    /* CH1/CH2: PWM mode 1, preload enable. */
    TIM1->CCMR1 = (TIM_OCM_PWM1 << 4) | (1UL << 3)   /* OC1M, OC1PE */
                | (TIM_OCM_PWM1 << 12) | (1UL << 11); /* OC2M, OC2PE */
    /* CH3: PWM mode 1, preload enable. */
    TIM1->CCMR2 = (TIM_OCM_PWM1 << 4) | (1UL << 3);   /* OC3M, OC3PE */

    /* Active high outputs, channels enabled. */
    TIM1->CCER = (1UL << 0) | (1UL << 4) | (1UL << 8); /* CC1E, CC2E, CC3E */

    TIM1->CR1 |= TIM_CR1_ARPE;
    TIM1->EGR = TIM_EGR_UG; /* latch PSC/ARR/CCR preload registers */
    TIM1->BDTR = TIM_BDTR_MOE; /* advanced timer: main output enable required */
    TIM1->CR1 |= TIM_CR1_CEN;
}

void pwm_tim1_set_duty(pwm_channel_t ch, uint32_t duty_ticks)
{
    if (duty_ticks > PWM_ARR_TICKS) {
        duty_ticks = PWM_ARR_TICKS;
    }

    switch (ch) {
    case PWM_CH_INHC:
        TIM1->CCR1 = duty_ticks;
        break;
    case PWM_CH_INHB:
        TIM1->CCR2 = duty_ticks;
        break;
    case PWM_CH_INHA:
        TIM1->CCR3 = duty_ticks;
        break;
    }
}
