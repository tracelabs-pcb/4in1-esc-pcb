#ifndef EDL7141_SPI_H
#define EDL7141_SPI_H

#include <stdint.h>

/*
 * Low-level SPI transaction layer for the 6EDL7141 gate driver.
 *
 * Frame format (per Infineon 6EDL SPI-Link documentation): 24 bits =
 * 1 R/W bit + 7-bit register address + 16-bit data, MSB first, CS low
 * for the whole frame. Sent here as three bytes over SPI1 with PB12 as
 * a software-managed chip select (PB12 is not the SPI1 hardware NSS
 * pin, so NSS is left in software/soft-slave-select mode).
 *
 * IMPORTANT / SAFETY NOTE:
 * The actual register addresses and bit-field layout of the 6EDL7141
 * (e.g. which register/bits select 3-PWM mode, dead time, OCP
 * thresholds, gate drive strength) are NOT filled in below. I could
 * not verify the authoritative register map (datasheet section 8.2)
 * from available sources in this session, and guessing exact values
 * here risks mis-configuring the gate driver on real, populated
 * hardware (shoot-through, wrong OCP level, etc.). Fill in
 * edl7141_configure_3pwm_mode() from the real datasheet register map
 * before ever enabling PWM/spinning a motor on this board.
 */

void edl7141_spi_init(void);

uint16_t edl7141_write_reg(uint8_t addr7, uint16_t data);
uint16_t edl7141_read_reg(uint8_t addr7);

/* STUB: intentionally left unimplemented, see safety note above. */
void edl7141_configure_3pwm_mode(void);

#endif /* EDL7141_SPI_H */
