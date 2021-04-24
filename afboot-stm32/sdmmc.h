#ifndef _SDMMC_H
#define _SDMMC_H

#include <stdint.h>

/* SDMMC_POWER */

#define SDMMC_PWR_PWR_EN	0x03

/* SDMMC_CLKCR */

#define SDMMC_CLKCR_DIVIDER(x)	(x << 0)
#define SDMMC_CLKCR_DMODE(x)	(x << 11)

#define SDMMC_CLKCR_CLKEN	(1 << 8)
#define SDMMC_CLKCR_PWRSV	(1 << 9)
#define SDMMC_CLKCR_BYPSS	(1 << 10)

#define SDMMC_DMODE_1_LINE	SDMMC_CLKCR_DMODE(0)
#define SDMMC_DMODE_4_LINE	SDMMC_CLKCR_DMODE(1)

void sdmmc_init(void *base);

#endif /* _SDMMC_H */
