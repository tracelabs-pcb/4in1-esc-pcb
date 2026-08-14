#include "stm32f405_regs.h"
#include "system_clock.h"

/*
 * Bring the STM32F405 from the default 16 MHz HSI up to 168 MHz using
 * the external crystal (HSE) and the main PLL:
 *
 *   f_VCO   = HSE / M * N   = 8MHz / 8 * 336 = 336 MHz
 *   SYSCLK  = f_VCO / P     = 336 MHz / 2    = 168 MHz
 *   USB/SDIO clk (unused)   = f_VCO / Q      = 336 MHz / 7 = 48 MHz
 *
 * This is the standard configuration for an 8 MHz HSE crystal; if the
 * board uses a different crystal, PLLM must be recalculated so that
 * HSE / PLLM = 1 MHz (PLL input reference).
 */
void system_clock_init(void)
{
    /* Enable full access to the FPU coprocessor (CP10/CP11) before
     * anything in this project could execute a floating-point
     * instruction (commutation.c's eRPM calculation, called from
     * main()'s cruise loop). Found missing via real hardware testing:
     * this board's STM32F405 build under STM32CubeIDE's bundled
     * toolchain generates real VFP instructions (confirmed in the
     * disassembly - e.g. vmov.f32/vldr in zero_cross_calc_erpm()),
     * unlike this project's own Makefile (-mfloat-abi=soft, verified
     * to produce zero VFP instructions there). Without this, the first
     * float instruction executed takes a NOCP UsageFault (escalates to
     * HardFault, since UsageFault is never individually enabled) - the
     * exact fault (CFSR=NOCP, HFSR=FORCED) chased through several
     * false leads (TIM3 IRQ, stack corruption) before being traced
     * here. The dsb/isb pair is the standard CMSIS idiom to guarantee
     * the CPACR write has taken effect before anything after it runs. */
    SCB_CPACR |= (0xFUL << 20); /* CP10 and CP11 = 0b11 (full access) */
    __asm volatile ("dsb");
    __asm volatile ("isb");

    /* 1. Enable HSE and wait for it to stabilize. */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) {
    }

    /* 2. Regulator voltage scale + flash wait states for 168 MHz @ 3.3V. */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    FLASH->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN |
                 FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /* 3. Bus prescalers: AHB /1, APB1 /4 (<=42MHz), APB2 /2 (<=84MHz). */
    RCC->CFGR = (RCC->CFGR & ~(0xFUL << 4)) | RCC_CFGR_HPRE_DIV1;
    RCC->CFGR = (RCC->CFGR & ~(0x7UL << 10)) | RCC_CFGR_PPRE1_DIV4;
    RCC->CFGR = (RCC->CFGR & ~(0x7UL << 13)) | RCC_CFGR_PPRE2_DIV2;

    /* 4. Main PLL: M=8, N=336, P=2, Q=7, source = HSE. */
    const uint32_t pllm = 8;
    const uint32_t plln = 336;
    const uint32_t pllp_bits = 0; /* 00 = /2 */
    const uint32_t pllq = 7;

    RCC->PLLCFGR = pllm
                 | (plln << 6)
                 | (pllp_bits << 16)
                 | RCC_PLLCFGR_PLLSRC_HSE
                 | (pllq << 24);

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {
    }

    /* 5. Switch SYSCLK to the PLL output and wait for confirmation. */
    RCC->CFGR = (RCC->CFGR & ~(0x3UL << 0)) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL) {
    }
}
