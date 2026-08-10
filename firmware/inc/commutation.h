#ifndef COMMUTATION_H
#define COMMUTATION_H

#include <stdint.h>

/* Six-step trapezoidal sensorless commutation for Motor 1.
 *
 * Sets up TIM1 (PWM), TIM2 (1 MHz timestamp base), TIM3 (30 electrical
 * degree commutation delay, one-pulse mode) and EXTI6/7/8 (comparator
 * zero-cross inputs), then runs the commutation state machine entirely
 * from ISRs (EXTI9_5_IRQHandler + TIM3_IRQHandler).
 *
 * Requires the 6EDL7141 to be in 6PWM mode (see edl7141_configure_pwm_mode()
 * in edl7141_spi.c) - NOT 3PWM - because this board grounds INLx. Per the
 * 6EDL7141 datasheet (Rev 1.02, Table 8), with INLx=0, driving INHx=0
 * yields GHx=LOW/GLx=LOW/SHx=High-Z, i.e. the inactive phase genuinely
 * floats, which is what apply_step()'s "the two non-driven phases get
 * duty=0" logic below relies on. 3PWM mode ignores INLx entirely and
 * always drives the complementary low side, which would clamp the
 * "floating" phase to GND instead and break BEMF sensing.
 *
 * NOT included here: open-loop startup. Back-EMF zero-cross detection
 * only works once the motor is already spinning fast enough to produce
 * a usable BEMF signal. A real ESC needs an alignment + open-loop ramp
 * stage before handing off to this closed-loop commutation - that
 * stage is a separate piece of work, not implemented in this drop.
 */
void commutation_init(void);

/* Commands the PWM duty (0..PWM_ARR_TICKS) applied to the currently
 * active phase. Ramp/throttle-mapping (DSHOT etc.) is out of scope
 * here; this is the raw actuator entry point. */
void commutation_set_duty(uint32_t duty_ticks);

/* Electrical RPM from the most recent accepted zero-crossing interval,
 * per eRPM = 60 / (6 * dt_seconds). Returns 0 if no edge seen yet. */
float commutation_get_erpm(void);

/* Mechanical RPM = eRPM / pole_pairs. */
float commutation_get_mech_rpm(uint8_t pole_pairs);

#endif /* COMMUTATION_H */
