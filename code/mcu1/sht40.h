/* 
 * File:   sht40.h
 * Author: Peiyu
 *
 * Created on November 15, 2025, 12:07 AM
 */

#ifndef SHT40_H
#define	SHT40_H

void i2c0_init();
void i2c0_error();
void i2c0_start();
void i2c0_stop();
void sht40_writeTo();
void sht40_readFrom();
void sht40_sendCMD(uint16_t cmd);
uint16_t sht40_readByte();

#endif	/* SHT40_H */

