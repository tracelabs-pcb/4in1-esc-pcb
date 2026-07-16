#ifndef ZERO_CROSS_H
#define ZERO_CROSS_H

#include <stdint.h>

/* Free-running 1 MHz (1 us/tick) timebase on TIM2, used to timestamp
 * comparator edges captured in the EXTI ISR and to derive eRPM. */
void zero_cross_timebase_init(void);
uint32_t zero_cross_now(void);

/* EXTI6/7/8 (PB6/7/8, Motor 1 comparator outputs), both edges enabled;
 * edge-direction filtering per commutation step happens in commutation.c. */
void zero_cross_exti_init(void);

/* eRPM = 60 / (6 * dt_seconds), per user spec: 6 zero-crossings per
 * electrical revolution, dt = time between two consecutive crossings. */
float zero_cross_calc_erpm(uint32_t dt_ticks_1mhz);

#endif /* ZERO_CROSS_H */
