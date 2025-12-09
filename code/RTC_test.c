/*
 * File:   newmain.c
 * Author: jingyuxin
 *
 * Created on November 13, 2025, 4:30 PM
 */


#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>
#include "uart.h"     


#define TWI_FREQ 100000UL   // 100kHz I2C

#define RTC_ADDR    0x68
#define ADDR_W  (RTC_ADDR << 1)
#define ADDR_R  (RTC_ADDR << 1 | 1)

#define REG_CONTROL_1   0x00
#define REG_CONTROL_2   0x01
#define REG_CONTROL_3   0x02
#define REG_SECONDS     0x03
#define REG_MINUTES     0x04
#define REG_HOURS       0x05
#define REG_DAYS        0x06
#define REG_WEEKDAYS    0x07
#define REG_MONTHS      0x08
#define REG_YEARS       0x09

//I2C set

void twi_init(void)
{
    // TWSR: TWI Prescaler bits in the TWSR(Status Register description)
    // Prescaler = 1;
    TWSR0 = 0x00;
    // SCL frequency = CPU Clock frequency / (16 + 2(TWBR)*(PrscalerValue))
    TWBR0 = (uint8_t)((F_CPU / TWI_FREQ - 16) / 2);
    // TWI Control Register (TWCRn)
    TWCR0 = (1 << TWEN);
}

void twi_start(uint8_t SLA_W)
{
    // TWI Send START condition
    //TWINT: TWI Interrupt Flag; TWEN:enable TWI; TWSTA: TWI START Condition
    TWCR0 = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    //wait for TWINT flag set indicates that the START condition has been transmitted
    while (!(TWCR0 & (1 << TWINT)));
    TWDR0 = SLA_W;
    TWCR0 = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR0 & (1 << TWINT)));
}

void twi_stop(void)
{
    // Transmit STOP condition
    TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
}

void twi_write(uint8_t DATA)
{
    //Load DATA into TWDR Register. Clear TWINT bit in TWCR to start transmission of data.
    TWDR0 = DATA;
    TWCR0 = (1 << TWINT) | (1 << TWEN);
    //Flag, DATA has been transmitted, ACK/NACK received
    while (!(TWCR0 & (1 << TWINT)));
}

uint8_t twi_read_ack(void)
{
    //TWEA TWI Enable Acknowledge
    TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    while (!(TWCR0 & (1 << TWINT)));
    return TWDR0;  
}

uint8_t twi_read_nack(void)
{
    TWCR0 = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR0 & (1 << TWINT)));
    return TWDR0;
}


//RTC module set

//data of time
typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;       
    uint8_t weekday;   
    uint8_t month;    
    uint8_t year;     
} rtc_time_t;

const char *weekday_name[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static uint8_t bin_to_bcd(uint8_t val)
{
    return (uint8_t)(((val / 10) << 4) | (val % 10));
}

static uint8_t bcd_to_bin(uint8_t val)
{
    return (uint8_t)((val >> 4) * 10 + (val & 0x0F));
}



void RTC_write_reg(uint8_t reg, uint8_t value)
{
    twi_start(ADDR_W);
    twi_write(reg);
    twi_write(value);
    twi_stop();
}

uint8_t RTC_read_reg(uint8_t reg)
{
    uint8_t val;
    twi_start(ADDR_W);
    twi_write(reg);
    twi_start(ADDR_R);
    val = twi_read_nack();
    twi_stop();
    return val;
}


void RTC_init(void)
{
    RTC_write_reg(REG_CONTROL_1, 0x00);
    RTC_write_reg(REG_CONTROL_2, 0x00);
    RTC_write_reg(REG_CONTROL_3, 0x00);
}


void RTC_set_time(uint8_t year, uint8_t mouth, uint8_t day, uint8_t wday, uint8_t hour, uint8_t minute, uint8_t second)
{
    twi_start(ADDR_W);
    twi_write(REG_SECONDS);

    // & 0x7F clean significant bit
    uint8_t sec_bcd = bin_to_bcd(second) & 0x7F;

    twi_write(sec_bcd);                 // second
    twi_write(bin_to_bcd(minute));      // minute
    twi_write(bin_to_bcd(hour));        // hour
    twi_write(bin_to_bcd(day));         // day
    twi_write(bin_to_bcd(wday));        // day of the week
    twi_write(bin_to_bcd(mouth));       // mouth
    twi_write(bin_to_bcd(year));        // year

    twi_stop();
}


uint8_t RTC_get_time(rtc_time_t *t)
{
    twi_start(ADDR_W);
    twi_write(REG_SECONDS);

    twi_start(ADDR_R);
    uint8_t sec = twi_read_ack();
    uint8_t min = twi_read_ack();
    uint8_t hrs = twi_read_ack();
    uint8_t day = twi_read_ack();
    uint8_t wday = twi_read_ack();
    uint8_t mon = twi_read_ack();
    uint8_t year = twi_read_nack();    
    twi_stop();

    t->seconds = bcd_to_bin((uint8_t)(sec & 0x7F));  
    t->minutes = bcd_to_bin((uint8_t)(min & 0x7F));
    t->hours   = bcd_to_bin((uint8_t)(hrs & 0x3F));  
    t->day     = bcd_to_bin((uint8_t)(day  & 0x3F));  
    t->weekday = (uint8_t)(wday & 0x07);              
    t->month   = bcd_to_bin((uint8_t)(mon  & 0x1F));  
    t->year    = bcd_to_bin(year);                    

    return 1;
}



int main(void)
{
    uart_init();     
    twi_init();
    RTC_init();

    //Set initial time
    RTC_set_time(25, 12, 31, 4, 23, 59, 30);  

    printf("RTC_test_demo\r\n");
    _delay_ms(1000);
  
    rtc_time_t t;

    while (1)
    {
        //printf("Loop before I2C\r\n");
        //_delay_ms(1000);
        
        if (RTC_get_time(&t))
            
        {
            //printf("Loop start\r\n"); 
            printf("Time: %02u:%02u:%02u  Date: 20%02u-%02u-%02u  %s\r\n",
                    t.hours, t.minutes, t.seconds,
                    t.year, t.month, t.day,
                    weekday_name[t.weekday]);
        
        }
        else
        {
            printf("RTC read error\r\n");
        }

        _delay_ms(1000);
           
    }

   
}
