/**
  ******************************************************************************
  * File Name          : MonitorForEmergencyShutoff.h
  ******************************************************************************
*/

#pragma once

/* Includes ------------------------------------------------------------------*/

/* Macros --------------------------------------------------------------------*/

/* Externs -------------------------------------------------------------------*/
extern uint8_t abortCmdReceived;

/* Structs -------------------------------------------------------------------*/

/* Prototypes ----------------------------------------------------------------*/
void monitorForEmergencyShutoffTask(void const* arg);