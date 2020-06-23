/***********************************************************************
 * \file RD117piA.cpp    (main)                                        *
 *                                                                     *
 *                                                                     *
 * Project:     RD117piA                                               *
 * Filename:    RD117piA.cpp                                           *
 * References:    Code adapted/stolen from:                            *
 *     Maxim:                                                          *
 *         https://github.com/MaximIntegratedRefDesTeam/RD117_ARDUINO  *
 *     Robert Fraczkiewicz                                             *
 *         https://github.com/aromring/MAX30102_by_RF/blob/master/ )   *
 *     pigpio c library ADXL345 example code at:                       *
 *         http://abyz.me.uk/rpi/pigpio/code/adxl345_c.zip             *
 *                                                                     *
 * Description: Raspberry Pi main code file for MAX30102 proto.        *
 *                                                                     *
 * Revision History:                                                   *
 *   01-18-2016 Rev 01.00 GL Initial release.                          *
 *   12-22-2017 Rev 02.00 Greatly mod'd by Robert Fraczkiewicz (RF)    *
 *                          to use Wire lib vs. MAXIM's SoftI2C lib.   *
 *   06/18/2020 Rev 03.00 Ported to Raspberry Pi using "pigpio" lib.   *
 *                          This version includes algorithm*           *
 ***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <linux/i2c-dev.h>
#include <thread>
#include <chrono>
#include <pigpio.h>

#include "max30102pi.h"
#include "max30102pi.cpp"
#include "algorithm2.h"
#include "algorithm2.cpp"
#include "algorithm2_by_RF.h"
#include "algorithm2_by_RF.cpp"

#define DUMPREGSSIZE 20
#define ITER 10

unsigned char byte1;
unsigned char byte2;
char buf[16];
int i2cHndl;
int regDumpArray[DUMPREGSSIZE];

FILE *ofpR;
FILE *ofpL;

uint32_t elapsedTime,timeStart;

uint32_t aun_ir_buffer[BUFFER_SIZE];   // infrared LED sensor data
uint32_t aun_red_buffer[BUFFER_SIZE];  // red LED sensor data
int tckVal_buffer[BUFFER_SIZE];        // uSec time intervals
double old_n_spo2;                     // Previous SPO2 value
unsigned char uch_dummy;

double n_spo2,ratio,correl;  //SPO2 value
int8_t ch_spo2_valid;        //indicator to show if the SPO2 calculation is valid
int32_t n_heart_rate;        //heart rate value
int8_t  ch_hr_valid;         //indicator to show if the heart rate calculation is valid
int32_t i;
char hr_str[10];

double n_spo2_maxim;         //SPO2 value
int8_t ch_spo2_valid_maxim;  //indicator to show if the SPO2 calculation is valid
int32_t n_heart_rate_maxim;  //heart rate value
int8_t  ch_hr_valid_maxim;   //indicator to show if the heart rate calculation is valid

int setup(int* i2cDevHndl)
{
  int hndl;
  
  gpioInitialise();

  gpioSetMode(INTPIN, PI_INPUT);         // RaspPi pin 23 connects to the
                                         // MAX30102 interrupt output pin
  gpioSetPullUpDown(INTPIN, PI_PUD_UP);  // Sets a pull-up (also 6.8k ext. to 3.3V).

  // Open port (i2c) for reading and writing
  hndl = max30102pi_initDevice();
  if (hndl == -1)
  {
    exit(1);
  }
  *i2cDevHndl = hndl;

  ofpR = fopen("oxiRegs.dat", "w");
  ofpL = fopen("oxiData.csv", "w");

  i2cReadByteData(i2cHndl, REG_INTR_STATUS_1);  // clear ISR1.

  return 0;
}  // End of int setup(int* i2cDevHndl)


int writeReadingsToLog(FILE *ofpL)
{
  for(int i = 0; i < BUFFER_SIZE; i++)
  {
    fprintf(ofpL, "%d, %i, %i, %d\n", i, aun_red_buffer[i], aun_ir_buffer[i], tckVal_buffer[i]);
  }

  return 0;
}  // End of int writeReadingsToLog(int ofpL)

int writeRegsToLog(FILE *ofpR)
{
  fprintf(ofpR, "----Begin MAX3010x Regs------------\n");
  for(int i = 0; i < DUMPREGSSIZE; i++)
  {
    fprintf(ofpR, "%02X\n", regDumpArray[i]);
  }
  fprintf(ofpR, "----End   MAX3010x Regs------------\n");
  return 0;
}  // End of int writeRegsToLog(int ofpR)


int main (int argc, char **argv)
{
  // int j = 0;
  setup(&i2cHndl);
  max30102pi_dumpRegs(i2cHndl, regDumpArray);
  writeRegsToLog(ofpR);
  
  //buffer length of BUFFER_SIZE stores ST seconds of samples running at FS sps
  //read BUFFER_SIZE samples, and determine the signal range
  uint32_t lastTick, nextTick;
  int diffTick;
  lastTick = gpioTick();

  for(int k = 0; k < ITER; k++)
  {
    printf("==================================================\n");
    for(int i = 0; i < BUFFER_SIZE; i++)
    {
      while(gpioRead(INTPIN)==1);
      i2cReadByteData(i2cHndl, REG_INTR_STATUS_1);                           // Clear ISR1.
      max30102pi_read_fifo(i2cHndl, (aun_red_buffer+i), (aun_ir_buffer+i));  // Read from MAX30102 FIFO.
      nextTick = gpioTick();
      diffTick = nextTick - lastTick;
      tckVal_buffer[i] = diffTick;
      lastTick = nextTick;
      // printf("readFifo: %d\n", j++);
      // printf("%X, %X\n", (aun_red_buffer+i), (aun_ir_buffer+i));
    }
  

    rf_heart_rate_and_oxygen_saturation(aun_ir_buffer, BUFFER_SIZE, aun_red_buffer,
                                        &n_spo2, &ch_spo2_valid, &n_heart_rate,
                                        &ch_hr_valid, &ratio, &correl); 

    printf("--RF--\n");
    printf("SpO2: %lf", n_spo2);
    printf("\t");
    printf("HR:   %d", n_heart_rate);
    printf("\t");
    printf("\n------\n");

    maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, BUFFER_SIZE, aun_red_buffer,
                                           &n_spo2_maxim, &ch_spo2_valid_maxim,
					   &n_heart_rate_maxim, &ch_hr_valid_maxim); 
    printf("--MX--\n");
    printf("%lf", n_spo2_maxim);
    printf("\t");
    printf("%d\n", n_heart_rate_maxim);
    printf("------\n");

    printf("\n");
  }

  writeReadingsToLog(ofpL);
  max30102pi_dumpRegs(i2cHndl, regDumpArray);
  writeRegsToLog(ofpR);
  fclose(ofpR);
  fclose(ofpL);
  i2cClose(i2cHndl);
  gpioTerminate();

  return 0;
}  // end of int main (int argc, char **argv)

