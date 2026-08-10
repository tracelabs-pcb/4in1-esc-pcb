#ifndef GPIO_CONFIG_H
#define GPIO_CONFIG_H

/*
 * Pin assignment for Motor 1 (identical layout on Motors 2-4, on the
 * other TIM/EXTI/SPI instances of the STM32F405RGT6):
 *
 *   PA8  (TIM1_CH1, AF1)  -> 6EDL7141 INHC
 *   PA9  (TIM1_CH2, AF1)  -> 6EDL7141 INHB
 *   PA10 (TIM1_CH3, AF1)  -> 6EDL7141 INHA
 *
 *   PB3  (SPI1_SCK,  AF5) -> 6EDL7141 SCLK
 *   PB4  (SPI1_MISO, AF5) -> 6EDL7141 SDO
 *   PB5  (SPI1_MOSI, AF5) -> 6EDL7141 SDI
 *   PB12 (GPIO output)    -> 6EDL7141 nCS (software-managed, not SPI1_NSS)
 *
 *   PB6  (EXTI6, floating input) <- LM2901 comparator A out (phase A zero-cross)
 *   PB7  (EXTI7, floating input) <- LM2901 comparator B out (phase B zero-cross)
 *   PB8  (EXTI8, floating input) <- LM2901 comparator C out (phase C zero-cross)
 */
void gpio_config_init(void);

#endif /* GPIO_CONFIG_H */
