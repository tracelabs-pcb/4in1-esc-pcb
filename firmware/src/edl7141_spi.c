#include "stm32f405_regs.h"
#include "edl7141_spi.h"

#define CS_LOW()  (GPIOB->BSRR = (1UL << (12 + 16)))
#define CS_HIGH() (GPIOB->BSRR = (1UL << 12))

static uint8_t spi1_transfer_byte(uint8_t tx)
{
    while (!(SPI1->SR & SPI_SR_TXE)) {
    }
    SPI1->DR = tx;
    while (!(SPI1->SR & SPI_SR_RXNE)) {
    }
    return (uint8_t) SPI1->DR;
}

void edl7141_spi_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    /* Mode 1 (CPOL=0, CPHA=1), master, software NSS, MSB first,
     * APB2/32 = ~2.6 MHz SCK (conservative; raise once bring-up is
     * confirmed against the 6EDL7141's max SPI clock spec). */
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_CPHA | SPI_CR1_BR_DIV32
              | SPI_CR1_SSM | SPI_CR1_SSI;
    SPI1->CR1 |= SPI_CR1_SPE;

    CS_HIGH();
}

static uint16_t edl7141_transfer(uint8_t rw_bit, uint8_t addr7, uint16_t data)
{
    uint8_t b0 = (uint8_t) ((rw_bit & 0x1U) << 7) | (addr7 & 0x7FU);
    uint8_t b1 = (uint8_t) (data >> 8);
    uint8_t b2 = (uint8_t) (data & 0xFFU);

    CS_LOW();
    (void) spi1_transfer_byte(b0);
    uint8_t r1 = spi1_transfer_byte(b1);
    uint8_t r2 = spi1_transfer_byte(b2);
    CS_HIGH();

    return (uint16_t) ((r1 << 8) | r2);
}

uint16_t edl7141_write_reg(uint8_t addr7, uint16_t data)
{
    return edl7141_transfer(1, addr7, data); /* rw_bit polarity: verify against datasheet */
}

uint16_t edl7141_read_reg(uint8_t addr7)
{
    return edl7141_transfer(0, addr7, 0x0000);
}

void edl7141_configure_3pwm_mode(void)
{
    /*
     * Intentionally not implemented.
     *
     * Before calling this, look up in the 6EDL7141 datasheet
     * (section 8.2, Register Map):
     *   - which register/bit selects 3-PWM vs 6-PWM input mode,
     *   - dead-time configuration,
     *   - OCP / gate-drive-current settings appropriate for the
     *     SiZF660LDT MOSFETs on this board,
     *   - how a phase is put into a floating/Hi-Z state while INHx is
     *     idle (needed for BEMF sensing - see the note in
     *     commutation.h about the driver's per-phase behavior with
     *     INLx tied low), rather than assuming it from this driver's
     *     PWM duty alone.
     * Then replace this function with the real edl7141_write_reg()
     * calls. Do not enable TIM1/apply duty on real hardware until
     * this is done and verified with the driver's nFAULT output
     * monitored.
     */
}
