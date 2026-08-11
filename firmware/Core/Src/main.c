#include "system_clock.h"
#include "gpio_config.h"
#include "edl7141_spi.h"
#include "stm32f405_regs.h"

/*
 * Minimal bring-up step 2: confirm SPI *writes* reach the 6EDL7141 too
 * (step 1 only proved reads work), then leave PWM_CFG in the real
 * desired 6PWM state. Still no EN_DRV, no PWM, no EXTI - the gate
 * driver's output stage stays disabled, MOSFETs are not involved at all.
 *
 * PC13 status LED: HIGH = green (device ID read AND a distinct SPI
 * write/read-back both checked out), LOW = red (something failed).
 */

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

int main(void)
{
    system_clock_init();
    gpio_config_init(); /* sets up SPI1 (PB3/4/5) and nCS (PB12) among other things */
    led_init();

    edl7141_spi_init();

    int id_ok = edl7141_check_device_id();

    /* Write a distinct, harmless test pattern to PWM_CFG (BRAKE_CFG =
     * b10 "High-Z/no power", PWM_MODE = b000 "6PWM") and read it back.
     * Using a non-zero value here (rather than testing with 6PWM's
     * 0x0000 directly) proves the write path actually works, since
     * 0x0000 is also PWM_CFG's power-on-reset value and would read
     * back the same even if the write silently did nothing. */
    uint16_t test_pattern = 0x0020U;
    edl7141_write_reg(EDL7141_ADDR_PWM_CFG, test_pattern);
    int write_ok = (edl7141_read_reg(EDL7141_ADDR_PWM_CFG) == test_pattern);

    /* Leave PWM_CFG in the real desired end state (6PWM, see the long
     * comment in edl7141_spi.h) regardless of the test above. */
    edl7141_configure_pwm_mode();

    led_set(id_ok && write_ok);

    for (;;) {
    }
}
