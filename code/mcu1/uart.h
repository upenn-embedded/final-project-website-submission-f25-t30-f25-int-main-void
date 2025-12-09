#ifndef UART0_H
#define	UART0_H

// according to datasheet and library used in lab
void uart_init(unsigned int baud);
void uart_printChar(unsigned int c);
void uart_printString(char* s);

#endif	/* UART0_H */

