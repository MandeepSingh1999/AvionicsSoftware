/**
  ******************************************************************************
  * File Name          : ReadBarometer.h
  ******************************************************************************
*/

#pragma once

/* Includes ------------------------------------------------------------------*/

/* Macros --------------------------------------------------------------------*/

/* Externs -------------------------------------------------------------------*/
extern SPI_HandleTypeDef hspi2;

/* Structs -------------------------------------------------------------------*/

/* Prototypes ----------------------------------------------------------------*/
void readBarometerTask(void const* arg);
