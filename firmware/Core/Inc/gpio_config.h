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
 *   PB6  (EXTI6, floating input) <- LM2901 comparator 1 out (phase A zero-cross, assumed)
 *   PB7  (EXTI7, floating input) <- LM2901 comparator 2 out (phase B zero-cross, assumed)
 *   PB8  (EXTI8, floating input) <- LM2901 comparator 3 out (phase C zero-cross, assumed)
 *
 *   PB2  (GPIO output)     -> 6EDL7141 EN_DRV (idle LOW at boot; user
 *                              confirmed CE is hardwired high via pull-up,
 *                              so power-supply start-up needs no MCU
 *                              action, only EN_DRV/gate-driver stage does)
 *
 *   PC1  (GPIO output)     -> 6EDL7141 VSENSE/nBRAKE (bodged in addition
 *                              to the board's existing pull-down resistor,
 *                              no series diode - see the long comment in
 *                              gpio_config.c). Idle LOW at boot (brake
 *                              asserted, harmless before EN_DRV); release
 *                              with gpio_nbrake_release() once ready for
 *                              normal PWM operation. INHx is silently
 *                              ignored by the driver while this is low.
 */
void gpio_config_init(void);

/* EN_DRV (PB2): 1 = enable gate driver stage, 0 = keep it disabled.
 * Must only be raised AFTER PWM_CFG has been written over SPI (see
 * edl7141_configure_pwm_mode()), since PWM_MODE only latches while
 * EN_DRV is low (datasheet Table 20, "Standby" programmability). */
void gpio_en_drv_set(int enable);

/* Drives PC1 (VSENSE/nBRAKE) high, releasing the brake so INHx PWM
 * signals actually reach the driver outputs. See the long comment in
 * gpio_config.c for why this is safe without a series diode on this
 * board (CE hardwired high, PC1 stays a floating input - hardware
 * default - until this is called well after the driver's own startup
 * analog-sensing window has passed). */
void gpio_nbrake_release(void);

#endif /* GPIO_CONFIG_H */
