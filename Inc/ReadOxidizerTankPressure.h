/**
  ******************************************************************************
  * File Name          : ReadOxidizerTankPressure.h
  ******************************************************************************
*/

#pragma once

/* Includes ------------------------------------------------------------------*/

/* Macros --------------------------------------------------------------------*/

/* Externs -------------------------------------------------------------------*/
extern ADC_HandleTypeDef hadc2;

/* Structs -------------------------------------------------------------------*/

/* Prototypes ----------------------------------------------------------------*/
void readOxidizerTankPressureTask(void const* arg);
