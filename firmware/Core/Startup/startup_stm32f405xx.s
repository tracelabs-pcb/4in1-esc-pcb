/*
 * Minimal startup file for STM32F405RGT6: initial stack pointer,
 * vector table, Reset_Handler (copies .data from flash to RAM, zeroes
 * .bss, calls SystemInit-less main()), and a full vector table with
 * weak Default_Handler aliases for every IRQ position so unused
 * interrupts can never jump into garbage. EXTI9_5_IRQHandler and
 * TIM3_IRQHandler are overridden with real implementations in
 * commutation.c (weak/strong symbol resolution at link time).
 */
    .syntax unified
    .cpu cortex-m4
    .fpu softvfp
    .thumb

.global g_pfnVectors
.global Default_Handler

/* Linker-script provided symbols. */
.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    ldr sp, =_estack

    /* Copy .data section from flash to RAM. */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b LoopCopyDataInit

CopyDataInit:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4

LoopCopyDataInit:
    adds r4, r0, r3
    cmp r4, r1
    bcc CopyDataInit

    /* Zero the .bss section. */
    ldr r2, =_sbss
    ldr r4, =_ebss
    movs r3, #0
    b LoopFillZerobss

FillZerobss:
    str r3, [r2]
    adds r2, r2, #4

LoopFillZerobss:
    cmp r2, r4
    bcc FillZerobss

    bl main
    b .
    .size Reset_Handler, .-Reset_Handler

    .section .text.Default_Handler,"ax",%progbits
Default_Handler:
    b .
    .size Default_Handler, .-Default_Handler

/* Macro to declare a weak IRQ handler aliased to Default_Handler. */
.macro WEAK_HANDLER name
    .weak \name
    .thumb_set \name, Default_Handler
.endm

WEAK_HANDLER NMI_Handler
WEAK_HANDLER HardFault_Handler
WEAK_HANDLER MemManage_Handler
WEAK_HANDLER BusFault_Handler
WEAK_HANDLER UsageFault_Handler
WEAK_HANDLER SVC_Handler
WEAK_HANDLER DebugMon_Handler
WEAK_HANDLER PendSV_Handler
WEAK_HANDLER SysTick_Handler

WEAK_HANDLER WWDG_IRQHandler
WEAK_HANDLER PVD_IRQHandler
WEAK_HANDLER TAMP_STAMP_IRQHandler
WEAK_HANDLER RTC_WKUP_IRQHandler
WEAK_HANDLER FLASH_IRQHandler
WEAK_HANDLER RCC_IRQHandler
WEAK_HANDLER EXTI0_IRQHandler
WEAK_HANDLER EXTI1_IRQHandler
WEAK_HANDLER EXTI2_IRQHandler
WEAK_HANDLER EXTI3_IRQHandler
WEAK_HANDLER EXTI4_IRQHandler
WEAK_HANDLER DMA1_Stream0_IRQHandler
WEAK_HANDLER DMA1_Stream1_IRQHandler
WEAK_HANDLER DMA1_Stream2_IRQHandler
WEAK_HANDLER DMA1_Stream3_IRQHandler
WEAK_HANDLER DMA1_Stream4_IRQHandler
WEAK_HANDLER DMA1_Stream5_IRQHandler
WEAK_HANDLER DMA1_Stream6_IRQHandler
WEAK_HANDLER ADC_IRQHandler
WEAK_HANDLER CAN1_TX_IRQHandler
WEAK_HANDLER CAN1_RX0_IRQHandler
WEAK_HANDLER CAN1_RX1_IRQHandler
WEAK_HANDLER CAN1_SCE_IRQHandler
WEAK_HANDLER EXTI9_5_IRQHandler
WEAK_HANDLER TIM1_BRK_TIM9_IRQHandler
WEAK_HANDLER TIM1_UP_TIM10_IRQHandler
WEAK_HANDLER TIM1_TRG_COM_TIM11_IRQHandler
WEAK_HANDLER TIM1_CC_IRQHandler
WEAK_HANDLER TIM2_IRQHandler
WEAK_HANDLER TIM3_IRQHandler
WEAK_HANDLER TIM4_IRQHandler
WEAK_HANDLER I2C1_EV_IRQHandler
WEAK_HANDLER I2C1_ER_IRQHandler
WEAK_HANDLER I2C2_EV_IRQHandler
WEAK_HANDLER I2C2_ER_IRQHandler
WEAK_HANDLER SPI1_IRQHandler
WEAK_HANDLER SPI2_IRQHandler
WEAK_HANDLER USART1_IRQHandler
WEAK_HANDLER USART2_IRQHandler
WEAK_HANDLER USART3_IRQHandler
WEAK_HANDLER EXTI15_10_IRQHandler
WEAK_HANDLER RTC_Alarm_IRQHandler
WEAK_HANDLER OTG_FS_WKUP_IRQHandler
WEAK_HANDLER TIM8_BRK_TIM12_IRQHandler
WEAK_HANDLER TIM8_UP_TIM13_IRQHandler
WEAK_HANDLER TIM8_TRG_COM_TIM14_IRQHandler
WEAK_HANDLER TIM8_CC_IRQHandler
WEAK_HANDLER DMA1_Stream7_IRQHandler
WEAK_HANDLER FSMC_IRQHandler
WEAK_HANDLER SDIO_IRQHandler
WEAK_HANDLER TIM5_IRQHandler
WEAK_HANDLER SPI3_IRQHandler
WEAK_HANDLER UART4_IRQHandler
WEAK_HANDLER UART5_IRQHandler
WEAK_HANDLER TIM6_DAC_IRQHandler
WEAK_HANDLER TIM7_IRQHandler
WEAK_HANDLER DMA2_Stream0_IRQHandler
WEAK_HANDLER DMA2_Stream1_IRQHandler
WEAK_HANDLER DMA2_Stream2_IRQHandler
WEAK_HANDLER DMA2_Stream3_IRQHandler
WEAK_HANDLER DMA2_Stream4_IRQHandler
WEAK_HANDLER Reserved61_IRQHandler
WEAK_HANDLER Reserved62_IRQHandler
WEAK_HANDLER Reserved63_IRQHandler
WEAK_HANDLER Reserved64_IRQHandler
WEAK_HANDLER OTG_FS_IRQHandler
WEAK_HANDLER DMA2_Stream5_IRQHandler
WEAK_HANDLER DMA2_Stream6_IRQHandler
WEAK_HANDLER DMA2_Stream7_IRQHandler
WEAK_HANDLER USART6_IRQHandler
WEAK_HANDLER I2C3_EV_IRQHandler
WEAK_HANDLER I2C3_ER_IRQHandler
WEAK_HANDLER OTG_HS_EP1_OUT_IRQHandler
WEAK_HANDLER OTG_HS_EP1_IN_IRQHandler
WEAK_HANDLER OTG_HS_WKUP_IRQHandler
WEAK_HANDLER OTG_HS_IRQHandler
WEAK_HANDLER DCMI_IRQHandler
WEAK_HANDLER CRYP_IRQHandler
WEAK_HANDLER HASH_RNG_IRQHandler
WEAK_HANDLER FPU_IRQHandler

    .section .isr_vector,"a",%progbits
    .type g_pfnVectors, %object
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler
    .word WWDG_IRQHandler
    .word PVD_IRQHandler
    .word TAMP_STAMP_IRQHandler
    .word RTC_WKUP_IRQHandler
    .word FLASH_IRQHandler
    .word RCC_IRQHandler
    .word EXTI0_IRQHandler
    .word EXTI1_IRQHandler
    .word EXTI2_IRQHandler
    .word EXTI3_IRQHandler
    .word EXTI4_IRQHandler
    .word DMA1_Stream0_IRQHandler
    .word DMA1_Stream1_IRQHandler
    .word DMA1_Stream2_IRQHandler
    .word DMA1_Stream3_IRQHandler
    .word DMA1_Stream4_IRQHandler
    .word DMA1_Stream5_IRQHandler
    .word DMA1_Stream6_IRQHandler
    .word ADC_IRQHandler
    .word CAN1_TX_IRQHandler
    .word CAN1_RX0_IRQHandler
    .word CAN1_RX1_IRQHandler
    .word CAN1_SCE_IRQHandler
    .word EXTI9_5_IRQHandler          /* IRQ23 */
    .word TIM1_BRK_TIM9_IRQHandler
    .word TIM1_UP_TIM10_IRQHandler
    .word TIM1_TRG_COM_TIM11_IRQHandler
    .word TIM1_CC_IRQHandler
    .word TIM2_IRQHandler
    .word TIM3_IRQHandler             /* IRQ29 */
    .word TIM4_IRQHandler
    .word I2C1_EV_IRQHandler
    .word I2C1_ER_IRQHandler
    .word I2C2_EV_IRQHandler
    .word I2C2_ER_IRQHandler
    .word SPI1_IRQHandler
    .word SPI2_IRQHandler
    .word USART1_IRQHandler
    .word USART2_IRQHandler
    .word USART3_IRQHandler
    .word EXTI15_10_IRQHandler
    .word RTC_Alarm_IRQHandler
    .word OTG_FS_WKUP_IRQHandler
    .word TIM8_BRK_TIM12_IRQHandler
    .word TIM8_UP_TIM13_IRQHandler
    .word TIM8_TRG_COM_TIM14_IRQHandler
    .word TIM8_CC_IRQHandler
    .word DMA1_Stream7_IRQHandler
    .word FSMC_IRQHandler
    .word SDIO_IRQHandler
    .word TIM5_IRQHandler
    .word SPI3_IRQHandler
    .word UART4_IRQHandler
    .word UART5_IRQHandler
    .word TIM6_DAC_IRQHandler
    .word TIM7_IRQHandler
    .word DMA2_Stream0_IRQHandler
    .word DMA2_Stream1_IRQHandler
    .word DMA2_Stream2_IRQHandler
    .word DMA2_Stream3_IRQHandler
    .word DMA2_Stream4_IRQHandler
    .word Reserved61_IRQHandler
    .word Reserved62_IRQHandler
    .word Reserved63_IRQHandler
    .word Reserved64_IRQHandler
    .word OTG_FS_IRQHandler
    .word DMA2_Stream5_IRQHandler
    .word DMA2_Stream6_IRQHandler
    .word DMA2_Stream7_IRQHandler
    .word USART6_IRQHandler
    .word I2C3_EV_IRQHandler
    .word I2C3_ER_IRQHandler
    .word OTG_HS_EP1_OUT_IRQHandler
    .word OTG_HS_EP1_IN_IRQHandler
    .word OTG_HS_WKUP_IRQHandler
    .word OTG_HS_IRQHandler
    .word DCMI_IRQHandler
    .word CRYP_IRQHandler
    .word HASH_RNG_IRQHandler
    .word FPU_IRQHandler
    .size g_pfnVectors, .-g_pfnVectors
