#include "system_clock.h"
#include "gpio_config.h"
#include "edl7141_spi.h"
#include "stm32f405_regs.h"

/*
 * Minimal bring-up step: talk to the 6EDL7141 over SPI and nothing
 * else. No PWM, no EN_DRV, no EXTI, no commutation - just confirm the
 * gate driver answers before building anything on top of it.
 *
 * PC13 status LED: HIGH = green (device responded correctly),
 * LOW = red (no/wrong response, check wiring).
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

    int ok = edl7141_check_device_id();
    led_set(ok);

    for (;;) {
    }
}
