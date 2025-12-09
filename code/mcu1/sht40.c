/* 
 * File:   sht40.c
 * Author: Peiyu
 *
 * Created on November 15, 2025, 12:07 AM
 */

/* 
 * I2C and SHT40 notes:
 * 1. Not support clock stretching.
 * 2. Measured data can only be read once, and will be deleted after reading.
 * 3. All transfer begins with START and ends with STOP.
 * 4. Temperature and Relative Humidity Data: 2 data bytes + 1 CRC8 byte for checking
 * 5. Temperature first, then, Relative Humidity.
 * 6. Sequence:
 *    START -> Sensor Address + WRITE -> STOP
 *    -> START -> Sensor Address + READ -> Read 6 bytes (last one with NACK to the sensor)
 *    -> STOP -> Convert data to floats.
 * 7. I2C speed:
 *    Adafruit SHT40 Sensor has 10k Ohms pull-up resistors for SDA and SCL
 *    However, too big for 400kHz speed (actual speed about 341kHz for 400kHz configuration).
 *    Added additional 10k Ohms pull-up resistors in parallel for SDA and SCL.
 *    Now, SDA and SCL has about 5k Ohms pull-up resistance.
 *    I2C speed improved to 389kHz for 400kHz configuration.
 * 8. TWSTA needs to be cleared by program; TWSTO is cleared automatically.
 * 9. !!!Temperature/Relative Humidity: Integer.Float - Receive the float part first, then the integer part.!!!
 */

#include <xc.h>
#include <stdio.h>
#include <stdlib.h>
#include "sht40.h"

// 400kHz speed
// From datasheet, 16,000,000 / (16 + 2xTWBR(=12)xPrescalerValue(=1)) = 400,000
// configure I2C0 for SHT40
void i2c0_init()
{
    // no action now
    TWCR0 &= ~(1<<TWINT); // make sure TWINT set
    // 16,000,000 / (16 + 2x12x1) = 400,000
    TWBR0 = 12;
    // prescaler = 1, easier for status code, don't have to mask it
    TWSR0 &= ~(1<<TWPS0);
    TWSR0 &= ~(1<<TWPS1);
    // disable I2C0 interrupt, use polling since not measuring frequently
    TWCR0 &= ~(1<<TWIE);
    // enable I2C0
    TWCR0 |= (1<<TWEN);
}

// START condition
void i2c0_start()
{
//    while(!(TWCR0 & (1<<TWINT))); // wait until the previous event finishes
    TWCR0 |= (1<<TWSTA); // START
    TWCR0 |= (1<<TWINT); // clear the flag. begin action
}

// STOP condition
void i2c0_stop()
{
    while(!(TWCR0 & (1<<TWINT))); // wait until the previous event finishes
    // NO need to check status code
    TWCR0 |= (1<<TWSTO); // STOP
    TWCR0 |= (1<<TWINT); // clear the flag. begin action
}

// enter Master transmitter mode
void sht40_writeTo()
{
    while(!(TWCR0&(1<<TWINT))); // wait until the previous event finishes
    if(TWSR0&0x08) // START success
    {
        TWDR0 = (0x44<<1); // 10001000, 0x44+0(WRITE)
        TWCR0 &= ~(1<<TWSTA); // already START, manually clear
        TWCR0 |= (1<<TWINT); // clear the flag. begin action
    }
}

// enter Master receiver mode
void sht40_readFrom()
{
    while(!(TWCR0&(1<<TWINT))); // wait until the previous event finishes
    if((TWSR0&0x08)||(TWSR0&0x10))
    {
        TWDR0 = (0x44<<1) + 1; // 10001001, 0x44+1(READ)
        TWCR0 &= ~(1<<TWSTA); // already START, manually clear
        TWCR0 |= (1<<TWEA); // able to generate ACK in Master Receiver mode
        TWCR0 |= (1<<TWINT); // clear the flag. begin action
    }
}

// load one command, SHT40 datasheet Table 8
void sht40_sendCMD(uint16_t cmd)
{
    while(!(TWCR0&(1<<TWINT))); // wait until the previous event finishes
    if(TWSR0&0x18) // writeTo success + ACK received
    {
        TWDR0 = cmd; // load command
        TWCR0 |= (1<<TWINT); // clear the flag. begin action
    }
}

// read 1 data byte from SHT40
uint16_t sht40_readByte()
{
    float Byte = 0;
    while(!(TWCR0&(1<<TWINT))); // wait until the previous event finishes
    if(TWSR0&0x40) // readFrom success + ACK sent
    {
        Byte = TWDR0;
        TWCR0 |= (1<<TWINT); // clear the flag. begin action
    }
    
    return Byte;
}