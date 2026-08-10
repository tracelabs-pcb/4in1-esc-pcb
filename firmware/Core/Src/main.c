#include "system_clock.h"
#include "gpio_config.h"
#include "edl7141_spi.h"
#include "pwm_tim1.h"
#include "commutation.h"

#define MOTOR1_POLE_PAIRS 7 /* adjust to the motor actually fitted */

/* Exposed so a debugger can watch live speed without a telemetry UART yet. */
volatile float g_motor1_erpm = 0.0f;
volatile float g_motor1_mech_rpm = 0.0f;

int main(void)
{
    system_clock_init();
    gpio_config_init();

    edl7141_spi_init();

    /* TODO: confirm on the schematic how CE and EN_DRV are wired
     * (dedicated GPIO vs. hardwired) - they are not in the pin list
     * this firmware was written against. PWM_CFG below is a
     * "Standby"-only bitfield: it must be written while EN_DRV is
     * still low/inactive, i.e. before whatever brings EN_DRV high. If
     * EN_DRV is hardwired directly to an always-on rail with no MCU
     * sequencing, this write may arrive too late. */
    if (!edl7141_check_device_id()) {
        for (;;) {
            /* SPI to the 6EDL7141 is not responding as expected (wrong
             * wiring, CS polarity, or EN_DRV/CE not yet powering the
             * chip). Do not proceed to PWM/commutation. */
        }
    }
    edl7141_configure_pwm_mode(); /* forces/confirms 6PWM mode, see edl7141_spi.h */

    commutation_init();

    /* Placeholder open-loop duty. Real ESC needs an alignment + ramp
     * stage here before back-EMF zero-crossing is detectable; see
     * commutation.h. */
    commutation_set_duty(0);

    for (;;) {
        g_motor1_erpm = commutation_get_erpm();
        g_motor1_mech_rpm = commutation_get_mech_rpm(MOTOR1_POLE_PAIRS);
    }
}
