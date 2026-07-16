#include <stddef.h>
#include "stm32f405_regs.h"
#include "system_clock.h"
#include "pwm_tim1.h"
#include "zero_cross.h"
#include "commutation.h"

/*
 * Six-step commutation table.
 *
 * pwm_phase   - the phase whose high side (INHx) is PWM-chopped this step.
 * floating_line - EXTI line (== GPIOB pin number) of the phase that is
 *                 left floating this step for BEMF sensing.
 * expect_rising  - direction of the floating phase's BEMF crossing that
 *                   is valid during this step (the other direction is
 *                   ignored as noise from the switched phases).
 *
 * Assumption (confirm against the schematic before first spin): Motor 1
 * comparator-to-pin mapping is Phase A -> PB6/EXTI6, Phase B -> PB7/EXTI7,
 * Phase C -> PB8/EXTI8, in that order. Swap the *_LINE defines below if
 * the actual net names differ.
 */
#define PHASE_A_LINE 6
#define PHASE_B_LINE 7
#define PHASE_C_LINE 8

typedef struct {
    pwm_channel_t pwm_phase;
    uint32_t floating_line;
    int expect_rising;
} commutation_step_t;

static const commutation_step_t STEP_TABLE[6] = {
    { PWM_CH_INHA, PHASE_C_LINE, 1 }, /* step 0: A -> B, C floating, rising  */
    { PWM_CH_INHA, PHASE_B_LINE, 0 }, /* step 1: A -> C, B floating, falling */
    { PWM_CH_INHB, PHASE_A_LINE, 1 }, /* step 2: B -> C, A floating, rising  */
    { PWM_CH_INHB, PHASE_C_LINE, 0 }, /* step 3: B -> A, C floating, falling */
    { PWM_CH_INHC, PHASE_B_LINE, 1 }, /* step 4: C -> A, B floating, rising  */
    { PWM_CH_INHC, PHASE_A_LINE, 0 }, /* step 5: C -> B, A floating, falling */
};

/* TIM3 shares TIM2's 1 MHz tick (see zero_cross_timebase_init/PSC calc). */
#define TIM3_ARR_MAX 0xFFFFUL

static volatile uint8_t  s_step = 0;
static volatile uint32_t s_duty_ticks = 0;
static volatile uint32_t s_last_zc_time = 0;
static volatile uint32_t s_last_period_ticks = 0;
static volatile uint8_t  s_have_period = 0;
static volatile uint8_t  s_first_edge_seen = 0;

static void apply_step(uint8_t step)
{
    for (uint8_t ch = PWM_CH_INHC; ch <= PWM_CH_INHA; ch++) {
        pwm_tim1_set_duty((pwm_channel_t) ch,
                           (ch == STEP_TABLE[step].pwm_phase) ? s_duty_ticks : 0);
    }
}

static void tim3_schedule_delay(uint32_t half_period_ticks)
{
    if (half_period_ticks == 0) {
        half_period_ticks = 1;
    } else if (half_period_ticks > TIM3_ARR_MAX) {
        half_period_ticks = TIM3_ARR_MAX;
    }

    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM3->CNT = 0;
    TIM3->ARR = half_period_ticks;
    TIM3->SR = 0;
    TIM3->CR1 |= TIM_CR1_CEN; /* one-pulse mode: runs once, CEN self-clears */
}

void commutation_init(void)
{
    pwm_tim1_init();
    zero_cross_timebase_init();
    zero_cross_exti_init();

    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    TIM3->PSC = (APB1_TIM_HZ / 1000000UL) - 1UL; /* 1 MHz tick, matches TIM2 */
    TIM3->CR1 = TIM_CR1_OPM;
    TIM3->DIER = TIM_DIER_UIE;
    NVIC->ISER[TIM3_IRQn / 32] = (1UL << (TIM3_IRQn % 32));

    s_step = 0;
    apply_step(s_step);
}

void commutation_set_duty(uint32_t duty_ticks)
{
    s_duty_ticks = duty_ticks;
    apply_step(s_step); /* re-apply so the active channel picks up the new duty immediately */
}

float commutation_get_erpm(void)
{
    if (!s_have_period) {
        return 0.0f;
    }
    return zero_cross_calc_erpm(s_last_period_ticks);
}

float commutation_get_mech_rpm(uint8_t pole_pairs)
{
    if (pole_pairs == 0) {
        return 0.0f;
    }
    return commutation_get_erpm() / (float) pole_pairs;
}

void EXTI9_5_IRQHandler(void)
{
    uint32_t pending = EXTI->PR & ((1UL << 6) | (1UL << 7) | (1UL << 8));
    if (pending == 0) {
        return;
    }
    EXTI->PR = pending; /* clear (write-1-to-clear) */

    const commutation_step_t *cur = &STEP_TABLE[s_step];
    uint32_t line_mask = (1UL << cur->floating_line);

    if ((pending & line_mask) == 0) {
        return; /* edge on a driven phase, not this step's floating phase: ignore */
    }

    int pin_is_high = (GPIOB->IDR & line_mask) != 0;
    int is_rising = pin_is_high; /* current level tells us the direction of the edge just seen */

    if ((is_rising ? 1 : 0) != cur->expect_rising) {
        return; /* right line, wrong direction for this step: ignore */
    }

    uint32_t now = zero_cross_now();

    if (s_first_edge_seen) {
        uint32_t dt = now - s_last_zc_time; /* unsigned subtraction handles 32-bit wraparound */
        s_last_period_ticks = dt;
        s_have_period = 1;
        tim3_schedule_delay(dt / 2); /* commutate 30 electrical degrees from now */
    } else {
        s_first_edge_seen = 1;
    }

    s_last_zc_time = now;
}

void TIM3_IRQHandler(void)
{
    TIM3->SR = 0;
    s_step = (s_step + 1) % 6;
    apply_step(s_step);
}
