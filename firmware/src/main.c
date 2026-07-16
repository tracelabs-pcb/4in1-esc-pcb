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
    edl7141_configure_3pwm_mode(); /* stub - see edl7141_spi.c safety note */

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
