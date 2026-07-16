#include "stm32f405_regs.h"
#include "gpio_config.h"

#define TIM1_AF   1UL
#define SPI1_AF   5UL

static void gpio_set_af(GPIO_TypeDef *port, uint32_t pin, uint32_t af)
{
    uint32_t idx = pin >> 3;          /* 0 = AFR[0] (pins 0-7), 1 = AFR[1] (pins 8-15) */
    uint32_t shift = (pin & 0x7UL) * 4UL;
    port->AFR[idx] = (port->AFR[idx] & ~(0xFUL << shift)) | (af << shift);
}

static void gpio_set_mode(GPIO_TypeDef *port, uint32_t pin, uint32_t mode)
{
    uint32_t shift = pin * 2UL;
    port->MODER = (port->MODER & ~(0x3UL << shift)) | (mode << shift);
}

static void gpio_set_speed_high(GPIO_TypeDef *port, uint32_t pin)
{
    uint32_t shift = pin * 2UL;
    port->OSPEEDR |= (0x3UL << shift); /* very high speed */
}

static void gpio_set_pupd_none(GPIO_TypeDef *port, uint32_t pin)
{
    uint32_t shift = pin * 2UL;
    port->PUPDR &= ~(0x3UL << shift); /* no pull-up/pull-down */
}

void gpio_config_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* PA8/PA9/PA10 -> TIM1_CH1/CH2/CH3, alternate function push-pull. */
    for (uint32_t pin = 8; pin <= 10; pin++) {
        gpio_set_mode(GPIOA, pin, GPIO_MODE_AF);
        gpio_set_af(GPIOA, pin, TIM1_AF);
        gpio_set_speed_high(GPIOA, pin);
        gpio_set_pupd_none(GPIOA, pin);
    }

    /* PB3/PB4/PB5 -> SPI1 SCK/MISO/MOSI, alternate function push-pull. */
    for (uint32_t pin = 3; pin <= 5; pin++) {
        gpio_set_mode(GPIOB, pin, GPIO_MODE_AF);
        gpio_set_af(GPIOB, pin, SPI1_AF);
        gpio_set_speed_high(GPIOB, pin);
        gpio_set_pupd_none(GPIOB, pin);
    }

    /* PB12 -> 6EDL7141 nCS, plain push-pull GPIO output, idle high. */
    gpio_set_mode(GPIOB, 12, GPIO_MODE_OUTPUT);
    gpio_set_speed_high(GPIOB, 12);
    gpio_set_pupd_none(GPIOB, 12);
    GPIOB->BSRR = (1UL << 12); /* nCS = 1 (deselected) */

    /* PB6/PB7/PB8 -> comparator outputs, floating input, EXTI6/7/8. */
    for (uint32_t pin = 6; pin <= 8; pin++) {
        gpio_set_mode(GPIOB, pin, GPIO_MODE_INPUT);
        gpio_set_pupd_none(GPIOB, pin);
    }

    /* Route EXTI6/7/8 to GPIO port B (SYSCFG_EXTICR2 covers EXTI4..7,
     * SYSCFG_EXTICR3 covers EXTI8..11). PB = 0b0001. */
    SYSCFG->EXTICR[1] = (SYSCFG->EXTICR[1] & ~(0xFUL << 8))  | (0x1UL << 8);  /* EXTI6 -> PB */
    SYSCFG->EXTICR[1] = (SYSCFG->EXTICR[1] & ~(0xFUL << 12)) | (0x1UL << 12); /* EXTI7 -> PB */
    SYSCFG->EXTICR[2] = (SYSCFG->EXTICR[2] & ~(0xFUL << 0))  | (0x1UL << 0);  /* EXTI8 -> PB */
}
