/*
 * test of water pump
 */

#define F_CPU 16000000UL 
#include <xc.h>
#include <util/delay.h>

int main(void) {
    DDRB |= (1 << PB1);
    PORTB &= ~(1 << PB1);
    while (1) {
        PORTB |= (1 << PB1);  
        // delay 3s
        for(int i = 0; i < 3; i++) {
            _delay_ms(1000); 
        }
        // motor stop for 6s
        PORTB &= ~(1 << PB1); 
        for(int i = 0; i < 6; i++) {
            _delay_ms(1000);
        }
    }
    return 0;
}