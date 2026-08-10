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
 * Back-EMF zero-cross detection only works once the motor is already
 * spinning fast enough to produce a usable BEMF signal, so getting the
 * motor moving from a standstill needs a separate open-loop stage -
 * see commutation_open_loop_start() below.
 */
void commutation_init(void);

/* Commands the PWM duty (0..PWM_ARR_TICKS) applied to the currently
 * active phase. Ramp/throttle-mapping (DSHOT etc.) is out of scope
 * here; this is the raw actuator entry point. */
void commutation_set_duty(uint32_t duty_ticks);

/*
 * Blocking open-loop start-up: align the rotor to a known step, then
 * ramp through commutation steps at a fixed, MCU-timed rate (no BEMF
 * involved at all), accelerating from a slow start rate to a cruise
 * rate, then keeps commutating forever at that cruise rate via TIM3
 * (non-blocking after this call returns - main() is free to do other
 * things, e.g. poll commutation_get_erpm()).
 *
 * This does NOT depend on the LM2901 comparator wiring/mapping being
 * correct - it's the simplest way to get the motor to actually turn
 * for a first bring-up test. Call commutation_handoff_to_closed_loop()
 * afterwards only once you want to test sensorless BEMF commutation.
 *
 * align_duty_ticks/run_duty_ticks: 0..PWM_ARR_TICKS. align_time_us:
 * how long to hold the first step so the rotor settles into a known
 * position before ramping. ramp_start_step_us/ramp_end_step_us: time
 * per commutation step at the start/end of the ramp (start slow, end
 * fast) - ramp_steps steps are taken, linearly interpolating between
 * them. cruise_step_us: the fixed step time held forever after the
 * ramp (should normally equal ramp_end_step_us for a smooth handover).
 *
 * These are motor/prop/voltage-dependent and the defaults used in
 * main.c are only a conservative starting guess - see firmware/README.md
 * for tuning notes if the motor doesn't move or stutters instead of
 * spinning smoothly.
 */
void commutation_open_loop_start(uint32_t align_duty_ticks,
                                  uint32_t align_time_us,
                                  uint32_t ramp_start_step_us,
                                  uint32_t ramp_end_step_us,
                                  uint32_t ramp_steps,
                                  uint32_t run_duty_ticks,
                                  uint32_t cruise_step_us);

/* Switches from the open-loop cruise (see above) to closed-loop
 * BEMF/EXTI-driven commutation. Only call this after
 * commutation_open_loop_start() and only once you are ready to debug
 * the comparator-to-phase mapping in commutation.c (PHASE_A_LINE etc.)
 * - if that mapping or the expected rising/falling edge per step is
 * wrong, the motor will stall or run rough right after this call. */
void commutation_handoff_to_closed_loop(void);

/* Electrical RPM from the most recent accepted zero-crossing interval,
 * per eRPM = 60 / (6 * dt_seconds). Returns 0 if no edge seen yet. */
float commutation_get_erpm(void);

/* Mechanical RPM = eRPM / pole_pairs. */
float commutation_get_mech_rpm(uint8_t pole_pairs);

#endif /* COMMUTATION_H */
