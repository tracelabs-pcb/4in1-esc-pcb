#include "system_clock.h"
#include "gpio_config.h"
#include "edl7141_spi.h"
#include "pwm_tim1.h"
#include "commutation.h"
#include "stm32f405_regs.h"

/*
 * Phase-A-only bring-up (SPI/EN_DRV/fault checks, single test pulse)
 * passed - see HANDOFF history. This is the next stage: an actual
 * open-loop spin-up with the motor connected, using the six-step
 * commutation module (commutation.c/.h) that was written earlier but
 * never wired up. No BEMF/comparators involved yet - commutation is
 * driven purely by fixed MCU timing (align, then a timed ramp, then a
 * fixed cruise rate), so it does not depend on the still-unverified
 * comparator-to-phase mapping. See firmware/README.md, section
 * "Open-Loop-Start", for the full rationale and tuning guidance.
 *
 * SAFETY before flashing this with a motor attached:
 *   - Propeller OFF for the first test. Free-spinning shaft only.
 *   - Bench PSU current limit set sanely, hand near the power switch.
 *   - Motor free to move (not clamped somewhere that fights the shaft).
 *
 * LED convention (PC13), continuing the numbering from the earlier
 * bring-up stages:
 *   solid red        -> a check failed / a fault tripped mid-run,
 *                        stopped here (EN_DRV forced back low)
 *   blinking green   -> steps 1-2 (SPI read/write) passed
 *   4x green blinks  -> step 3 (EN_DRV + fault check) passed
 *   brief solid green -> step 4 (commutation_init() + fault check) passed
 *   slow green blink (~3s) -> "get ready" window before the motor moves
 *   fast green blink -> open-loop align+ramp+cruise is running
 *   (once cruising starts it keeps running forever, fast-blinking,
 *   until a fault is detected - then solid red, EN_DRV disabled)
 */

#define APPROX_MS_LOOP_COUNT 16800U /* crude, uncalibrated busy-wait unit at ~168MHz */

/* Open-loop start parameters - conservative starting guesses, not
 * values calculated for this specific motor/propeller. See
 * firmware/README.md "Open-Loop-Start" for what to tune if the motor
 * doesn't move, judders instead of ramping smoothly, or ends up too
 * slow/weak. */
#define ALIGN_DUTY_TICKS   ((PWM_ARR_TICKS * 15U) / 100U) /* 15% */
#define ALIGN_TIME_US      500000UL                        /* 500 ms */
#define RAMP_START_STEP_US 20000UL                         /* 20 ms/step at ramp start */
#define RAMP_END_STEP_US   3000UL                          /* 3 ms/step at ramp end */
#define RAMP_STEPS         120UL                           /* 20 electrical revolutions */
#define RUN_DUTY_TICKS     ((PWM_ARR_TICKS * 25U) / 100U) /* 25% */
#define CRUISE_STEP_US     3000UL                          /* matches RAMP_END_STEP_US for a smooth handover */

static void led_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER = (GPIOC->MODER & ~(0x3UL << (13 * 2))) | (0x1UL << (13 * 2)); /* PC13 output */
    GPIOC->PUPDR &= ~(0x3UL << (13 * 2));
}

static void led_set(int high)
{
    GPIOC->BSRR = high ? (1UL << 13) : (1UL << (13 + 16));
}

/* Bicolor LED on a single pin (HIGH=green, LOW=red) has no "off" via
 * BSRR alone - LOW just lights red. For a real green/dark blink,
 * switch PC13 to input (Hi-Z) instead of driving it low: no current
 * through either LED die, so it goes dark rather than red. */
static void led_off_hiz(void)
{
    GPIOC->MODER &= ~(0x3UL << (13 * 2)); /* PC13 -> input */
}

static void led_green_on(void)
{
    GPIOC->MODER = (GPIOC->MODER & ~(0x3UL << (13 * 2))) | (0x1UL << (13 * 2)); /* PC13 -> output */
    GPIOC->BSRR = (1UL << 13); /* drive high = green */
}

static void delay_approx_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < APPROX_MS_LOOP_COUNT; j++) {
        }
    }
}

/* Exposed for the debugger: raw FAULT_ST (address 0x00) reads, in case
 * the pass/fail LED needs a second look.
 *   g_debug_fault_st_before_clear - right after EN_DRV, before clearing
 *   g_debug_fault_st              - after writing FAULTS_CLR (the one
 *                                    that actually decides pass/fail) */
volatile uint16_t g_debug_fault_st_before_clear = 0xFFFFU;
volatile uint16_t g_debug_fault_st = 0xFFFFU;
volatile uint16_t g_debug_fault_st_after_pwm_init = 0xFFFFU; /* step 4 (commutation_init()) */
volatile uint16_t g_debug_fault_st_running = 0xFFFFU;        /* polled continuously once cruising */
volatile uint16_t g_debug_pwm_cfg_readback = 0xFFFFU; /* confirms PWM_MODE really is 6PWM (0x0000) */
volatile float    g_debug_erpm = 0.0f; /* stays 0 in open-loop mode - EXTI edges are ignored until commutation_handoff_to_closed_loop() */

static void fail_forever(void)
{
    led_set(0);
    for (;;) {
    }
}

/* Exposed for the debugger: raw fault-status registers, captured the
 * instant a HardFault happens (see stm32f405_regs.h for the individual
 * SCB_CFSR_ and SCB_HFSR_ bit meanings). Previously this project had
 * no real HardFault_Handler (only the startup file's weak
 * Default_Handler, which just loops forever) - meaning any crash was
 * indistinguishable from a plain infinite loop in the debugger, since
 * both showed up as "stuck in Default_Handler". This makes a real
 * crash diagnosable. */
volatile uint32_t g_debug_fault_cfsr = 0;
volatile uint32_t g_debug_fault_hfsr = 0;
volatile uint32_t g_debug_fault_mmfar = 0;
volatile uint32_t g_debug_fault_bfar = 0;
/* Confirmed by real hardware testing: CFSR came back as NOCP (bit 19,
 * "no coprocessor") with HFSR FORCED - but arm-none-eabi-objdump shows
 * zero VFP/coprocessor instructions anywhere in the final .elf. That
 * combination (a real NOCP fault with no real coprocessor instruction
 * in the compiled code at all) points at a wild jump: the CPU executed
 * from a corrupted/unintended address whose bytes happened to decode
 * as a coprocessor opcode - not an actual intentional FPU use. Capture
 * the exact faulting PC (and LR/EXC_RETURN) from the exception stack
 * frame to find out exactly where. */
volatile uint32_t g_debug_fault_pc = 0;
volatile uint32_t g_debug_fault_lr = 0;

/* used: only ever called from HardFault_Handler's inline asm below,
 * which GCC's own dead-code analysis can't see - without this it gets
 * silently dropped (just a "defined but not used" warning at compile
 * time, but an undefined-reference link error once the linker actually
 * tries to resolve the inline asm's branch target). */
__attribute__((used)) static void hard_fault_diagnose(uint32_t *stack_frame)
{
    /* Hardware-stacked exception frame layout: r0,r1,r2,r3,r12,LR,PC,xPSR. */
    g_debug_fault_lr = stack_frame[5];
    g_debug_fault_pc = stack_frame[6];
    g_debug_fault_cfsr = SCB->CFSR;
    g_debug_fault_hfsr = SCB->HFSR;
    g_debug_fault_mmfar = SCB->MMFAR;
    g_debug_fault_bfar = SCB->BFAR;
    gpio_en_drv_set(0); /* stop driving the motor immediately */
    fail_forever();     /* see g_debug_fault_pc and friends */
}

/* Naked: must read SP before any C prologue touches it, to recover the
 * hardware-pushed exception frame untouched. EXC_RETURN bit 2 (in LR
 * at fault entry) tells us whether MSP or PSP was in use - this
 * project only ever uses MSP (no RTOS/PSP switch), so this always
 * resolves to MSP in practice, but checking it properly costs nothing. */
__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4        \n"
        "ite eq            \n"
        "mrseq r0, msp     \n"
        "mrsne r0, psp     \n"
        "b hard_fault_diagnose \n"
    );
}

int main(void)
{
    system_clock_init();
    gpio_config_init(); /* sets up SPI1 (PB3/4/5), nCS (PB12), EN_DRV (PB2, starts LOW) */
    led_init();

    edl7141_spi_init();

    if (!edl7141_check_device_id()) {
        fail_forever(); /* step 1 (SPI read) failed */
    }

    uint16_t test_pattern = 0x0020U; /* BRAKE_CFG=b10 (High-Z), PWM_MODE=b000 (6PWM) */
    edl7141_write_reg(EDL7141_ADDR_PWM_CFG, test_pattern);
    if (edl7141_read_reg(EDL7141_ADDR_PWM_CFG) != test_pattern) {
        fail_forever(); /* step 2 (SPI write) failed */
    }

    /* Leave PWM_CFG in the real desired end state (6PWM) before EN_DRV
     * goes high - PWM_MODE only latches while EN_DRV is low. Unlike the
     * test pattern above, this write was never read back and verified -
     * do that now, since if PWM_MODE somehow isn't really b000 (6PWM),
     * INHA wouldn't mean what we assume it means (e.g. in 1PWM mode
     * INHA alone is duty/frequency only, commutation pattern comes from
     * other pins that are all sitting at 0 - nothing would move even
     * with a perfectly clean INHA signal arriving at the chip). */
    edl7141_configure_pwm_mode();
    g_debug_pwm_cfg_readback = edl7141_read_reg(EDL7141_ADDR_PWM_CFG);
    if (g_debug_pwm_cfg_readback != EDL7141_PWM_MODE_6PWM) {
        fail_forever(); /* PWM_CFG didn't stick at 6PWM - see g_debug_pwm_cfg_readback */
    }

    /* This board has no shunt resistors on the driver's CSNx/CSOx pins
     * (confirmed unbeschaltet) - disable all 3 internal current-sense
     * amplifiers so the floating phase-B amplifier (enabled by reset
     * default) can't spuriously trip CS_OCP_FLT. See edl7141_spi.h. */
    edl7141_disable_unused_current_sense();

    gpio_en_drv_set(1);
    delay_approx_ms(20); /* let charge pumps/output stage settle before checking for faults */

    /* Release VSENSE/nBRAKE (PC1, bodge wire): the board's existing
     * pull-down resistor holds this low forever otherwise, which the
     * driver reads as a permanently asserted brake and silently ignores
     * all INHx PWM commands. See gpio_config.c for why this is safe
     * without a series diode. Must happen well after the driver's own
     * startup analog-sensing window - by this point (after clock init,
     * SPI checks, EN_DRV settle delay) it long since has. */
    gpio_nbrake_release();

    /* FAULT_ST bits stay set once tripped until explicitly cleared,
     * even after the triggering condition is gone (e.g. a one-off
     * startup transient). Record the raw pre-clear value for
     * debugging, then clear and re-read to see the real ongoing state. */
    g_debug_fault_st_before_clear = edl7141_read_reg(EDL7141_ADDR_FAULT_ST);
    edl7141_clear_faults();
    delay_approx_ms(5);

    g_debug_fault_st = edl7141_read_reg(EDL7141_ADDR_FAULT_ST);
    if (g_debug_fault_st != 0x0000U) {
        gpio_en_drv_set(0); /* something's wrong, disable the driver stage again */
        fail_forever();     /* step 3 (EN_DRV + fault check) failed - see g_debug_fault_st */
    }

    /* Step 3 passed: a few green/dark blinks as a visual checkpoint,
     * then move on to step 4. */
    for (int i = 0; i < 4; i++) {
        led_green_on();
        delay_approx_ms(300);
        led_off_hiz();
        delay_approx_ms(300);
    }

    /* Step 4: bring up TIM1 (PWM), TIM2 (1 MHz timestamp), TIM3 (commutation
     * delay) and EXTI6/7/8 (comparator inputs, left masked - see
     * zero_cross_exti_init()). All channels sit at 0% duty right after
     * this (commutation_init() applies step 0 with s_duty_ticks==0), so
     * this is still safe in the same sense the old step 4 was - no
     * MOSFET gets driven yet. Re-check for any newly provoked fault. */
    commutation_init();
    delay_approx_ms(20);

    g_debug_fault_st_after_pwm_init = edl7141_read_reg(EDL7141_ADDR_FAULT_ST);
    if (g_debug_fault_st_after_pwm_init != 0x0000U) {
        gpio_en_drv_set(0);
        fail_forever(); /* step 4 failed - see g_debug_fault_st_after_pwm_init */
    }

    /* Step 4 passed: brief solid green. */
    led_green_on();
    delay_approx_ms(1000);
    led_off_hiz();
    delay_approx_ms(300);

    /* ~3s slow "get ready" blink before the motor actually moves. */
    for (int i = 0; i < 6; i++) {
        led_green_on();
        delay_approx_ms(250);
        led_off_hiz();
        delay_approx_ms(250);
    }

    /* Blocking for ~2s (align + ramp), then returns with the motor
     * cruising forever via TIM3_IRQHandler - see commutation.h. Purely
     * open-loop, no BEMF/comparators involved (commutation_handoff_to_closed_loop()
     * is intentionally not called here - verify smooth open-loop spin
     * first, see firmware/README.md). */
    commutation_open_loop_start(ALIGN_DUTY_TICKS, ALIGN_TIME_US,
                                 RAMP_START_STEP_US, RAMP_END_STEP_US, RAMP_STEPS,
                                 RUN_DUTY_TICKS, CRUISE_STEP_US);

    /* Cruising now. Keep watching FAULT_ST forever - any real fault
     * (e.g. overcurrent) needs an immediate stop, not just a one-off
     * check like the earlier bring-up steps had. */
    for (;;) {
        led_green_on();
        delay_approx_ms(80);
        led_off_hiz();
        delay_approx_ms(80);

        g_debug_erpm = commutation_get_erpm();
        g_debug_fault_st_running = edl7141_read_reg(EDL7141_ADDR_FAULT_ST);
        if (g_debug_fault_st_running != 0x0000U) {
            gpio_en_drv_set(0); /* cut all phase drive immediately */
            fail_forever(); /* fault while running - see g_debug_fault_st_running */
        }
    }
}
