#ifndef EDL7141_SPI_H
#define EDL7141_SPI_H

#include <stdint.h>

/*
 * Low-level SPI driver for the 6EDL7141 gate driver, verified against
 * Infineon 6EDL7141 datasheet Rev. 1.02 (2021-09-27), sections 7.1.2
 * and 8.2.
 *
 * Frame format: 24 bits, MSB first, CS low for the whole frame:
 *   bit 23      : command (1 = register write, 0 = register read)
 *   bits 22..16 : 7-bit register address
 *   bits 15..0  : 16-bit data (ignored by the device on a read)
 * Data is sampled on the falling SPI clock edge -> SPI mode 1
 * (CPOL=0, CPHA=1). Confirmed against the datasheet's own worked
 * example (write 0xFE01 to TDRIVE_SRC_CFG/0x19 -> bytes 0x99 0xFE 0x01,
 * i.e. 0x99 = (1<<7)|0x19), which matches this driver's rw_bit=1-for-
 * write convention exactly.
 */

#define EDL7141_ADDR_FAULT_ST       0x00U
#define EDL7141_ADDR_DEVICE_ID      0x07U
#define EDL7141_ADDR_PWM_CFG        0x13U

/* PWM_CFG (0x13) reset value 0x0000 = PWM_MODE b000 = 6PWM mode. */
#define EDL7141_PWM_MODE_6PWM       0x0000U

/* DEVICE_ID (0x07) reset value 0x0006, DEV_ID in bits[3:0] -> read-only,
 * usable as a "is SPI actually talking to the chip" sanity check. */
#define EDL7141_DEVICE_ID_EXPECTED  0x0006U

void edl7141_spi_init(void);

uint16_t edl7141_write_reg(uint8_t addr7, uint16_t data);
uint16_t edl7141_read_reg(uint8_t addr7);

/* Returns 1 if DEVICE_ID reads back as expected (SPI link + CS wiring
 * are working), 0 otherwise. Call before touching PWM_CFG/EN_DRV. */
int edl7141_check_device_id(void);

/*
 * Explicitly writes PWM_CFG = 6PWM mode (PWM_MODE = b000).
 *
 * This is deliberately 6PWM, not 3PWM, and this is not optional for
 * this board: with INLx hard-wired to GND (per the schematic), 3PWM
 * mode's truth table (datasheet Table 9) ties GLx HIGH whenever INHx
 * is low - i.e. the "off" phase would have its low-side FET driven ON
 * continuously instead of floating, which pulls the phase node to GND
 * and destroys the back-EMF zero-cross reading on the LM2901 comparator.
 * In 6PWM mode (datasheet Table 8), with INLx=0, INHx=0 correctly
 * produces GHx=LOW/GLx=LOW/SHx=High-Z - a genuinely floating phase,
 * which is what the external comparator + VSTAR network needs. 6PWM is
 * also the register's power-on-reset default (PWM_CFG resets to
 * 0x0000), so this call is a safety-net / explicit statement of intent
 * rather than a functional necessity on an unprogrammed part - but do
 * not "simplify" it away, and do not switch this board to 3PWM mode.
 *
 * PWM_MODE is a "Standby"-programmable bitfield (datasheet Table 20):
 * it only takes effect while EN_DRV is low, so this must be called
 * before EN_DRV is raised. This board's EN_DRV/CE pin wiring was not
 * part of the pin list you gave me - confirm on the schematic how
 * EN_DRV and CE are driven (dedicated GPIO vs. hardwired) before
 * relying on this sequencing.
 */
void edl7141_configure_pwm_mode(void);

#endif /* EDL7141_SPI_H */
