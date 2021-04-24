#include <stdint.h>

#include "sdmmc.h"

void sdmmc_init(void *base)
{
	volatile uint32_t *SDMMC_PWR	= base + 0x00;
	volatile uint32_t *SDMMC_CLKCR	= base + 0x04;

	// init CLK at 400KHz for card discovery
	*SDMMC_PWR   |= SDMMC_PWR_PWR_EN;
	*SDMMC_CLKCR = SDMMC_DMODE_4_LINE | SDMMC_CLKCR_DIVIDER(1); // default use D0 for data, 400KHz clock
	
	*SDMMC_CLKCR |= SDMMC_CLKCR_CLKEN; // enable clock



}

