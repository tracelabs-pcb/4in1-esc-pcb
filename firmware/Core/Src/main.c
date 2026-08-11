#include "system_clock.h"
#include "gpio_config.h"
#include "edl7141_spi.h"
#include "pwm_tim1.h"
#include "stm32f405_regs.h"

/*
 * Minimal bring-up step 4: initialize TIM1/PWM (PA8/9/10 -> INHC/B/A)
 * with all three channels held at 0% duty, then re-check FAULT_ST.
 *
 * Still safe: at 0% duty, INHx=0 on all channels, and with INLx
 * hardwired to GND in 6PWM mode that means GHx=LOW/GLx=LOW/SHx=High-Z
 * on all three phases (see the long comment in edl7141_spi.h) - no
 * MOSFET gets driven at all. This step only tests that TIM1 itself
 * starts cleanly and doesn't provoke a new fault (e.g. extra load on
 * the buck regulator), not that anything moves.
 *
 * Step 5: apply 5% duty on phase A (PWM_CH_INHA) only, for ~3 seconds,
 * so the first real switching event can be watched on a scope
 * (GHA-SHA, SHA-GND, PA10 as trigger reference, GLA-GND as the real
 * shoot-through check). Auto-stops back to 0% after ~3s regardless of
 * what's observed, then re-checks FAULT_ST.
 *
 * LED convention (PC13), building up step by step:
 *   solid red       -> a check failed, stopped here, do not proceed
 *   blinking green  -> step 3 (EN_DRV + fault check) passed
 *   4x green blinks -> step 4 (TIM1 PWM init @ 0% duty + fault check) passed
 *   fast green blink-> step 5 test pulse actively running (~3s)
 *   solid green     -> step 5 passed, test pulse finished cleanly
 */

#define APPROX_MS_LOOP_COUNT 16800U /* crude, uncalibrated busy-wait unit at ~168MHz */

static void led_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER = (GPIOC->MODER & ~(0x3UL << (13 * 2))) | (0x1UL << (13 * 2)); /* PC13 output */
    GPIOC->PUPDR &= ~(0x3UL << (13 * 2));
}

static void led_set(int high)
{
    GPIOC->BSRR = high ? (1UL << 13) : (1UL << (13 + 16));
}

/* Bicolor LED on a single pin (HIGH=green, LOW=red) has no "off" via
 * BSRR alone - LOW just lights red. For a real green/dark blink,
 * switch PC13 to input (Hi-Z) instead of driving it low: no current
 * through either LED die, so it goes dark rather than red. */
static void led_off_hiz(void)
{
    GPIOC->MODER &= ~(0x3UL << (13 * 2)); /* PC13 -> input */
}

static void led_green_on(void)
{
    GPIOC->MODER = (GPIOC->MODER & ~(0x3UL << (13 * 2))) | (0x1UL << (13 * 2)); /* PC13 -> output */
    GPIOC->BSRR = (1UL << 13); /* drive high = green */
}

static void delay_approx_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < APPROX_MS_LOOP_COUNT; j++) {
        }
    }
}

/* Exposed for the debugger: raw FAULT_ST (address 0x00) reads, in case
 * the pass/fail LED needs a second look.
 *   g_debug_fault_st_before_clear - right after EN_DRV, before clearing
 *   g_debug_fault_st              - after writing FAULTS_CLR (the one
 *                                    that actually decides pass/fail) */
volatile uint16_t g_debug_fault_st_before_clear = 0xFFFFU;
volatile uint16_t g_debug_fault_st = 0xFFFFU;
volatile uint16_t g_debug_fault_st_after_pwm_init = 0xFFFFU; /* step 4 */
volatile uint16_t g_debug_fault_st_after_step5 = 0xFFFFU;    /* step 5 */

static void fail_forever(void)
{
    led_set(0);
    for (;;) {
    }
}

int main(void)
{
    system_clock_init();
    gpio_config_init(); /* sets up SPI1 (PB3/4/5), nCS (PB12), EN_DRV (PB2, starts LOW) */
    led_init();

    edl7141_spi_init();

    if (!edl7141_check_device_id()) {
        fail_forever(); /* step 1 (SPI read) failed */
    }

    uint16_t test_pattern = 0x0020U; /* BRAKE_CFG=b10 (High-Z), PWM_MODE=b000 (6PWM) */
    edl7141_write_reg(EDL7141_ADDR_PWM_CFG, test_pattern);
    if (edl7141_read_reg(EDL7141_ADDR_PWM_CFG) != test_pattern) {
        fail_forever(); /* step 2 (SPI write) failed */
    }

    /* Leave PWM_CFG in the real desired end state (6PWM) before EN_DRV
     * goes high - PWM_MODE only latches while EN_DRV is low. */
    edl7141_configure_pwm_mode();

    /* This board has no shunt resistors on the driver's CSNx/CSOx pins
     * (confirmed unbeschaltet) - disable all 3 internal current-sense
     * amplifiers so the floating phase-B amplifier (enabled by reset
     * default) can't spuriously trip CS_OCP_FLT. See edl7141_spi.h. */
    edl7141_disable_unused_current_sense();

    gpio_en_drv_set(1);
    delay_approx_ms(20); /* let charge pumps/output stage settle before checking for faults */

    /* FAULT_ST bits stay set once tripped until explicitly cleared,
     * even after the triggering condition is gone (e.g. a one-off
     * startup transient). Record the raw pre-clear value for
     * debugging, then clear and re-read to see the real ongoing state. */
    g_debug_fault_st_before_clear = edl7141_read_reg(EDL7141_ADDR_FAULT_ST);
    edl7141_clear_faults();
    delay_approx_ms(5);

    g_debug_fault_st = edl7141_read_reg(EDL7141_ADDR_FAULT_ST);
    if (g_debug_fault_st != 0x0000U) {
        gpio_en_drv_set(0); /* something's wrong, disable the driver stage again */
        fail_forever();     /* step 3 (EN_DRV + fault check) failed - see g_debug_fault_st */
    }

    /* Step 3 passed: a few green/dark blinks as a visual checkpoint,
     * then move on to step 4. */
    for (int i = 0; i < 4; i++) {
        led_green_on();
        delay_approx_ms(300);
        led_off_hiz();
        delay_approx_ms(300);
    }

    /* Step 4: TIM1 PWM init, all channels at 0% duty (see the comment
     * at the top of this file for why that's still safe), then
     * re-check for any newly provoked fault. */
    pwm_tim1_init();
    delay_approx_ms(20);

    g_debug_fault_st_after_pwm_init = edl7141_read_reg(EDL7141_ADDR_FAULT_ST);
    if (g_debug_fault_st_after_pwm_init != 0x0000U) {
        gpio_en_drv_set(0);
        fail_forever(); /* step 4 failed - see g_debug_fault_st_after_pwm_init */
    }

    /* Step 4 passed: brief solid green, then move on to step 5. */
    led_green_on();
    delay_approx_ms(1000);
    led_off_hiz();
    delay_approx_ms(300);

    /* Step 5: 5% duty on phase A only (~420/8399 ticks), ~3 seconds,
     * fast-blinking green while it runs so it's obvious on the board
     * that the test is live. INHB/INHC stay at 0% (untouched). */
    pwm_tim1_set_duty(PWM_CH_INHA, (PWM_ARR_TICKS * 5U) / 100U);
    for (int i = 0; i < 30; i++) {
        led_green_on();
        delay_approx_ms(50);
        led_off_hiz();
        delay_approx_ms(50);
    }
    pwm_tim1_set_duty(PWM_CH_INHA, 0); /* back to 0% - phase A floats again */
    delay_approx_ms(20);

    g_debug_fault_st_after_step5 = edl7141_read_reg(EDL7141_ADDR_FAULT_ST);
    if (g_debug_fault_st_after_step5 != 0x0000U) {
        gpio_en_drv_set(0);
        fail_forever(); /* step 5 failed - see g_debug_fault_st_after_step5 */
    }

    /* Step 5 passed: solid green forever. */
    led_green_on();
    for (;;) {
    }
}
