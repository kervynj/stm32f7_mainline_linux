#ifndef _USART_H
#define _USART_H

void usart_setup(void *base, uint32_t clk_freq);
void usart_putch(void *base, int8_t ch);
void usart_putString(void *base, int8_t * str);
void usart_putNumber(void *base, uint32_t x);

#endif /* _USART_H */
