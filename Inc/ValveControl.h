/**
  ******************************************************************************
  * File Name          : ValveControl.h
  ******************************************************************************
*/

#pragma once

/* Includes ------------------------------------------------------------------*/

/* Macros --------------------------------------------------------------------*/
#define MAX_DURATION_VENT_VALVE_OPEN (8000)        // Required in AbortPhase.c
#define REQUIRED_DURATION_VENT_VALVE_CLOSED (4000) // Required in AbortPhase.c

/* Externs -------------------------------------------------------------------*/

/* Structs -------------------------------------------------------------------*/

/* Prototypes ----------------------------------------------------------------*/
void openVentValve();
void closeVentValve();
void openInjectionValve();
void closeInjectionValve();