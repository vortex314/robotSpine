/*
 * Crc32.cpp
 *
 *  Created on: Nov 11, 2021
 *      Author: lieven
 */



#include <sys/types.h>
#include <stdint.h>
#include "Crc32.h"

uint32_t crc32(uint32_t Crc, uint32_t Data)
{
  int i;

  Crc = Crc ^ Data;

  for(i=0; i<32; i++)
    if (Crc & 0x80000000)
      Crc = (Crc << 1) ^ 0x04C11DB7; // Polynomial used in STM32
    else
      Crc = (Crc << 1);

  return(Crc);
}

uint32_t Crc32::bufferCrc(uint32_t *data,size_t size){
	uint32_t crc = 0xFFFFFFFF;
	for( uint32_t i=0;i<size;i++) crc=crc32(crc,data[i]);
	return crc;
}
