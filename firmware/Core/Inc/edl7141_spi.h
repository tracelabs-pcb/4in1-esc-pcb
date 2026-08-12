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
#define EDL7141_ADDR_SUPPLY_ST      0x02U
#define EDL7141_ADDR_FUNC_ST        0x03U
#define EDL7141_ADDR_DEVICE_ID      0x07U

/* SUPPLY_ST (0x02) is a LIVE status register (unlike FAULT_ST, which
 * latches until cleared) - bits[5:0] read the current UVLO/OVLO state
 * of the charge pumps and internal regulators, b1 = above threshold
 * (healthy). Per the datasheet's fault table, VCCLS/VCCHS UVLO force
 * all MOSFET outputs to Hi-Z "independently of fault handling" - i.e.
 * independently of FAULT_ST/EN_DRV/nBRAKE - so this is worth checking
 * directly if INHx is confirmed reaching the driver (SPI, EN_DRV,
 * nBRAKE all good) but the gates still never move.
 *   bit0 VCCLS_UVST, bit1 VCCHS_UVST, bit2 DVDD_UVST,
 *   bit3 DVDD_OVST,  bit4 VDDB_UVST,  bit5 VDDB_OVST
 * All-healthy reads as 0x003F (bits 0-5 all 1); bits 12:6 are a PVDD
 * ADC reading (informational, not a threshold flag). */
#define EDL7141_SUPPLY_ST_ALL_HEALTHY 0x003FU
#define EDL7141_ADDR_FAULTS_CLR     0x10U
#define EDL7141_ADDR_PWM_CFG        0x13U
#define EDL7141_ADDR_CSAMP_CFG      0x1DU

/* FAULTS_CLR (0x10): bit0 CLR_FLTS (clear non-latched faults), bit1
 * CLR_LATCH (clear latched faults). Per the datasheet, FAULT_ST bits
 * stay set once tripped "independently of latch configuration" until
 * explicitly cleared - so a one-off startup transient (e.g. from the
 * CS_OCP_FLT false trigger before CSAMP_CFG was fixed) can leave a
 * stale bit sitting in FAULT_ST forever even after the real cause is
 * gone. Write both bits to clear everything regardless of latch type. */
#define EDL7141_FAULTS_CLR_ALL      0x0003U

/* PWM_CFG (0x13) reset value 0x0000 = PWM_MODE b000 = 6PWM mode. */
#define EDL7141_PWM_MODE_6PWM       0x0000U

/* CSAMP_CFG (0x1D) reset value 0x0028 = CS_GAIN_ANA=1, CS_EN=b010 (only
 * phase B's current-sense amplifier enabled - an odd reset default,
 * but confirmed against the datasheet register table). This board has
 * no shunt resistors wired to the driver's CSNx/CSOx pins at all (a
 * separate INA180A3 + STM32 ADC handles total board current instead),
 * so phase B's amplifier floats and its internal OCP comparator trips
 * on noise (CS_OCP_FLT in FAULT_ST). Fix: disable all three amplifiers
 * (CS_EN=000), keeping CS_GAIN_ANA at its reset value untouched - this
 * is exactly what the datasheet recommends for unused current-sense
 * amplifiers. CS_EN is "Always" programmable, no EN_DRV sequencing
 * constraint, but configure it before EN_DRV anyway to avoid ever
 * seeing the spurious fault at all. */
#define EDL7141_CSAMP_CFG_CS_DISABLED 0x0008U

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

/* Disables all three internal current-sense amplifiers (CS_EN=000 in
 * CSAMP_CFG) - see the long comment above EDL7141_CSAMP_CFG_CS_DISABLED.
 * Call this on boards where the CSNx/CSOx pins aren't wired to real
 * shunt resistors, to avoid spurious CS_OCP_FLT faults. */
void edl7141_disable_unused_current_sense(void);

/* Clears all fault status bits (latched and non-latched) in FAULT_ST. */
void edl7141_clear_faults(void);

#endif /* EDL7141_SPI_H */
