#include "uart.h"
#include <avr/io.h>

void uart_init(unsigned int baud)
{
    // set baud rate
    UBRR0L = (unsigned char) baud;
    UBRR0H = (unsigned char) (baud>>8); // >>8 since UBRR0L 8 bits, UBRR0H 4 bits.
    UCSR0B |= (1<<RXEN0); // receiver enable
    UCSR0B |= (1<<TXEN0); // transmitter enable
    
    // asynchronous
    UCSR0C &= ~(1<<UMSEL00);
    UCSR0C &= ~(1<<UMSEL01);
    
    // disable parity
    UCSR0C &= ~(1<<UPM00);
    UCSR0C &= ~(1<<UPM01);
    
    // 1 stop bit, since esp32 uses this as default OR change esp32 Serial1.begin
    UCSR0C &= ~(1<<USBS0);
    
    // 8 data bits
    UCSR0C |= (1<<UCSZ00);
    UCSR0C |= (1<<UCSZ01);
}
void uart_printChar(unsigned int c)
{
    while (!(UCSR0A&(1<<UDRE0))); // USART data register empty? UDRE0=1 when empty
    UDR0 = c; // write char to USART I/O data register 0
}
void uart_printString(char* s)
{
    for(uint8_t i=0; s[i]!='\0'; i++)
    {
        uart_printChar(s[i]); // char by char
    }
}