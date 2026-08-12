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
 * Confirmed: PB6/PB7/PB8 = LM2901 comparator 1/2/3 outputs (EXTI6/7/8).
 * Assumed, NOT yet confirmed against the schematic: comparator 1 -> Phase A,
 * comparator 2 -> Phase B, comparator 3 -> Phase C, i.e. which motor
 * phase feeds IN+ of comparator 1 vs 2 vs 3. This only matters for
 * commutation_handoff_to_closed_loop() (open-loop spin-up in main.c
 * does not use these comparators at all). If closed-loop commutation
 * stalls/judders right after handoff, this mapping (or the
 * expect_rising direction below) is the first thing to check - swap
 * the *_LINE defines to match the real net names.
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

typedef enum {
    COMMUTATION_MODE_OPEN_LOOP = 0,
    COMMUTATION_MODE_CLOSED_LOOP = 1,
} commutation_mode_t;

static volatile uint8_t  s_step = 0;
static volatile uint32_t s_duty_ticks = 0;
static volatile uint32_t s_last_zc_time = 0;
static volatile uint32_t s_last_period_ticks = 0;
static volatile uint8_t  s_have_period = 0;
static volatile uint8_t  s_first_edge_seen = 0;
static volatile commutation_mode_t s_mode = COMMUTATION_MODE_OPEN_LOOP;
static volatile uint32_t s_open_loop_step_ticks = 0;

volatile uint32_t g_debug_checkpoint = 0;
volatile uint32_t g_debug_ramp_i = 0xFFFFFFFFUL;
volatile uint32_t g_debug_tim3_count = 0;
volatile uint32_t g_debug_exti_count = 0;

static void apply_step(uint8_t step)
{
    for (uint8_t ch = PWM_CH_INHC; ch <= PWM_CH_INHA; ch++) {
        pwm_tim1_set_duty((pwm_channel_t) ch,
                           (ch == STEP_TABLE[step].pwm_phase) ? s_duty_ticks : 0);
    }
}

static void tim3_schedule_delay(uint32_t delay_ticks)
{
    if (delay_ticks == 0) {
        delay_ticks = 1;
    } else if (delay_ticks > TIM3_ARR_MAX) {
        delay_ticks = TIM3_ARR_MAX;
    }

    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM3->CNT = 0;
    TIM3->ARR = delay_ticks;
    TIM3->SR = 0;
    TIM3->CR1 |= TIM_CR1_CEN; /* one-pulse mode: runs once, CEN self-clears */
}

/* Calibrated busy-wait, same technique/constant as main.c's
 * delay_approx_ms() (proven reliable across every bring-up step so
 * far - PWM timing, LED timing). Originally this used the free-running
 * TIM2 timestamp (zero_cross_now()), but that hung forever during the
 * open-loop align delay in real testing (TIM2 apparently not counting -
 * root cause not yet found, TIM2 had never actually been exercised
 * before this). zero_cross_now()/TIM2 stay initialized for later BEMF
 * period measurement (closed-loop only, not used by the open-loop path
 * at all), but are no longer load-bearing here. */
#define US_LOOP_COUNT 17UL /* ~APPROX_MS_LOOP_COUNT/1000 at ~168MHz, see main.c */

static void delay_us(uint32_t us)
{
    for (volatile uint32_t i = 0; i < us * US_LOOP_COUNT; i++) {
    }
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

void commutation_step_advance(void)
{
    s_step = (uint8_t) ((s_step + 1) % 6);
    apply_step(s_step);
}

void commutation_open_loop_start(uint32_t align_duty_ticks,
                                  uint32_t align_time_us,
                                  uint32_t ramp_start_step_us,
                                  uint32_t ramp_end_step_us,
                                  uint32_t ramp_steps,
                                  uint32_t run_duty_ticks)
{
    s_mode = COMMUTATION_MODE_OPEN_LOOP;

    /* Align: hold step 0 so the rotor settles into a known position. */
    s_duty_ticks = align_duty_ticks;
    s_step = 0;
    apply_step(s_step);
    delay_us(align_time_us);
    g_debug_checkpoint = 100; /* align done, entering ramp */

    /* Ramp: fixed MCU-timed steps, linearly speeding up, no BEMF. */
    for (uint32_t i = 0; i < ramp_steps; i++) {
        g_debug_ramp_i = i;
        s_step = (uint8_t) ((s_step + 1) % 6);
        apply_step(s_step);

        /* Plain 32-bit arithmetic on purpose (not uint64_t): the Cortex-M4
         * has no hardware 64-bit divider, a 64-bit division here would
         * need a libgcc soft-division helper, and this project links
         * with -nostdlib. Values are small enough (step times are tens
         * of thousands of us, ramp_steps in the low hundreds) that the
         * intermediate product never gets close to overflowing 32 bits. */
        uint32_t step_us = ramp_start_step_us
            - ((ramp_start_step_us - ramp_end_step_us) * i) / ramp_steps;
        delay_us(step_us);
    }

    /* Ramp done: apply the cruise duty at whatever step the ramp ended
     * on, then return. Cruise stepping itself is NOT driven from here
     * (no TIM3 IRQ armed) - see commutation_step_advance() and the
     * caller (main.c's cruise loop), which calls it from a plain
     * polled loop instead. This was originally TIM3-interrupt-driven,
     * but real hardware testing hit a HardFault (NOCP, with a garbage
     * PC/LR pointing nowhere sensible - not a real coprocessor
     * instruction anywhere in the build) shortly after the first TIM3
     * IRQ fired, root cause not yet found. Polling from main() sidesteps
     * that entirely for open-loop; TIM3/EXTI still get set up in
     * commutation_init() for whenever the interrupt-driven path (needed
     * for real closed-loop BEMF timing) gets debugged. */
    g_debug_ramp_i = ramp_steps; /* ramp completed fully */
    g_debug_checkpoint = 900;    /* ramp done, cruise duty about to apply */
    s_duty_ticks = run_duty_ticks;
    apply_step(s_step);
    g_debug_checkpoint = 901; /* commutation_open_loop_start() about to return */
}

void commutation_handoff_to_closed_loop(void)
{
    s_first_edge_seen = 0;
    s_have_period = 0;
    EXTI->PR = (1UL << 6) | (1UL << 7) | (1UL << 8); /* clear stale pending from open-loop switching noise */
    s_mode = COMMUTATION_MODE_CLOSED_LOOP;
    EXTI->IMR |= (1UL << 6) | (1UL << 7) | (1UL << 8); /* only now unmask comparator interrupts (see zero_cross_exti_init) */
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
    g_debug_exti_count++;

    if (s_mode != COMMUTATION_MODE_CLOSED_LOOP) {
        return; /* still in open-loop ramp/cruise: ignore comparator edges entirely */
    }

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
    g_debug_tim3_count++;
    commutation_step_advance();

    if (s_mode == COMMUTATION_MODE_OPEN_LOOP) {
        tim3_schedule_delay(s_open_loop_step_ticks); /* keep cruising at a fixed rate */
    }
}
