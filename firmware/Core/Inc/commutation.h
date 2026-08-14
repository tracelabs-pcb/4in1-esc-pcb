#ifndef COMMUTATION_H
#define COMMUTATION_H

#include <stdint.h>

/*
 * Bring-up diagnostics: plain counters you can read in the debugger's
 * Variables/Expressions view after a crash, no register/hex decoding
 * needed. Add them as Expressions (Debug perspective -> Expressions
 * view -> Add) so they're visible even if source-level Variables
 * doesn't resolve after a fault:
 *
 *   g_debug_checkpoint  - main.c stage reached (see the numbered
 *                          comments next to each assignment in main.c)
 *   g_debug_ramp_i       - last ramp-loop iteration index reached
 *                          (0..RAMP_STEPS-1), 0xFFFFFFFF if ramp not
 *                          started yet, set to RAMP_STEPS once cruise begins
 *   g_debug_tim3_count   - how many times TIM3_IRQHandler has fired
 *   g_debug_exti_count   - how many times EXTI9_5_IRQHandler has fired
 *                          (should stay 0 during open-loop; if it's
 *                          nonzero, the EXTI mask fix isn't active -
 *                          you're still running old code)
 */
extern volatile uint32_t g_debug_checkpoint;
extern volatile uint32_t g_debug_ramp_i;
extern volatile uint32_t g_debug_tim3_count;
extern volatile uint32_t g_debug_exti_count;

/* Six-step trapezoidal sensorless commutation for Motor 1.
 *
 * Sets up TIM1 (PWM), TIM2 (1 MHz timestamp base), TIM3 (30 electrical
 * degree commutation delay, one-pulse mode) and EXTI6/7/8 (comparator
 * zero-cross inputs), then runs the commutation state machine entirely
 * from ISRs (EXTI9_5_IRQHandler + TIM3_IRQHandler).
 *
 * Requires the 6EDL7141 to be in 6PWM mode (see edl7141_configure_pwm_mode()
 * in edl7141_spi.c) - NOT 3PWM - because this board grounds INLx. Per the
 * 6EDL7141 datasheet (Rev 1.20, Table 8), with INLx=0, driving INHx=0
 * yields GHx=LOW/GLx=LOW/SHx=High-Z, i.e. the inactive phase genuinely
 * floats, which is what apply_step()'s "the two non-driven phases get
 * duty=0" logic below relies on. 3PWM mode ignores INLx entirely and
 * always drives the complementary low side, which would clamp the
 * "floating" phase to GND instead and break BEMF sensing.
 *
 * CURRENTLY NOT SATISFIED: edl7141_configure_pwm_mode() presently
 * writes 3PWM, not 6PWM (see the long comment in edl7141_spi.h) -
 * INLx being hardwired to GND with no MCU control means 6PWM mode can
 * never establish real motor current at all (confirmed by real
 * hardware testing: no current increase with a motor attached, any
 * duty). 3PWM trades away BEMF-sensing compatibility for real torque,
 * which is fine for open-loop bring-up (nothing below reads the
 * comparators yet) but means this module's closed-loop half
 * (commutation_handoff_to_closed_loop() and everything it enables)
 * will NOT work correctly until either INLx gets bodged to real GPIOs
 * and 6PWM is restored, or a non-comparator closed-loop strategy is
 * built instead.
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

/* Advances to the next of the six commutation steps and applies it
 * (same effect as one TIM3_IRQHandler firing, minus the interrupt).
 * Call this from a plain polled loop at roughly cruise_step_us
 * intervals to run the motor without relying on the TIM3 interrupt at
 * all - see the comment on commutation_open_loop_start() below for
 * why this exists as a separate, explicitly-callable step. */
void commutation_step_advance(void);

/*
 * Blocking open-loop start-up: align the rotor to a known step, then
 * ramp through commutation steps at a fixed, MCU-timed rate (no BEMF
 * involved at all), accelerating from a slow start rate to a cruise
 * rate. Returns with the cruise duty applied at whatever step the
 * ramp ended on - the caller is responsible for continuing to advance
 * steps from there (see commutation_step_advance() above), typically
 * from a polled loop at roughly the same rate as ramp_end_step_us.
 *
 * This intentionally does NOT arm TIM3 to keep cruising by itself
 * anymore (an earlier version did) - real hardware testing hit a
 * HardFault (CFSR=NOCP, garbage PC/LR not matching any real
 * coprocessor instruction in the build) shortly after the first TIM3
 * IRQ fired, root cause not yet found. Polling from the caller
 * sidesteps that for open-loop; TIM3/EXTI are still set up in
 * commutation_init() for whenever the interrupt-driven path (needed
 * for real closed-loop BEMF timing) gets debugged.
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
 * them.
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
                                  uint32_t run_duty_ticks);

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
