#include "stm32f405_regs.h"
#include "system_clock.h"
#include "zero_cross.h"

void zero_cross_timebase_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /* TIM2 is a 32-bit counter on the F405. APB1_TIM_HZ / (83+1) = 1 MHz. */
    TIM2->PSC = (APB1_TIM_HZ / 1000000UL) - 1UL;
    TIM2->ARR = 0xFFFFFFFFUL;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CR1 |= TIM_CR1_CEN;
}

uint32_t zero_cross_now(void)
{
    return TIM2->CNT;
}

void zero_cross_exti_init(void)
{
    /* GPIO/AF routing (SYSCFG EXTICR) is done in gpio_config_init(). */
    EXTI->RTSR |= (1UL << 6) | (1UL << 7) | (1UL << 8);
    EXTI->FTSR |= (1UL << 6) | (1UL << 7) | (1UL << 8);
    EXTI->PR   = (1UL << 6) | (1UL << 7) | (1UL << 8); /* clear stale pending */
    EXTI->IMR |= (1UL << 6) | (1UL << 7) | (1UL << 8);

    NVIC->ISER[EXTI9_5_IRQn / 32] = (1UL << (EXTI9_5_IRQn % 32));
}

float zero_cross_calc_erpm(uint32_t dt_ticks_1mhz)
{
    float dt_seconds = (float) dt_ticks_1mhz / 1000000.0f;
    if (dt_seconds <= 0.0f) {
        return 0.0f;
    }
    return 60.0f / (6.0f * dt_seconds);
}
