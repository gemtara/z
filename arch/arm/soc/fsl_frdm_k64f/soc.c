/*
 * Copyright (c) 2014-2015 Wind River Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file
 * @brief System/hardware module for fsl_frdm_k64f platform
 *
 * This module provides routines to initialize and support board-level
 * hardware for the fsl_frdm_k64f platform.
 */

#include <nanokernel.h>
#include <device.h>
#include <init.h>
#include <soc.h>
#include <drivers/k20_mcg.h>
#include <uart.h>
#include <drivers/k20_pcr.h>
#include <drivers/k20_sim.h>
#include <drivers/k6x_mpu.h>
#include <drivers/k6x_pmc.h>
#include <sections.h>

#include <arch/cpu.h>


/* board's setting for PLL multipler (PRDIV0) */
#define FRDM_K64F_PLL_DIV_20 (20 - 1)

/* board's setting for PLL multipler (VDIV0) */
#define FRDM_K64F_PLL_MULT_48 (48 - 24)

/*
 * K64F Flash configuration fields
 * These 16 bytes, which must be loaded to address 0x400, include default
 * protection and security settings.
 * They are loaded at reset to various Flash Memory module (FTFE) registers.
 *
 * The structure is:
 * -Backdoor Comparison Key for unsecuring the MCU - 8 bytes
 * -Program flash protection bytes, 4 bytes, written to FPROT0-3
 * -Flash security byte, 1 byte, written to FSEC
 * -Flash nonvolatile option byte, 1 byte, written to FOPT
 * -Reserved, 1 byte, (Data flash protection byte for FlexNVM)
 * -Reserved, 1 byte, (EEPROM protection byte for FlexNVM)
 *
 */
uint8_t __security_frdm_k64f_section __security_frdm_k64f[] = {
	/* Backdoor Comparison Key (unused) */
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* Program flash protection; 1 bit/region - 0=protected, 1=unprotected
	 */
	0xFF, 0xFF, 0xFF, 0xFF,
	/*
	 * Flash security: Backdoor key disabled, Mass erase enabled,
	 *                 Factory access enabled, MCU is unsecure
	 */
	0xFE,
	/* Flash nonvolatile option: NMI enabled, EzPort enabled, Normal boot */
	0xFF,
	/* Reserved for FlexNVM feature (unsupported by this MCU) */
	0xFF, 0xFF};

/**
 *
 * @brief Initialize the system clock
 *
 * This routine will configure the multipurpose clock generator (MCG) to
 * set up the system clock.
 * The MCG has nine possible modes, including Stop mode.  This routine assumes
 * that the current MCG mode is FLL Engaged Internal (FEI), as from reset.
 * It transitions through the FLL Bypassed External (FBE) and
 * PLL Bypassed External (PBE) modes to get to the desired
 * PLL Engaged External (PEE) mode and generate the maximum 120 MHz system
 * clock.
 *
 * @return N/A
 *
 */

static void clkInit(void)
{
	uint8_t temp_reg;
	K20_MCG_t *mcg_p = (K20_MCG_t *)PERIPH_ADDR_BASE_MCG; /* clk gen. ctl */

	/*
	 * Select the 50 Mhz external clock as the MCG OSC clock.
	 * MCG Control 7 register:
	 * - Select OSCCLK0 / XTAL
	 */

	temp_reg = mcg_p->c7 & ~MCG_C7_OSCSEL_MASK;
	temp_reg |= MCG_C7_OSCSEL_OSC0;
	mcg_p->c7 = temp_reg;

	/*
	 * Transition MCG from FEI mode (at reset) to FBE mode.
	 */

	/*
	 * MCG Control 2 register:
	 * - Set oscillator frequency range = very high for 50 MHz external
	 * clock
	 * - Set oscillator mode = low power
	 * - Select external reference clock as the oscillator source
	 */

	temp_reg = mcg_p->c2 &
		   ~(MCG_C2_RANGE_MASK | MCG_C2_HGO_MASK | MCG_C2_EREFS_MASK);

	temp_reg |=
		(MCG_C2_RANGE_VHIGH | MCG_C2_HGO_LO_PWR | MCG_C2_EREFS_EXT_CLK);

	mcg_p->c2 = temp_reg;

	/*
	 * MCG Control 1 register:
	 * - Set system clock source (MCGOUTCLK) = external reference clock
	 * - Set FLL external reference divider = 1024 (MCG_C1_FRDIV_32_1024)
	 *   to get the FLL frequency of 50 MHz/1024 = 48.828KHz
	 *   (Note: If FLL frequency must be in the in 31.25KHz-39.0625KHz
	 *range,
	 *          the FLL external reference divider = 1280
	 *(MCG_C1_FRDIV_64_1280)
	 *          to get 50 MHz/1280 = 39.0625KHz)
	 * - Select the external reference clock as the FLL reference source
	 *
	 */

	temp_reg = mcg_p->c1 &
		   ~(MCG_C1_CLKS_MASK | MCG_C1_FRDIV_MASK | MCG_C1_IREFS_MASK);

	temp_reg |=
		(MCG_C1_CLKS_EXT_REF | MCG_C1_FRDIV_32_1024 | MCG_C1_IREFS_EXT);

	mcg_p->c1 = temp_reg;

	/*
	 * Confirm that the external reference clock is the FLL reference
	 * source
	 */

	while ((mcg_p->s & MCG_S_IREFST_MASK) != 0)
		;
	;

	/*
	 * Confirm the external ref. clock is the system clock source
	 * (MCGOUTCLK)
	 */

	while ((mcg_p->s & MCG_S_CLKST_MASK) != MCG_S_CLKST_EXT_REF)
		;
	;

	/*
	 * Transition to PBE mode.
	 * Configure the PLL frequency in preparation for PEE mode.
	 * The goal is PEE mode with a 120 MHz system clock source (MCGOUTCLK),
	 * which is calculated as (oscillator clock / PLL divider) * PLL
	 * multiplier,
	 * where oscillator clock = 50MHz, PLL divider = 20 and PLL multiplier =
	 * 48.
	 */

	/*
	 * MCG Control 5 register:
	 * - Set the PLL divider
	 */

	temp_reg = mcg_p->c5 & ~MCG_C5_PRDIV0_MASK;

	temp_reg |= FRDM_K64F_PLL_DIV_20;

	mcg_p->c5 = temp_reg;

	/*
	 * MCG Control 6 register:
	 * - Select PLL as output for PEE mode
	 * - Set the PLL multiplier
	 */

	temp_reg = mcg_p->c6 & ~(MCG_C6_PLLS_MASK | MCG_C6_VDIV0_MASK);

	temp_reg |= (MCG_C6_PLLS_PLL | FRDM_K64F_PLL_MULT_48);

	mcg_p->c6 = temp_reg;

	/* Confirm that the PLL clock is selected as the PLL output */

	while ((mcg_p->s & MCG_S_PLLST_MASK) == 0)
		;
	;

	/* Confirm that the PLL has acquired lock */

	while ((mcg_p->s & MCG_S_LOCK0_MASK) == 0)
		;
	;

	/*
	 * Transition to PEE mode.
	 * MCG Control 1 register:
	 * - Select PLL as the system clock source (MCGOUTCLK)
	 */

	temp_reg = mcg_p->c1 & ~MCG_C1_CLKS_MASK;

	temp_reg |= MCG_C1_CLKS_FLL_PLL;

	mcg_p->c1 = temp_reg;

	/* Confirm that the PLL output is the system clock source (MCGOUTCLK) */

	while ((mcg_p->s & MCG_S_CLKST_MASK) != MCG_S_CLKST_PLL)
		;
	;
}

/**
 *
 * @brief Perform basic hardware initialization
 *
 * Initialize the interrupt controller device drivers and the
 * Kinetis UART device driver.
 * Also initialize the timer device driver, if required.
 *
 * @return 0
 */

static int fsl_frdm_k64f_init(struct device *arg)
{
	ARG_UNUSED(arg);
	/* System Integration module */
	volatile struct K20_SIM *sim_p =
		(volatile struct K20_SIM *)PERIPH_ADDR_BASE_SIM;

	/* Power Mgt Control module */
	volatile struct K6x_PMC *pmc_p =
		(volatile struct K6x_PMC *)PERIPH_ADDR_BASE_PMC;

	/* Power Mgt Control module */
	volatile struct K6x_MPU *mpu_p =
		(volatile struct K6x_MPU *)PERIPH_ADDR_BASE_MPU;

	int oldLevel; /* old interrupt lock level */
	uint32_t temp_reg;

	/* disable interrupts */
	oldLevel = irq_lock();

	/* enable the port clocks */
	sim_p->scgc5.value |= (SIM_SCGC5_PORTA_CLK_EN | SIM_SCGC5_PORTB_CLK_EN |
			       SIM_SCGC5_PORTC_CLK_EN | SIM_SCGC5_PORTD_CLK_EN |
			       SIM_SCGC5_PORTE_CLK_EN);

	/* release I/O power hold to allow normal run state */
	pmc_p->regsc.value |= PMC_REGSC_ACKISO_MASK;

	/*
	 * Disable memory protection and clear slave port errors.
	 * Note that the K64F does not implement the optional ARMv7-M memory
	 * protection unit (MPU), specified by the architecture (PMSAv7), in the
	 * Cortex-M4 core.  Instead, the processor includes its own MPU module.
	 */
	temp_reg = mpu_p->ctrlErrStatus.value;
	temp_reg &= ~MPU_VALID_MASK;
	temp_reg |= MPU_SLV_PORT_ERR_MASK;
	mpu_p->ctrlErrStatus.value = temp_reg;

	/* clear all faults */

	_ScbMemFaultAllFaultsReset();
	_ScbBusFaultAllFaultsReset();
	_ScbUsageFaultAllFaultsReset();

	_ScbHardFaultAllFaultsReset();

	/*
	 * Initialize the clock dividers for:
	 * core and system clocks = 120 MHz (PLL/OUTDIV1)
	 * bus clock = 60 MHz (PLL/OUTDIV2)
	 * FlexBus clock = 40 MHz (PLL/OUTDIV3)
	 * Flash clock = 24 MHz (PLL/OUTDIV4)
	 */
	sim_p->clkdiv1.value = (
		(SIM_CLKDIV(CONFIG_K64_CORE_CLOCK_DIVIDER) <<
			SIM_CLKDIV1_OUTDIV1_SHIFT) |
		(SIM_CLKDIV(CONFIG_K64_BUS_CLOCK_DIVIDER) <<
			SIM_CLKDIV1_OUTDIV2_SHIFT) |
		(SIM_CLKDIV(CONFIG_K64_FLEXBUS_CLOCK_DIVIDER) <<
			SIM_CLKDIV1_OUTDIV3_SHIFT) |
		(SIM_CLKDIV(CONFIG_K64_FLASH_CLOCK_DIVIDER) <<
			SIM_CLKDIV1_OUTDIV4_SHIFT));

	/* Initialize PLL/system clock to 120 MHz */
	clkInit();

	/*
	 * install default handler that simply resets the CPU
	 * if configured in the kernel, NOP otherwise
	 */
	NMI_INIT();

	/* restore interrupt state */
	irq_unlock(oldLevel);
	return 0;
}

SYS_INIT(fsl_frdm_k64f_init, PRIMARY, 0);
