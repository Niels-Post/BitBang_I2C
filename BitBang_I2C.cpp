//
// Bit Bang I2C library
// Copyright (c) 2018 BitBank Software, Inc.
// Written by Larry Bank (bitbank@pobox.com)
// Project started 10/12/2018
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include <Arduino.h>
#include <BitBang_I2C.h>

static int iSCL, iSDA; // keep requested pin numbers in private statics
static int iDelay; // bit delay in ms for the requested clock rate
#ifdef __AVR__
volatile uint8_t *iDDR_SCL, *iPort_SCL_Out;
volatile uint8_t *iDDR_SDA, *iPort_SDA_In, *iPort_SDA_Out;
uint8_t iSDABit, iSCLBit;
#endif

#ifdef __AVR__
uint8_t getPinInfo(uint8_t pin, volatile uint8_t **iDDR, volatile uint8_t **iPort, int bInput)
{
  uint8_t port, bit;

  port = (pin & 0xf0); // hex port (A,B,D,E,F)
  bit = pin & 0x7;
  switch (port)
  {
#ifdef PORTE
    case 0xE0:
      *iPort = (bInput) ? &PINE : &PORTE;
      *iDDR = &DDRE;
      break;
#endif
#ifdef PORTF
    case 0xF0:
      *iPort = (bInput) ? &PINF : &PORTF;
      *iDDR = &DDRF;
      break;
#endif
#ifdef PORTG
    case 0xA0: // really port G
      *iPort = (bInput) ? &PING : &PORTG;
      *iDDR = &DDRG;
      break;
#endif
#ifdef PORTC
    case 0xC0:
      *iPort = (bInput) ? &PINC : &PORTC;
      *iDDR = &DDRC;
      break;
#endif
#ifdef PORTB
    case 0xB0:
      *iPort = (bInput) ? &PINB : &PORTB;
      *iDDR = &DDRB;
      break;
#endif
#ifdef PORTD
    case 0xD0:
      *iPort = (bInput) ? &PIND : &PORTD;
      *iDDR = &DDRD;
      break;
#endif
  }
  return bit;
} /* getPinInfo() */
#endif // __AVR__

inline uint8_t SDA_READ(void)
{
#ifdef __AVR__
  if (iSDA >= 0xa0) // direct pin numbering
  {
    if (*iPort_SDA_In & iSDABit)
       return HIGH;
    else
       return LOW;
  }
  else
#endif
  {
    return digitalRead(iSDA);
  }
}
inline void SCL_HIGH(void)
{
#ifdef __AVR__
  if (iSCL >= 0xa0) // direct pin numbering
  {
    *iDDR_SCL &= ~iSCLBit;
  }
  else
#endif
  {
    pinMode(iSCL, INPUT);
  }
}

inline void SCL_LOW(void)
{
#ifdef __AVR__
  if (iSCL >= 0xa0) // direct pin numbering
  {
    *iDDR_SCL |= iSCLBit;
  }
  else
#endif
  {
    pinMode(iSCL, OUTPUT);
  }
}

inline void SDA_HIGH(void)
{
#ifdef __AVR__
  if (iSDA >= 0xa0) // direct pin numbering
  {
    *iDDR_SDA &= ~iSDABit;
  }
  else
#endif
  {
    pinMode(iSDA, INPUT);
  }
}

inline void SDA_LOW(void)
{
#ifdef __AVR__
  if (iSDA >= 0xa0) // direct pin numbering
  {
    *iDDR_SDA |= iSDABit;
  }
  else
#endif
  {
    pinMode(iSDA, OUTPUT);
  }
}

inline void sleep_us(int iDelay)
{
  delayMicroseconds(iDelay);
}
// Transmit a byte and read the ack bit
// if we get a NACK (negative acknowledge) return 0
// otherwise return 1 for success
//
static inline int i2cByteOut(uint8_t b)
{
uint8_t i, ack;

     for (i=0; i<8; i++)
     {
         if (b & 0x80)
           SDA_HIGH(); // set data line to 1
         else
           SDA_LOW(); // set data line to 0
         SCL_HIGH(); // clock high (slave latches data)
         sleep_us(iDelay);
         SCL_LOW(); // clock low
         b <<= 1;
     } // for i
// read ack bit
  SDA_HIGH(); // set data line for reading
  SCL_HIGH(); // clock line high
  sleep_us(iDelay); // DEBUG - delay/2
  ack = SDA_READ();
  SCL_LOW(); // clock low
  sleep_us(iDelay); // DEBUG - delay/2
  SDA_LOW(); // data low
  return (ack == 0) ? 1:0; // a low ACK bit means success
} /* i2cByteOut() */

static inline int i2cByteOutFast(uint8_t b)
{
uint8_t i, ack;

     if (b & 0x80)
        SDA_HIGH(); // set data line to 1
     else
        SDA_LOW(); // set data line to 0
     for (i=0; i<8; i++)
     {
         SCL_HIGH(); // clock high (slave latches data)
         sleep_us(iDelay);
         SCL_LOW(); // clock low
     } // for i
// read ack bit
  SDA_HIGH(); // set data line for reading
  SCL_HIGH(); // clock line high
  sleep_us(iDelay); // DEBUG - delay/2
  ack = SDA_READ();
  SCL_LOW(); // clock low
  sleep_us(iDelay); // DEBUG - delay/2
  SDA_LOW(); // data low
  return (ack == 0) ? 1:0; // a low ACK bit means success
} /* i2cByteOutFast() */

//
// Receive a byte and read the ack bit
// if we get a NACK (negative acknowledge) return 0
// otherwise return 1 for success
//
static inline uint8_t i2cByteIn(uint8_t bLast)
{
uint8_t i;
uint8_t b = 0;

     SDA_HIGH(); // set data line as input
     for (i=0; i<8; i++)
     {
         sleep_us(iDelay); // wait for data to settle
         SCL_HIGH(); // clock high (slave latches data)
         b <<= 1;
         if (SDA_READ() != 0) // read the data bit
           b |= 1; // set data bit
         SCL_LOW(); // cloc low
     } // for i
     if (bLast)
        SDA_HIGH(); // last byte sends a NACK
     else
        SDA_LOW();
     SCL_HIGH(); // clock high
     sleep_us(iDelay);
     SCL_LOW(); // clock low to send ack
     sleep_us(iDelay);
     SDA_LOW(); // data low
  return b;
} /* i2cByteIn() */

//
// Send I2C STOP condition
//
static inline void i2cEnd()
{
   SDA_LOW(); // data line low
   sleep_us(iDelay);
   SCL_HIGH(); // clock high
   sleep_us(iDelay);
   SDA_HIGH(); // data high
   sleep_us(iDelay);
} /* i2cEnd() */


static inline int i2cBegin(uint8_t addr, uint8_t bRead)
{
   int rc;
   SDA_LOW(); // data line low first
   sleep_us(iDelay);
   SCL_LOW(); // then clock line low is a START signal
   addr <<= 1;
   if (bRead)
      addr++; // set read bit
   rc = i2cByteOut(addr); // send the slave address and R/W bit
   return rc;
} /* i2cBegin() */

static inline int i2cWrite(uint8_t *pData, int iLen)
{
uint8_t i, b;
int rc, iOldLen = iLen;

   rc = 1;
   while (iLen && rc == 1)
   {
      b = *pData++;
      if (b == 0xff || b == 0)
         rc = i2cByteOutFast(b); // speed it up a bit more if all bits are ==
      else
         rc = i2cByteOut(b);
      if (rc == 1) // success
      {
         iLen--;
      }
   } // for each byte
   return (rc == 1) ? (iOldLen - iLen) : 0; // 0 indicates bad ack from sending a byte
} /* i2cWrite() */

static inline void i2cRead(uint8_t *pData, int iLen)
{
uint8_t i, b;
int iOldLen = iLen;

   while (iLen--)
   {
      *pData++ = i2cByteIn(iLen == 0);
   } // for each byte
} /* i2cRead() */
//
// Initialize the I2C BitBang library
// Pass the pin numbers used for SDA and SCL
// as well as the clock rate in Hz
//
void I2CInit(int iSDA_Pin, int iSCL_Pin, int32_t iClock)
{
   iSDA = iSDA_Pin;
   iSCL = iSCL_Pin;
   if (iSDA < 0xa0)
   {
     pinMode(iSDA, INPUT); // let the lines float (tri-state)
     pinMode(iSCL, INPUT);
     digitalWrite(iSDA, LOW); // setting low = enabling as outputs
     digitalWrite(iSCL, LOW);
   }
#ifdef __AVR__
   else // direct pin mode, get port address and bit
   {
      iSDABit = 1 << (iSDA & 0x7);
      getPinInfo(iSDA, &iDDR_SDA, &iPort_SDA_Out, 0);
      getPinInfo(iSDA, &iDDR_SDA, &iPort_SDA_In, 1);
      iSCLBit = 1 << (iSCL & 0x7);
      getPinInfo(iSDA, &iDDR_SCL, &iPort_SCL_Out, 0);
      *iDDR_SDA &= ~iSDABit; // pinMode input
      *iDDR_SCL &= ~iSCLBit; // pinMode input
      *iPort_SDA_Out &= ~iSDABit; // digitalWrite SDA LOW
      *iPort_SCL_Out &= ~iSCLBit; // digitalWrite SCL LOW
   }
#endif // __AVR__
  // For now, we only support 100, 400 or 800K clock rates
  // all other values default to 100K
   if (iClock >= 800000)
      iDelay = 1;
   else if (iClock >= 400000)
      iDelay = 2;
   else if (iClock >= 100000)
      iDelay = 10;
   else iDelay = 1000000 / iClock;
} /* i2cInit() */
//
// Scans for I2C devices on the bus
// returns a bitmap of devices which are present (128 bits = 16 bytes, LSB first)
// A set bit indicates that a device responded at that address
//
void I2CScan(uint8_t *pMap)
{
  int i;
  memset(pMap, 0, 16);
  for (i=0; i<128; i++) // try every address
  {
    if (i2cBegin(i, 0)) // try to write to every device
    {
      pMap[i >> 3] |= (1 << (i & 7));
    }
    i2cEnd();
  }
} /* I2CScan() */
//
// Write I2C data
// quits if a NACK is received and returns 0
// otherwise returns the number of bytes written
//
int I2CWrite(uint8_t iAddr, uint8_t *pData, int iLen)
{
  int rc;
  
  rc = i2cBegin(iAddr, 0);
  if (rc == 1) // slave sent ACK for its address
  {
     rc = i2cWrite(pData, iLen);
  }
  i2cEnd();
  return rc; // returns the number of bytes sent or 0 for error
} /* I2CWrite() */
//
// Read N bytes starting at a specific I2C internal register
//
int I2CReadRegister(uint8_t iAddr, uint8_t u8Register, uint8_t *pData, int iLen)
{
  int rc;
  
  rc = i2cBegin(iAddr, 0); // start a write operation
  if (rc == 1) // slave sent ACK for its address
  {
     rc = i2cWrite(&u8Register, 1); // write the register we want to read from
     if (rc == 1)
     {
       i2cEnd();
       rc = i2cBegin(iAddr, 1); // start a read operation
       if (rc == 1)
       {
         i2cRead(pData, iLen);
       }
     }
  }
  i2cEnd();
  return rc; // returns the number of bytes received or 0 for error
} /* I2CReadRegister() */
//
// Read N bytes
//
int I2CRead(uint8_t iAddr, uint8_t *pData, int iLen)
{
  int rc;
  
  rc = i2cBegin(iAddr, 1);
  if (rc == 1) // slave sent ACK for its address
  {
     i2cRead(pData, iLen);
  }
  i2cEnd();
  return rc; // returns 1 for success, 0 for error
} /* I2CRead() */
