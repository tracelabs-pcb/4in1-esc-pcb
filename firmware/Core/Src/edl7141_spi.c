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

int edl7141_check_device_id(void)
{
    uint16_t id = edl7141_read_reg(EDL7141_ADDR_DEVICE_ID) & 0x000FU;
    return id == (EDL7141_DEVICE_ID_EXPECTED & 0x000FU);
}

void edl7141_configure_pwm_mode(void)
{
    /* See the long comment in edl7141_spi.h: 3PWM, not 6PWM, for now -
     * temporary while verifying open-loop spin (no BEMF sensing
     * involved yet). Revisit before closed-loop testing. */
    edl7141_write_reg(EDL7141_ADDR_PWM_CFG, EDL7141_PWM_MODE_3PWM);
}

void edl7141_disable_unused_current_sense(void)
{
    edl7141_write_reg(EDL7141_ADDR_CSAMP_CFG, EDL7141_CSAMP_CFG_CS_DISABLED);
}

void edl7141_clear_faults(void)
{
    edl7141_write_reg(EDL7141_ADDR_FAULTS_CLR, EDL7141_FAULTS_CLR_ALL);
}
