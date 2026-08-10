#include "system_clock.h"
#include "gpio_config.h"
#include "edl7141_spi.h"
#include "pwm_tim1.h"
#include "commutation.h"

#define MOTOR1_POLE_PAIRS 7 /* adjust to the motor actually fitted */

/* Open-loop bring-up parameters - conservative starting guesses, will
 * likely need tuning on the bench. See firmware/README.md. Duty values
 * are in TIM1 ticks, 0..PWM_ARR_TICKS (see pwm_tim1.h). */
#define ALIGN_DUTY_TICKS     ((PWM_ARR_TICKS * 15U) / 100U) /* 15% */
#define ALIGN_TIME_US        500000U                        /* 500 ms */
#define RAMP_START_STEP_US   20000U                         /* slow start */
#define RAMP_END_STEP_US     3000U                           /* ramp end / cruise rate */
#define RAMP_STEPS           120U                            /* 20 electrical revolutions */
#define RUN_DUTY_TICKS       ((PWM_ARR_TICKS * 25U) / 100U) /* 25% */
#define CRUISE_STEP_US       RAMP_END_STEP_US

/* Exposed so a debugger can watch live speed without a telemetry UART yet. */
volatile float g_motor1_erpm = 0.0f;
volatile float g_motor1_mech_rpm = 0.0f;

int main(void)
{
    system_clock_init();
    g_debug_checkpoint = 1; /* clocks up */
    gpio_config_init(); /* EN_DRV (PB2) starts LOW here; CE is hardwired high via pull-up */
    g_debug_checkpoint = 2; /* gpio configured */

    edl7141_spi_init();
    g_debug_checkpoint = 3; /* SPI peripheral initialized */

    if (!edl7141_check_device_id()) {
        g_debug_checkpoint = 0xDEAD0001UL; /* stuck here: 6EDL7141 SPI not responding */
        for (;;) {
            /* SPI to the 6EDL7141 is not responding as expected (wrong
             * wiring or CS polarity). Do not proceed to PWM/EN_DRV. */
        }
    }
    g_debug_checkpoint = 4; /* device ID check passed */

    /* PWM_CFG (6PWM mode) is "Standby"-programmable: must be written
     * while EN_DRV is still low, i.e. before gpio_en_drv_set(1). */
    edl7141_configure_pwm_mode();
    g_debug_checkpoint = 5; /* 6PWM mode written */
    gpio_en_drv_set(1); /* enable gate driver stage now that 6PWM mode is latched */
    g_debug_checkpoint = 6; /* EN_DRV high */

    commutation_init();
    g_debug_checkpoint = 7; /* TIM1/TIM2/TIM3/EXTI configured, about to start open-loop ramp */

    /* Open-loop only for this bring-up: aligns the rotor, ramps up,
     * then cruises forever at a fixed rate - independent of whether
     * the LM2901 comparator-to-phase wiring assumed in commutation.c
     * is correct. This alone should make the motor turn.
     *
     * Once that is confirmed working, call
     * commutation_handoff_to_closed_loop() (e.g. after a fixed delay
     * once at cruise speed) to try sensorless BEMF commutation - watch
     * g_motor1_erpm afterwards: if it stays nonzero and the motor keeps
     * spinning smoothly, the mapping is right; if it stalls/judders,
     * the comparator-to-phase assumption (PHASE_A_LINE etc. in
     * commutation.c) needs correcting. */
    commutation_open_loop_start(ALIGN_DUTY_TICKS, ALIGN_TIME_US,
                                 RAMP_START_STEP_US, RAMP_END_STEP_US, RAMP_STEPS,
                                 RUN_DUTY_TICKS, CRUISE_STEP_US);

    for (;;) {
        g_motor1_erpm = commutation_get_erpm();
        g_motor1_mech_rpm = commutation_get_mech_rpm(MOTOR1_POLE_PAIRS);
    }
}
