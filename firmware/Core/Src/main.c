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
 * Step 5: apply 5% duty on one phase at a time, alternating A and B
 * each cycle, ~3 seconds each, so the first real switching event can
 * be watched on a scope (GHx-SHx, SHx-GND, PA9/PA10 as trigger
 * reference, GLx-GND as the real shoot-through check). Repeats forever
 * (phase-ID blinks, then a slow "get ready" blink, then the test
 * pulse, then re-check faults, then the other phase) so there's no
 * need to race a one-shot window with the scope - arm the trigger any
 * time during the slow blink, the next test pulse is always coming.
 * Alternating instead of only phase B lets GHA be re-measured (at the
 * driver pin and at the MOSFET gate, see the handoff doc) without a
 * reflash, right after watching a known-good GHB pulse for comparison.
 *
 * LED convention (PC13), building up step by step:
 *   solid red        -> a check failed, stopped here, do not proceed
 *   blinking green   -> step 3 (EN_DRV + fault check) passed
 *   4x green blinks  -> step 4 (TIM1 PWM init @ 0% duty + fault check) passed
 *   1 short blink + pause  -> step 5: phase A is up next
 *   2 short blinks + pause -> step 5: phase B is up next
 *   slow green blink -> step 5: "get ready" window (~6s) - arm the
 *                        scope trigger now, the test pulse is coming
 *   fast green blink -> step 5 test pulse actively running (~3s)
 *   (repeats: phase-ID blinks, slow blink, fast blink, phase-ID blinks
 *   for the other phase, ... forever, unless a fault shows up, then
 *   solid red)
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
volatile uint16_t g_debug_supply_st = 0xFFFFU; /* live UVLO/OVLO status, see edl7141_spi.h */
volatile uint16_t g_debug_pwm_cfg_readback = 0xFFFFU; /* confirms PWM_MODE really is 6PWM (0x0000) */

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
     * goes high - PWM_MODE only latches while EN_DRV is low. Unlike the
     * test pattern above, this write was never read back and verified -
     * do that now, since if PWM_MODE somehow isn't really b000 (6PWM),
     * INHA wouldn't mean what we assume it means (e.g. in 1PWM mode
     * INHA alone is duty/frequency only, commutation pattern comes from
     * other pins that are all sitting at 0 - nothing would move even
     * with a perfectly clean INHA signal arriving at the chip). */
    edl7141_configure_pwm_mode();
    g_debug_pwm_cfg_readback = edl7141_read_reg(EDL7141_ADDR_PWM_CFG);
    if (g_debug_pwm_cfg_readback != EDL7141_PWM_MODE_6PWM) {
        fail_forever(); /* PWM_CFG didn't stick at 6PWM - see g_debug_pwm_cfg_readback */
    }

    /* This board has no shunt resistors on the driver's CSNx/CSOx pins
     * (confirmed unbeschaltet) - disable all 3 internal current-sense
     * amplifiers so the floating phase-B amplifier (enabled by reset
     * default) can't spuriously trip CS_OCP_FLT. See edl7141_spi.h. */
    edl7141_disable_unused_current_sense();

    gpio_en_drv_set(1);
    delay_approx_ms(20); /* let charge pumps/output stage settle before checking for faults */

    /* Release VSENSE/nBRAKE (PC1, bodge wire): the board's existing
     * pull-down resistor holds this low forever otherwise, which the
     * driver reads as a permanently asserted brake and silently ignores
     * all INHx PWM commands. See gpio_config.c for why this is safe
     * without a series diode. Must happen well after the driver's own
     * startup analog-sensing window - by this point (after clock init,
     * SPI checks, EN_DRV settle delay) it long since has. */
    gpio_nbrake_release();

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

    /* Step 5, repeating forever, alternating phase A then phase B each
     * cycle: a phase-ID blink (1 blink = A, 2 blinks = B) so the LED
     * alone tells you which phase is up without needing to already be
     * watching, then ~6s slow "get ready" blink (arm the scope trigger
     * any time in this window), then 5% duty on that phase only
     * (~420/8399 ticks) for ~3s with fast blink, then back to 0% and a
     * fault re-check before moving to the other phase. The channel not
     * under test stays at 0% duty throughout (INHC is never driven in
     * this step). */
    static const pwm_channel_t step5_channels[2] = { PWM_CH_INHA, PWM_CH_INHB };
    static const int step5_blink_counts[2] = { 1, 2 };
    int phase_idx = 0;

    for (;;) {
        pwm_channel_t ch = step5_channels[phase_idx];
        int blinks = step5_blink_counts[phase_idx];

        /* Phase-ID blinks: quick on/off, distinct from both the slow
         * "get ready" blink and the fast test-pulse blink below. */
        for (int i = 0; i < blinks; i++) {
            led_green_on();
            delay_approx_ms(150);
            led_off_hiz();
            delay_approx_ms(150);
        }
        delay_approx_ms(700); /* pause so the blink count is easy to read */

        for (int i = 0; i < 6; i++) {
            led_green_on();
            delay_approx_ms(500);
            led_off_hiz();
            delay_approx_ms(500);
        }

        pwm_tim1_set_duty(ch, (PWM_ARR_TICKS * 5U) / 100U);
        for (int i = 0; i < 30; i++) {
            /* Live UVLO/OVLO status while the pulse is actually running -
             * VCCLS/VCCHS UVLO forces Hi-Z outputs independently of
             * FAULT_ST/EN_DRV/nBRAKE, so this is worth catching mid-pulse
             * rather than only afterward. See edl7141_spi.h. */
            g_debug_supply_st = edl7141_read_reg(EDL7141_ADDR_SUPPLY_ST);
            g_debug_pwm_cfg_readback = edl7141_read_reg(EDL7141_ADDR_PWM_CFG);
            led_green_on();
            delay_approx_ms(50);
            led_off_hiz();
            delay_approx_ms(50);
        }
        pwm_tim1_set_duty(ch, 0); /* back to 0% - this phase floats again */
        delay_approx_ms(20);

        g_debug_fault_st_after_step5 = edl7141_read_reg(EDL7141_ADDR_FAULT_ST);
        if (g_debug_fault_st_after_step5 != 0x0000U) {
            gpio_en_drv_set(0);
            fail_forever(); /* step 5 failed - see g_debug_fault_st_after_step5 */
        }

        phase_idx = 1 - phase_idx; /* alternate: A, B, A, B, ... */
    }
}
