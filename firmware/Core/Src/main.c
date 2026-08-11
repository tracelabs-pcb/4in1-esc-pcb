#include "system_clock.h"
#include "gpio_config.h"
#include "edl7141_spi.h"
#include "stm32f405_regs.h"

/*
 * Minimal bring-up step 3: enable EN_DRV (gate driver output stage /
 * charge pumps) and check FAULT_ST over SPI - still no PWM, no EXTI,
 * TIM1/PA8-10 stay unconfigured as far as this file is concerned, so
 * the MOSFET gates see nothing from the driver's high side outputs.
 *
 * LED convention (PC13), building up step by step:
 *   solid red      -> a check failed, stopped here, do not proceed
 *   blinking green -> this step (EN_DRV + fault check) passed
 *   solid green    -> reserved for the next step after this one
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

    /* Step 3 passed: blink green forever. */
    for (;;) {
        led_set(1);
        delay_approx_ms(300);
        led_set(0);
        delay_approx_ms(300);
    }
}
