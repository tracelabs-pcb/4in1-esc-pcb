/*
 * Minimal bare-metal register map for STM32F405RGT6.
 *
 * Only the peripherals actually used by this firmware are declared
 * (RCC, PWR, FLASH interface, GPIOA/B, TIM1/2/3, SPI1, SYSCFG, EXTI,
 * plus the Cortex-M4 core NVIC/SCB). Addresses and layouts are taken
 * from ST RM0090 (STM32F405/415/407/417/427/437/429/439 reference
 * manual) and the ARMv7-M architecture reference manual.
 */
#ifndef STM32F405_REGS_H
#define STM32F405_REGS_H

#include <stdint.h>

#define __IO volatile

/* ------------------------------------------------------------------ */
/* Core peripherals (Cortex-M4)                                        */
/* ------------------------------------------------------------------ */
typedef struct {
    __IO uint32_t ISER[8];
    uint32_t RESERVED0[24];
    __IO uint32_t ICER[8];
    uint32_t RESERVED1[24];
    __IO uint32_t ISPR[8];
    uint32_t RESERVED2[24];
    __IO uint32_t ICPR[8];
    uint32_t RESERVED3[24];
    __IO uint32_t IABR[8];
    uint32_t RESERVED4[56];
    __IO uint8_t  IP[240];
    uint32_t RESERVED5[644];
    __IO uint32_t STIR;
} NVIC_TypeDef;

typedef struct {
    __IO uint32_t CPUID;
    __IO uint32_t ICSR;
    __IO uint32_t VTOR;
    __IO uint32_t AIRCR;
    __IO uint32_t SCR;
    __IO uint32_t CCR;
    __IO uint8_t  SHP[12];
    __IO uint32_t SHCSR;
} SCB_TypeDef;

#define NVIC_BASE   (0xE000E100UL)
#define SCB_BASE    (0xE000ED00UL)
#define NVIC        ((NVIC_TypeDef *) NVIC_BASE)
#define SCB         ((SCB_TypeDef  *) SCB_BASE)

/* ------------------------------------------------------------------ */
/* Peripheral base addresses                                          */
/* ------------------------------------------------------------------ */
#define PERIPH_BASE       (0x40000000UL)
#define APB1PERIPH_BASE   (PERIPH_BASE)
#define APB2PERIPH_BASE   (PERIPH_BASE + 0x00010000UL)
#define AHB1PERIPH_BASE   (PERIPH_BASE + 0x00020000UL)

#define TIM2_BASE   (APB1PERIPH_BASE + 0x0000UL)
#define TIM3_BASE   (APB1PERIPH_BASE + 0x0400UL)
#define TIM4_BASE   (APB1PERIPH_BASE + 0x0800UL)
#define PWR_BASE    (APB1PERIPH_BASE + 0x7000UL)

#define TIM1_BASE   (APB2PERIPH_BASE + 0x0000UL)
#define SPI1_BASE   (APB2PERIPH_BASE + 0x3000UL)
#define SYSCFG_BASE (APB2PERIPH_BASE + 0x3800UL)
#define EXTI_BASE   (APB2PERIPH_BASE + 0x3C00UL)

#define GPIOA_BASE  (AHB1PERIPH_BASE + 0x0000UL)
#define GPIOB_BASE  (AHB1PERIPH_BASE + 0x0400UL)
#define RCC_BASE    (AHB1PERIPH_BASE + 0x3800UL)
#define FLASH_R_BASE (AHB1PERIPH_BASE + 0x3C00UL)

/* ------------------------------------------------------------------ */
/* RCC                                                                 */
/* ------------------------------------------------------------------ */
typedef struct {
    __IO uint32_t CR;
    __IO uint32_t PLLCFGR;
    __IO uint32_t CFGR;
    __IO uint32_t CIR;
    __IO uint32_t AHB1RSTR;
    __IO uint32_t AHB2RSTR;
    __IO uint32_t AHB3RSTR;
    uint32_t RESERVED0;
    __IO uint32_t APB1RSTR;
    __IO uint32_t APB2RSTR;
    uint32_t RESERVED1[2];
    __IO uint32_t AHB1ENR;
    __IO uint32_t AHB2ENR;
    __IO uint32_t AHB3ENR;
    uint32_t RESERVED2;
    __IO uint32_t APB1ENR;
    __IO uint32_t APB2ENR;
} RCC_TypeDef;

#define RCC ((RCC_TypeDef *) RCC_BASE)

#define RCC_CR_HSEON        (1UL << 16)
#define RCC_CR_HSERDY       (1UL << 17)
#define RCC_CR_PLLON        (1UL << 24)
#define RCC_CR_PLLRDY       (1UL << 25)

#define RCC_PLLCFGR_PLLSRC_HSE (1UL << 22)

#define RCC_CFGR_SW_PLL     (2UL << 0)
#define RCC_CFGR_SWS_PLL    (2UL << 2)
#define RCC_CFGR_SWS_Msk    (3UL << 2)
#define RCC_CFGR_HPRE_DIV1  (0UL << 4)
#define RCC_CFGR_PPRE1_DIV4 (5UL << 10)
#define RCC_CFGR_PPRE2_DIV2 (4UL << 13)

#define RCC_AHB1ENR_GPIOAEN (1UL << 0)
#define RCC_AHB1ENR_GPIOBEN (1UL << 1)

#define RCC_APB1ENR_TIM2EN  (1UL << 0)
#define RCC_APB1ENR_TIM3EN  (1UL << 1)
#define RCC_APB1ENR_TIM4EN  (1UL << 2)
#define RCC_APB1ENR_PWREN   (1UL << 28)

#define RCC_APB2ENR_TIM1EN  (1UL << 0)
#define RCC_APB2ENR_SPI1EN  (1UL << 12)
#define RCC_APB2ENR_SYSCFGEN (1UL << 14)

/* ------------------------------------------------------------------ */
/* FLASH interface (wait states)                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    __IO uint32_t ACR;
} FLASH_TypeDef;

#define FLASH ((FLASH_TypeDef *) FLASH_R_BASE)

#define FLASH_ACR_LATENCY_5WS (5UL << 0)
#define FLASH_ACR_PRFTEN     (1UL << 8)
#define FLASH_ACR_ICEN       (1UL << 9)
#define FLASH_ACR_DCEN       (1UL << 10)

/* ------------------------------------------------------------------ */
/* PWR                                                                 */
/* ------------------------------------------------------------------ */
typedef struct {
    __IO uint32_t CR;
    __IO uint32_t CSR;
} PWR_TypeDef;

#define PWR ((PWR_TypeDef *) PWR_BASE)
#define PWR_CR_VOS (1UL << 14)

/* ------------------------------------------------------------------ */
/* GPIO                                                                */
/* ------------------------------------------------------------------ */
typedef struct {
    __IO uint32_t MODER;
    __IO uint32_t OTYPER;
    __IO uint32_t OSPEEDR;
    __IO uint32_t PUPDR;
    __IO uint32_t IDR;
    __IO uint32_t ODR;
    __IO uint32_t BSRR;
    __IO uint32_t LCKR;
    __IO uint32_t AFR[2];
} GPIO_TypeDef;

#define GPIOA ((GPIO_TypeDef *) GPIOA_BASE)
#define GPIOB ((GPIO_TypeDef *) GPIOB_BASE)

#define GPIO_MODE_INPUT  0x0UL
#define GPIO_MODE_OUTPUT 0x1UL
#define GPIO_MODE_AF     0x2UL
#define GPIO_MODE_ANALOG 0x3UL

/* ------------------------------------------------------------------ */
/* TIM (general purpose / advanced control, common subset)             */
/* ------------------------------------------------------------------ */
typedef struct {
    __IO uint32_t CR1;
    __IO uint32_t CR2;
    __IO uint32_t SMCR;
    __IO uint32_t DIER;
    __IO uint32_t SR;
    __IO uint32_t EGR;
    __IO uint32_t CCMR1;
    __IO uint32_t CCMR2;
    __IO uint32_t CCER;
    __IO uint32_t CNT;
    __IO uint32_t PSC;
    __IO uint32_t ARR;
    __IO uint32_t RCR;
    __IO uint32_t CCR1;
    __IO uint32_t CCR2;
    __IO uint32_t CCR3;
    __IO uint32_t CCR4;
    __IO uint32_t BDTR;
    __IO uint32_t DCR;
    __IO uint32_t DMAR;
} TIM_TypeDef;

#define TIM1 ((TIM_TypeDef *) TIM1_BASE)
#define TIM2 ((TIM_TypeDef *) TIM2_BASE)
#define TIM3 ((TIM_TypeDef *) TIM3_BASE)

#define TIM_CR1_CEN   (1UL << 0)
#define TIM_CR1_OPM   (1UL << 3)
#define TIM_CR1_ARPE  (1UL << 7)

#define TIM_DIER_UIE  (1UL << 0)
#define TIM_SR_UIF    (1UL << 0)
#define TIM_EGR_UG    (1UL << 0)

#define TIM_BDTR_MOE  (1UL << 15)

/* ------------------------------------------------------------------ */
/* SPI                                                                 */
/* ------------------------------------------------------------------ */
typedef struct {
    __IO uint32_t CR1;
    __IO uint32_t CR2;
    __IO uint32_t SR;
    __IO uint32_t DR;
    __IO uint32_t CRCPR;
    __IO uint32_t RXCRCR;
    __IO uint32_t TXCRCR;
    __IO uint32_t I2SCFGR;
    __IO uint32_t I2SPR;
} SPI_TypeDef;

#define SPI1 ((SPI_TypeDef *) SPI1_BASE)

#define SPI_CR1_CPHA    (1UL << 0)
#define SPI_CR1_CPOL    (1UL << 1)
#define SPI_CR1_MSTR    (1UL << 2)
#define SPI_CR1_BR_DIV8  (2UL << 3)
#define SPI_CR1_BR_DIV32 (4UL << 3)
#define SPI_CR1_SPE     (1UL << 6)
#define SPI_CR1_SSI     (1UL << 8)
#define SPI_CR1_SSM     (1UL << 9)

#define SPI_SR_RXNE (1UL << 0)
#define SPI_SR_TXE  (1UL << 1)
#define SPI_SR_BSY  (1UL << 7)

/* ------------------------------------------------------------------ */
/* SYSCFG / EXTI                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    __IO uint32_t MEMRMP;
    __IO uint32_t PMC;
    __IO uint32_t EXTICR[4];
    uint32_t RESERVED[2];
    __IO uint32_t CMPCR;
} SYSCFG_TypeDef;

#define SYSCFG ((SYSCFG_TypeDef *) SYSCFG_BASE)

typedef struct {
    __IO uint32_t IMR;
    __IO uint32_t EMR;
    __IO uint32_t RTSR;
    __IO uint32_t FTSR;
    __IO uint32_t SWIER;
    __IO uint32_t PR;
} EXTI_TypeDef;

#define EXTI ((EXTI_TypeDef *) EXTI_BASE)

/* IRQ numbers (STM32F405 vector table, position = IRQn) */
#define EXTI9_5_IRQn   23
#define TIM2_IRQn      28
#define TIM3_IRQn      29

#endif /* STM32F405_REGS_H */
