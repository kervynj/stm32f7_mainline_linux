#include <stdlib.h>
#include <stdint.h>

#include "stm32f4_regs.h"
#include "usart.h"
#include "gpio.h"
#include "mpu.h"
#include "start_kernel.h"
#include "qspi.h"

#define CONFIG_HSE_HZ	8000000
#define CONFIG_PLL_M	8
#define CONFIG_PLL_N	432
#define CONFIG_PLL_P	2
#define CONFIG_PLL_Q	9
#define PLLCLK_HZ (((CONFIG_HSE_HZ / CONFIG_PLL_M) * CONFIG_PLL_N) / CONFIG_PLL_P)
#if PLLCLK_HZ == 216000000
#define FLASH_LATENCY	9
#else
#error PLL clock does not match 216 MHz
#endif

static void *usart_base = (void *)USART1_BASE;
static void *gpio_base = (void *)GPIOA_BASE;

static void clock_setup(void)
{
	volatile uint32_t *RCC_CR = (void *)(RCC_BASE + 0x00);
	volatile uint32_t *RCC_PLLCFGR = (void *)(RCC_BASE + 0x04);
	volatile uint32_t *RCC_CFGR = (void *)(RCC_BASE + 0x08);
	volatile uint32_t *FLASH_ACR = (void *)(FLASH_BASE + 0x00);
	volatile uint32_t *RCC_AHB1ENR = (void *)(RCC_BASE + 0x30);
	volatile uint32_t *RCC_AHB2ENR = (void *)(RCC_BASE + 0x34);
	volatile uint32_t *RCC_AHB3ENR = (void *)(RCC_BASE + 0x38);
	volatile uint32_t *RCC_APB1ENR = (void *)(RCC_BASE + 0x40);
	volatile uint32_t *RCC_APB2ENR = (void *)(RCC_BASE + 0x44);
	volatile uint32_t *RCC_AHB3RST = (void *)(RCC_BASE + 0x18);
	uint32_t val;


	*RCC_CR |= RCC_CR_HSEON; // crystal used as source, enable oscillator
	while (!(*RCC_CR & RCC_CR_HSERDY)) {
	}

	val = *RCC_CFGR;
	val &= ~RCC_CFGR_HPRE_MASK; 	//AHB clock 216MHz
	//val |= 0 << 4; // not divided
	val &= ~RCC_CFGR_PPRE1_MASK; 	// APB1 set for 54MHz
	val |= 0x5 << 10; // divided by 4
	val &= ~RCC_CFGR_PPRE2_MASK; 	//APB2 set for 108MHz
	val |= 0x4 << 13; // divided by 2
	*RCC_CFGR = val;

	val = 0;
	val |= RCC_PLLCFGR_PLLSRC_HSE;
	val |= CONFIG_PLL_M;
	val |= CONFIG_PLL_N << 6;
	val |= ((CONFIG_PLL_P >> 1) - 1) << 16;
	val |= CONFIG_PLL_Q << 24;
	*RCC_PLLCFGR = val;

	*RCC_CR |= RCC_CR_PLLON;
	while (!(*RCC_CR & RCC_CR_PLLRDY));

	*FLASH_ACR = FLASH_ACR_ICEN | FLASH_ACR_PRFTEN | FLASH_LATENCY;

	*RCC_CFGR &= ~RCC_CFGR_SW_MASK;
	*RCC_CFGR |= RCC_CFGR_SW_PLL;
	while ((*RCC_CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL) {
	}

	/*  Enable all clocks, unused ones will be gated at end of kernel boot */
	*RCC_AHB1ENR |= 0x7ef417ff;
	*RCC_AHB2ENR |= 0xf1;
	*RCC_AHB3ENR |= 0x3;
	*RCC_APB1ENR |= 0xf6fec9ff;
	*RCC_APB2ENR |= 0x4777f33;

	/* togle reset QSPI */
	*RCC_AHB3RST |= 0x2;
	*RCC_AHB3RST &= 0xfffffffd;

}



static void fmc_wait_busy(void)
{
	volatile uint32_t *FMC_SDSR = (void *)(FMC_BASE + 0x158);

	while ((*FMC_SDSR & FMC_SDSR_BUSY)) {
	}
}

int main(void)
{
	volatile uint32_t *FLASH_KEYR = (void *)(FLASH_BASE + 0x04);
	volatile uint32_t *FLASH_CR = (void *)(FLASH_BASE + 0x10);
	volatile uint32_t *FMC_SDCR1 = (void *)(FMC_BASE + 0x140);
	volatile uint32_t *FMC_SDCR2 = (void *)(FMC_BASE + 0x144);
	volatile uint32_t *FMC_SDTR1 = (void *)(FMC_BASE + 0x148);
	volatile uint32_t *FMC_SDTR2 = (void *)(FMC_BASE + 0x14C);
	volatile uint32_t *FMC_SDCMR = (void *)(FMC_BASE + 0x150);
	volatile uint32_t *FMC_SDRTR = (void *)(FMC_BASE + 0x154);
	volatile uint32_t *SYSCFG_MEMRMP = (void *)(SYSCFG_BASE + 0x00);

	struct qspi_params qspi_756_params = {
		.address_size = QUADSPI_CCR_ADSIZE_32BITS,
		.fifo_threshold = QUADSPI_CR_FTHRES(3),
		.sshift = QUADSPI_CR_SSHIFT,
		.fsize = QUADSPI_DCR_FSIZE_64MB,
		.prescaler = 1,
		.dummy_cycle = 10,
		.fsel = 0,
		.dfm = 0,
	};
	int i;

	mpu_config(0xc0000000);

	if (*FLASH_CR & FLASH_CR_LOCK) { //unlock flash control register
		*FLASH_KEYR = 0x45670123;
		*FLASH_KEYR = 0xCDEF89AB;
	}
	*FLASH_CR &= ~(FLASH_CR_ERRIE | FLASH_CR_EOPIE | FLASH_CR_PSIZE_MASK); //disable error interrupts
	*FLASH_CR |= FLASH_CR_PSIZE_X32;
	*FLASH_CR |= FLASH_CR_LOCK; // lock flash control register

	clock_setup();

	gpio_set_fmc(gpio_base, 'D', 0); //D2
	gpio_set_fmc(gpio_base, 'D', 1); //D3
	gpio_set_fmc(gpio_base, 'D', 8); //D13
	gpio_set_fmc(gpio_base, 'D', 9); //D14
	gpio_set_fmc(gpio_base, 'D', 10); //D15
	gpio_set_fmc(gpio_base, 'D', 14); //D0
	gpio_set_fmc(gpio_base, 'D', 15); //D1
	gpio_set_fmc(gpio_base,'E', 0); //NBL0
	gpio_set_fmc(gpio_base,'E', 1); //NBL1
	gpio_set_fmc(gpio_base, 'E', 7); //D4
	gpio_set_fmc(gpio_base, 'E', 8); //D5
	gpio_set_fmc(gpio_base, 'E', 9); //D6
	gpio_set_fmc(gpio_base, 'E', 10); //D7
	gpio_set_fmc(gpio_base, 'E', 11); //D8
	gpio_set_fmc(gpio_base, 'E', 12); //D9
	gpio_set_fmc(gpio_base, 'E', 13); //D10
	gpio_set_fmc(gpio_base, 'E', 14); //D11
	gpio_set_fmc(gpio_base, 'E', 15); //D12
	gpio_set_fmc(gpio_base, 'F', 0); //A0
	gpio_set_fmc(gpio_base, 'F', 1); //A1
	gpio_set_fmc(gpio_base, 'F', 2); //A2
	gpio_set_fmc(gpio_base, 'F', 3); //A3
	gpio_set_fmc(gpio_base, 'F', 4); //A4
	gpio_set_fmc(gpio_base, 'F', 5); //A5
	gpio_set_fmc(gpio_base, 'F', 11); //SDNRAS
	gpio_set_fmc(gpio_base, 'F', 12); //A6
	gpio_set_fmc(gpio_base, 'F', 13); //A7
	gpio_set_fmc(gpio_base, 'F', 14); //A8
	gpio_set_fmc(gpio_base, 'F', 15); //A9
	gpio_set_fmc(gpio_base, 'G', 0); //A10
	gpio_set_fmc(gpio_base, 'G', 1); //A11
	gpio_set_fmc(gpio_base, 'G', 8); //SDCLK
	gpio_set_fmc(gpio_base, 'G', 15); //SDNCAS
	gpio_set_fmc(gpio_base, 'C', 5); //SDCKE0
	gpio_set_fmc(gpio_base, 'C', 4); //SDNE0
	gpio_set_fmc(gpio_base, 'A', 7); //SDNWE

	*FMC_SDCR1 |= 0x000019D4; // 8 column address bits, 12 row address bits
	//*FMC_SDCR2 = 0x000019D4;
	//*FMC_SDTR1 = 0x00106000;
	*FMC_SDTR1 = 0x01116371;

	fmc_wait_busy();
	*FMC_SDCMR = 0x00000011; // clock
	for (i = 0; i < 50000000; i++) { // 10 ms
		asm volatile ("nop");
	}
	fmc_wait_busy();
	*FMC_SDCMR = 0x00000012; // PALL
	fmc_wait_busy();
	*FMC_SDCMR = 0x000001F3; // auto-refresh
	fmc_wait_busy();
	*FMC_SDCMR = 0x00046014; // external memory mode, CAS latency of 3
	*FMC_SDRTR = 1650<<1; // refresh rate
	*FMC_SDCR1 &= 0xFFFFFDFF;
	fmc_wait_busy();

	gpio_set_qspi(gpio_base, 'B', 6, GPIOx_PUPDR_PULLUP, 0xa); //CS
	gpio_set_qspi(gpio_base, 'B', 2, GPIOx_PUPDR_NOPULL, 0x9); //CLK
	gpio_set_qspi(gpio_base, 'F', 8, GPIOx_PUPDR_NOPULL, 0xa); //DO
	gpio_set_qspi(gpio_base, 'F', 9, GPIOx_PUPDR_NOPULL, 0xa); //D1
	gpio_set_qspi(gpio_base, 'F', 7, GPIOx_PUPDR_NOPULL, 0x9); //D2
	gpio_set_qspi(gpio_base, 'F', 6, GPIOx_PUPDR_NOPULL, 0x9); //D3

	//quadspi_init(&qspi_756_params, (void *)QUADSPI_BASE);

	//*SYSCFG_MEMRMP = SYSCFG_MEMRMP_SWP_FMC << 10;

	gpio_set_usart(gpio_base,'A', 9, 7);
	gpio_set_usart(gpio_base,'A', 10, 7);


	usart_setup(usart_base, 108000000);
/*
	volatile uint32_t write_pointer=0x6000c400;
	*(uint32_t *)(write_pointer) = (uint32_t) 'd'; 
	uint32_t val = *(uint32_t *) write_pointer;
	usart_putch(usart_base, (val));

	volatile uint32_t write_pointer2=0x60000004;
	*(uint32_t *)(write_pointer2) = (uint32_t) 'c'; 
	uint32_t val2 = *(uint32_t *) write_pointer2;
	usart_putch(usart_base, (val2));
*/
	usart_putch(usart_base, '.');

	start_kernel();

	return 0;
}

static void noop(void)
{
	usart_putch(usart_base, 'E');
	while (1) {
	}
}

extern unsigned int _end_text;
extern unsigned int _start_data;
extern unsigned int _end_data;
extern unsigned int _start_bss;
extern unsigned int _end_bss;

void reset(void)
{
	unsigned int *src, *dst;

	asm volatile ("cpsid i");

	src = &_end_text;
	dst = &_start_data;
	while (dst < &_end_data) {
		*dst++ = *src++;
	}

	dst = &_start_bss;
	while (dst < &_end_bss) {
		*dst++ = 0;
	}

	main();
}

extern unsigned long _stack_top;

__attribute__((section(".vector_table")))
void (*vector_table[16 + 91])(void) = {
	(void (*))&_stack_top,
	reset,
	noop,
	noop,
	noop,
	noop,
	noop,
	NULL,
	NULL,
	NULL,
	NULL,
	noop,
	noop,
	NULL,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
};

