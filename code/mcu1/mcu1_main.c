/*
 * File:   main.c
 * Author: Peiyu
 *
 * Created on November 13, 2025, 10:43 AM
 */

#define F_CPU 16000000UL
#define BAUD_RATE 9600
#define BAUD F_CPU/BAUD_RATE/16 - 1 // from datasheet
#define STRLEN 10 // length of char array for data
#define SHT40_HEAT_TH 5 // SHT40 heat before measurement if temperature drops below a threshold
#define TEMP_MIN -20
#define TEMP_MAX 80
#define RH_MIN 0
#define RH_MAX 100
#define INTERVAL 5

// common
#include <xc.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// 160x128 screen Timer0 used by screen
#include "ST7735.h"
#include "LCD_GFX.h"
#include "ASCII_LUT.h"

// uart0 to print and to esp32
#include "uart.h"

// sht40 and i2c0
#include "sht40.h"

// if NOT read successfully, obviously wrong data shown.
volatile float temperature = 999;
volatile float humidity = 999;
// default thresholds
volatile int temp_threshold = 10;
volatile int rh_threshold = 50;
volatile uint8_t mode = 0; // mode = (mode + 1) % 3. possible modes: 0, 1, 2

// time
volatile uint8_t days = 0;
volatile uint8_t hours = 0;
volatile uint8_t minutes = 0;
volatile uint8_t sec_4 = 0;

// store data as string
char str_temperature[STRLEN] = "";
char str_humidity[STRLEN] = "";
char str_temp_threshold[STRLEN] = "";
char str_rh_threshold[STRLEN] = "";
char str_ADC[STRLEN] = "";
char str_minutes[STRLEN] = "";
char str_hours[STRLEN] = "";
char str_days[STRLEN] = "";

// prototypes
void sht40_softReset();
void sht40_MeasureMode();
void sht40_Measure();
void MeasureTen();
void ScreenSetClear();
void ShowDataScreen();
void TransmitData();
void SetTH_init();
void SetThresholds();
void WaterPump_init();
void WaterPump(uint8_t speed);
void Time_init();
void Time();

ISR(TIMER4_COMPA_vect)
{
    //count time OCF4A automatically cleared
    sec_4 += 1;
    if (sec_4 == 15)
    {
        minutes += 1;
        sec_4 = 0;
    }
}

ISR(TIMER3_CAPT_vect) // datasheet 15.1
{
    //TIFR3 |= (1<<ICF3); // Automatically cleared. Clear Input Capture Flag in TC3 Interrupt Flag Register    
    if (PINE & (1<<PINE2))
    {
        mode = (mode + 1) % 3; // 0, 1, 2 possible
    }
}

int main(void)
{
    cli();
    uart_init(BAUD);
    i2c0_init();
    sht40_softReset();
    SetTH_init();
    ScreenSetClear();
    WaterPump_init();
    Time_init();
    float temp_diff = 0;
    float rh_diff = 0;
    uint16_t speed = 0;
    uint8_t flag = 1;
    sei();
    while(1)
    {
        MeasureTen();
        SetThresholds();
        ShowDataScreen();
        Time();
        TransmitData();
        temp_diff = temperature - temp_threshold;
        rh_diff = rh_threshold - humidity;
        flag = minutes % INTERVAL;
        if ((flag == 0) && (temp_diff > 0) && (rh_diff > 0))
        {
            speed = (uint16_t) (temp_diff * 25 + rh_diff * 10); // with different weights
            if (speed > 255)
            {
                WaterPump(255);
            }
            else
            {
                WaterPump((uint8_t) speed);
            }
        }
        else
        {
            // Turn OFF LED
            PORTD &= ~(1<<PORTD7);
            OCR2B = 0; // 0% duty cycle, turn OFF water pump
        }
    }
    
    return 0;
}

// SHT40 soft reset
void sht40_softReset()
{
    // START
    i2c0_start();
    _delay_ms(1); // wait for sensor
    // WRITE
    sht40_writeTo();
    _delay_ms(1); // wait for sensor
    // 0x94 soft reset
    sht40_sendCMD(0x94);
    _delay_ms(50); // wait for sensor
    // STOP
    i2c0_stop();
    _delay_ms(1); // wait for sensor
}

// Let SHT40 measure in a mode
void sht40_MeasureMode()
{
    // START
    i2c0_start();
    _delay_ms(1); // wait for sensor
    // WRITE
    sht40_writeTo();
    _delay_ms(1); // wait for sensor
    // Measure in X mode, depends on temperature
    if(temperature<SHT40_HEAT_TH)
    {
        sht40_sendCMD(0x15); // heat 0.1s before high precision measurement
        _delay_ms(105); // wait for sensor
    }
    else
    {
        sht40_sendCMD(0xFD); // high precision measurement
        _delay_ms(1); // wait for sensor
    }
    // STOP
    i2c0_stop();
    _delay_ms(25); // wait for sensor to measure
}

void sht40_Measure()
{
    // Start
    i2c0_start();
    _delay_ms(1); // wait for sensor
    // READ
    sht40_readFrom();
    _delay_ms(1); // wait for sensor
    // Read 6 bytes with NACK from ATmega328PB to SHT40 for the last data byte
    uint16_t t_l = sht40_readByte();
    _delay_ms(1); // wait for sensor
    uint16_t t_h = sht40_readByte();
    _delay_ms(1); // wait for sensor
    sht40_readByte(); // crc byte
    _delay_ms(1); // wait for sensor
    uint16_t rh_l = sht40_readByte();
    _delay_ms(1); // wait for sensor
    uint16_t rh_h = sht40_readByte();
    _delay_ms(1); // wait for sensor
    TWCR0 &= ~(1<<TWEA); // NACK for last data byte
    sht40_readByte(); // crc byte
    _delay_ms(1); // wait for sensor
    // STOP
    i2c0_stop();
    _delay_ms(25); // wait for sensor
    
    // apply datasheet formula, get temperature and humidity
    // temperature
    temperature = (float) ((t_h<<8) | t_l) * 175 / 65535 - 45;
    
    // relative humidity
    humidity = (float) ((rh_h<<8) | rh_l) * 125 /65535 - 6;
}

// measure 10 times before make decision
void MeasureTen()
{
    float temp = 0;
    float humi = 0;
    for(uint8_t i=0; i<10; i++)
    {
        sht40_MeasureMode();
        sht40_Measure();
        
        temp += temperature;
        humi += humidity;
    }
    
    temperature = temp / 10;
    humidity = humi / 10;
}

void ScreenSetClear()
{
    lcd_init();
    LCD_setScreen(BLACK); // clear
    LCD_drawLine(89, 0, 89, 110, WHITE);
    LCD_drawString(94, 2, "Thresholds", WHITE, BLACK);
    LCD_drawString(10, 2, "{ D A T A }", WHITE, BLACK);
    LCD_drawLine(0, 20, 159, 20, WHITE);
    LCD_drawString(2, 31, "Temp_C:", WHITE, BLACK);
    LCD_drawString(94, 31, "T_TH: ", WHITE, BLACK);
    LCD_drawLine(0, 50, 159, 50, WHITE);
    LCD_drawString(2, 61, "RH_%:", WHITE, BLACK);
    LCD_drawString(94, 61, "RH_TH:", WHITE, BLACK);
    LCD_drawLine(0, 80, 159, 80, WHITE);
    LCD_drawString(2, 91, "Time Here", WHITE, BLACK);
    LCD_drawLine(0, 110, 159, 110, WHITE);
}

void ShowDataScreen()
{
    // temperature
    snprintf(str_temperature, STRLEN, "%.1f ", temperature); // safer than sprintf
    LCD_drawString(50, 31, str_temperature, WHITE, BLACK);
    
    // temperature threshold
    snprintf(str_temp_threshold, STRLEN, "%d ", temp_threshold);
    LCD_drawString(131, 31, str_temp_threshold, WHITE, BLACK);
    
    // humidity
    snprintf(str_humidity, STRLEN, "%.1f ", humidity);
    LCD_drawString(38, 61, str_humidity, WHITE, BLACK);
    
    // relative humidity threshold
    snprintf(str_rh_threshold, STRLEN, "%d ", rh_threshold);
    LCD_drawString(137, 61, str_rh_threshold, WHITE, BLACK);
    
    // LOCK
    if (mode == 0)
    {
        LCD_drawString(94, 91, "LOCKED   ", WHITE, BLACK);
    }
    else if (mode == 1)
    {
        LCD_drawString(94, 91, "T UNLOCK ", WHITE, BLACK);
    }
    else if (mode == 2)
    {
        LCD_drawString(94, 91, "RH UNLOCK", WHITE, BLACK);
    }
}

void TransmitData()
{
    char message[101] = "Temp: "; // start of message
    strcat(message, str_temperature);
    strcat(message, "C, T_TH: ");
    strcat(message, str_temp_threshold);
    strcat(message, "C ||| RH: ");
    strcat(message, str_humidity);
    strcat(message, "%, RH_TH: ");
    strcat(message, str_rh_threshold);
    strcat(message, "%\n");
    uart_printString(message);
}

void SetTH_init()
{
    // ADC change threshold values
    // disable power reduction for ADC
    PRR0 &= ~(1<<PRADC);
    
    // Vref = AVcc
    ADMUX |= (1<<REFS0);
    ADMUX &= ~(1<<REFS1);
    
    // ADC prescaler 128. 16MHz/128=125kHz
    ADCSRA |= (1<<ADPS0);
    ADCSRA |= (1<<ADPS1);
    ADCSRA |= (1<<ADPS2);
    
    // use ADC0 PC0
    ADMUX &= ~(1<<MUX0);
    ADMUX &= ~(1<<MUX1);
    ADMUX &= ~(1<<MUX2);
    ADMUX &= ~(1<<MUX3);
    ADCSRA |= (1 << ADATE); // enable auto triggering of ADC
    
    // free running mode
    ADCSRB &= ~(1<<ADTS0);
    ADCSRB &= ~(1<<ADTS1);
    ADCSRB &= ~(1<<ADTS2);
    DIDR0 |= (1<<ADC0D); // disable digital input buffer on ADC pin
    ADCSRA |= (1<<ADEN); // enable ADC
    ADCSRA |= (1<<ADSC); // start ADC
    
    // button change modes
    DDRE &= ~(1<<DDRE2); // PE2 as input
       
    TCCR3B |= (1<<ICES3); // Initialize Input Capture Edge as rising edge
    TIMSK3 |= (1<<ICIE3); // Enable Input Capture Interrupt
    TCCR3B |= (1<<CS30);  // Enable Timer 3. No prescaling.
}

void SetThresholds() // ADC default 505, [005, 1016]
{
    if (mode == 1) // adjust temperature threshold
    {
        if ((ADC < 350) && (temp_threshold > TEMP_MIN))
        {
            temp_threshold -= 1;
        }
        else if ((ADC > 655) && (temp_threshold < TEMP_MAX))
        {
            temp_threshold += 1;
        }
    }
    else if (mode == 2) // adjust relative humidity threshold
    {
        if ((ADC < 350) && (rh_threshold > RH_MIN))
        {
            rh_threshold -= 1;
        }
        else if ((ADC > 655) && (rh_threshold < RH_MAX))
        {
            rh_threshold += 1;
        }
    }
}

void WaterPump_init()
{
    // Green LED indication
    DDRD |= (1<<DDD7); // PD7 as output
    
    // Timer 2 for WaterPump
    DDRD |= (1<<DDD3); // PD3 as output OC2B
    // fast PWM mode TOP = 0xFF
    TCCR2A |= (1<<WGM20);
    TCCR2A |= (1<<WGM21);
    TCCR2B &= ~(1<<WGM22);
    OCR2B = 0; // 0% duty cycle initially
    // clear OC2B at compare match
    TCCR2A &= ~(1<<COM2B0);
    TCCR2A |= (1<<COM2B1);
}

void WaterPump(uint8_t speed)
{
    // Turn ON Green LED
    PORTD |= (1<<PORTD7);
    PORTD |= (1<<PORTD3);
    
    // enable water pump. prescaler 32. 16000000/255/256 approx 245 Hz
    OCR2B = speed; // decided by how far away from the threshold
    TCCR2B &= ~(1<<CS20);
    TCCR2B |= (1<<CS21);
    TCCR2B |= (1<<CS22);
}

void Time_init()
{
    // timer4 for counting seconds, ctc mode, interrupt enabled
    TCCR4A &= ~(1<<WGM40);
    TCCR3A &= ~(1<<WGM41);
    TCCR4B |= (1<<WGM42);
    TCCR4B &= ~(1<<WGM43);
    
    //enable compare A match interrupt
    TIMSK4 |= (1<<OCIE4A);
    
    // for 4 seconds 16000000*4/1024/62500 = 1
    OCR4A = 62500;
    TCCR4B |= (1<<CS40);
    TCCR4B &= ~(1<<CS41);
    TCCR4B |= (1<<CS42);    
}

void Time()
{
    if (minutes == 60)
    {
        minutes = 0;
        hours += 1;
    }
    
    if (hours == 24)
    {
        hours = 0;
        days += 1;
    }
    
    
    snprintf(str_minutes, STRLEN, "%d ", minutes);
    snprintf(str_hours, STRLEN, "%d", hours);
    snprintf(str_days, STRLEN, "%d", days);
    char time[25] = "T: ";
    strcat(time, str_days);
    strcat(time, ":");
    strcat(time, str_hours);
    strcat(time, ":");
    strcat(time, str_minutes);
    LCD_drawString(2, 91, time, WHITE, BLACK);
}