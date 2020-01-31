/**
  ******************************************************************************
  * File Name          : ParachutesControl.c
  * Description        : This file contains constants and functions designed to
  *                      detect the apogee of the rocket's tragectory, and
  *                      release the appropriate parachutes (drouge and main)
  *                      at the right time to prevent the rocket from becoming
  *                      a ballistic missile.
  ******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/

#include <math.h>

#include "stm32f4xx.h"
#include "stm32f4xx_hal_conf.h"
#include "cmsis_os.h"

#include "ParachutesControl.h"
#include "FlightPhase.h"
#include "Data.h"
#include "AltitudeKalmanFilter.h"

/* Macros --------------------------------------------------------------------*/

/* Constants -----------------------------------------------------------------*/

#define SPACE_PORT_AMERICA_ALTITUDE_ABOVE_SEA_LEVEL (1401) // metres

// Pressure at spaceport america in 100*millibars on May 27, 2018
static const int MAIN_DEPLOYMENT_ALTITUDE = 457 + 1401; // Units in meters. Equivalent of 15000 ft + altitude of spaceport america.
static const int MONITOR_FOR_PARACHUTES_PERIOD = 200;
static const int NUM_DESCENTS_TO_TRIGGER_DROUGE = 3;
static const int KALMAN_FILTER_MAIN_TIMEOUT = 10 * 60 * 1000; // 10 minutes

/*
***These are the old constants before the KalmanFilter file breakout***
//TODO remove after everything works

// Units in meters. Equivalent of 1500 ft + altitude of spaceport america.
static const int MAIN_DEPLOYMENT_ALTITUDE = 457 + SPACE_PORT_AMERICA_ALTITUDE_ABOVE_SEA_LEVEL;

static const int MONITOR_FOR_PARACHUTES_PERIOD = 50;
static const int KALMAN_FILTER_DROGUE_TIMEOUT = 2 * 60 * 1000; // 2 minutes

static const int PARACHUTE_PULSE_DURATION = 2 * 1000; // 2 seconds
*/


/* Variables -----------------------------------------------------------------*/

static int numDescents = 0;
static struct KalmanStateVector kalmanFilterState;

/* Structs -------------------------------------------------------------------*/

/* Prototypes ----------------------------------------------------------------*/

/* Functions -----------------------------------------------------------------*/

/**
 * Takes the current state vector and determines if apogee has been reached.
 *
 * Params:
 *   state - (KalmanStateVector) Past altitude, velocity and acceleration
 *
 * Returns:
 *   - (int32_t) 1 if apogee has been reached, 0 if not.
 */
int32_t detectApogee()
{
    // Monitor for when to deploy drogue chute. Looking for a certain number of descents
    if (numDescents >= NUM_DESCENTS_TO_TRIGGER_DROUGE)
    {
        return 1;
    }

    return 0;
}

/**
 * Takes the current state vector and determines if main chute should be released.
 *
 * ***NOTE: This is determined by the constant MAIN_DEPLOYMENT_ALTITUDE,
 *           which should be verified before launch.
 *
 * Params:
 *   state - (KalmanStateVector) Past altitude, velocity and acceleration
 *
 * Returns:
 *   - (int32_t) 1 if main chute should be deployed, 0 if not.
 */
int32_t detectMainDeploymentAltitude(struct KalmanStateVector state)
{
    // Monitor for when to deploy main chute. Simply look for less than desired altitude.
    if (state.altitude < MAIN_DEPLOYMENT_ALTITUDE)
    {
        return 1;
    }

    return 0;
}

/**
 * Releases the drouge parachute on the rocket.
 */
void ejectDrogueParachute()
{
    HAL_GPIO_WritePin(DROGUE_PARACHUTE_TEMP_GPIO_Port, DROGUE_PARACHUTE_TEMP_Pin, GPIO_PIN_SET);  // high signal causes high current to ignite e-match
}

void closeDrogueParachute()
{
    HAL_GPIO_WritePin(DROGUE_PARACHUTE_TEMP_GPIO_Port, DROGUE_PARACHUTE_TEMP_Pin, GPIO_PIN_RESET);
}

/**
 * Releases the main parachute on the rocket.
 */
void ejectMainParachute()
{
    HAL_GPIO_WritePin(MAIN_PARACHUTE_GPIO_Port, MAIN_PARACHUTE_Pin, GPIO_PIN_SET);  // high signal causes high current to ignite e-match
}

void closeMainParachute()
{
    HAL_GPIO_WritePin(MAIN_PARACHUTE_GPIO_Port, MAIN_PARACHUTE_Pin, GPIO_PIN_RESET);
}


/**
 * Waits for the current flight phase to get out of PRELAUNCH
 */
void parachutesControlPrelaunchRoutine()
{
    uint32_t prevWakeTime = osKernelSysTick();

    for (;;)
    {
        osDelayUntil(&prevWakeTime, MONITOR_FOR_PARACHUTES_PERIOD);

        if (getCurrentFlightPhase() != PRELAUNCH)
        {
            // Ascent has begun
            return;
        }
    }
}

/**
 * Desc
 *
 * Params:
 *   accelGyromMagnetismData	- (AccelGyroMagnetismData*) Pointer to the IMU data struct to read the acceleration from.
 *   barometerData				- (barometerData*) Pointer to the barometer data struct to read the pressure from.
 *   state						- (KalmanStateVector) The current acceleration, pressure, and altitude state of the rocket.
 */
void parachutesControlBurnRoutine(
    AccelGyroMagnetismData* accelGyroMagnetismData,
    BarometerData* barometerData,
    struct KalmanStateVector state
)
{
    uint32_t prevWakeTime = osKernelSysTick();

    for (;;)
    {
        osDelayUntil(&prevWakeTime, MONITOR_FOR_PARACHUTES_PERIOD);

        if (getCurrentFlightPhase() != BURN)
        {
            return;
        }

        int32_t currentAccel = readAccel(accelGyroMagnetismData);
        int32_t currentPressure = readPressure(barometerData);

        if (currentAccel == -1 || currentPressure == -1)
        {
            // failed to read values
            continue;
        }

        state = filterSensors(state, currentAccel, currentPressure, MONITOR_FOR_PARACHUTES_PERIOD);
    }
}


/**
 * Monitors for apogee. Once apogee has been detected,
 * eject the drogue parachute and update the current flight phase.
 *
 * Params:
 *   accelGyromMagnetismData	- (AccelGyroMagnetismData*) Pointer to the IMU data struct to read the acceleration from.
 *   barometerData				- (barometerData*) Pointer to the barometer data struct to read the pressure from.
 *   state						- (KalmanStateVector) The current acceleration, pressure, and altitude state of the rocket.
 */
void parachutesControlCoastRoutine(
    AccelGyroMagnetismData* accelGyroMagnetismData,
    BarometerData* barometerData,
    struct KalmanStateVector state
)
{
    uint32_t prevWakeTime = osKernelSysTick();
    uint32_t elapsedTime = 0;

    for (;;)
    {
        osDelayUntil(&prevWakeTime, MONITOR_FOR_PARACHUTES_PERIOD);

        elapsedTime += MONITOR_FOR_PARACHUTES_PERIOD;

        int32_t currentAccel = readAccel(accelGyroMagnetismData);
        int32_t currentPressure = readPressure(barometerData);

        if (currentAccel == -1 || currentPressure == -1)
        {
            // failed to read values
            continue;
        }

        double oldAltitude = state.altitude;

        state = filterSensors(state, currentAccel, currentPressure, MONITOR_FOR_PARACHUTES_PERIOD);

        if (state.altitude < oldAltitude)
        {
            numDescents++;
        }
        else
        {
            numDescents = 0;
        }

        if (detectApogee())
        {
            ejectDrogueParachute();
            newFlightPhase(DROGUE_DESCENT);
            return;
        }
    }
}

/**
 * This routine detects reaching a certain altitude.
 * Once that altitude has been reached, eject the main parachute
 * and update the current flight phase.
 *
 * Params:
 *   accelGyromMagnetismData	- (AccelGyroMagnetismData*) Pointer to the IMU data struct to read the acceleration from.
 *   barometerData				- (barometerData*) Pointer to the barometer data struct to read the pressure from.
 *   state						- (KalmanStateVector) The current acceleration, pressure, and altitude state of the rocket.
 */
void parachutesControlDrogueDescentRoutine(
    AccelGyroMagnetismData* accelGyroMagnetismData,
    BarometerData* barometerData,
    struct KalmanStateVector state
)
{
    uint32_t prevWakeTime = osKernelSysTick();
    uint32_t elapsedTime = 0;

    for (;;)
    {
        osDelayUntil(&prevWakeTime, MONITOR_FOR_PARACHUTES_PERIOD);

        elapsedTime += MONITOR_FOR_PARACHUTES_PERIOD;

        int32_t currentAccel = readAccel(accelGyroMagnetismData);
        int32_t currentPressure = readPressure(barometerData);

        if (currentAccel == -1 || currentPressure == -1)
        {
            // failed to read values
            continue;
        }

        state = filterSensors(state, currentAccel, currentPressure, MONITOR_FOR_PARACHUTES_PERIOD);

        // detect 4600 ft above sea level and eject main parachute
        if (detectMainDeploymentAltitude(kalmanFilterState) || elapsedTime > KALMAN_FILTER_MAIN_TIMEOUT)
        {
            ejectMainParachute();
            newFlightPhase(MAIN_DESCENT);
            return;
        }
    }
}

/**
 * This function is to be used as a thread task that reads and updates
 * a kalman state vector, which it uses to detect the apogee of the rocket's
 * flight path. Once apogee is detected, a drouge parachute will be released.
 * Some time after this, the main parachute will also be deployed.
 *
 * Params:
 *   arg - (void const*) A pointer to the ParachutesControlData struct that will be used.
 */
void parachutesControlTask(void const* arg)
{
    ParachutesControlData* data = (ParachutesControlData*) arg;
    kalmanFilterState.altitude = SPACE_PORT_AMERICA_ALTITUDE_ABOVE_SEA_LEVEL;
    kalmanFilterState.velocity = 0;
    kalmanFilterState.acceleration = 0;

    for (;;)
    {
        switch (getCurrentFlightPhase())
        {
            case PRELAUNCH:
            case ARM:
                parachutesControlPrelaunchRoutine();
                break;

            case BURN:
                parachutesControlBurnRoutine(
                    data->accelGyroMagnetismData_,
                    data->barometerData_
                );
                break;

            case COAST:
                parachutesControlCoastRoutine(
                    data->accelGyroMagnetismData_,
                    data->barometerData_
                );
                break;

            case DROGUE_DESCENT:
                parachutesControlDrogueDescentRoutine(
                    data->accelGyroMagnetismData_,
                    data->barometerData_
                );

                break;

            case MAIN_DESCENT:
                parachutesControlMainDescentRoutine();
                break;

            case ABORT_COMMAND_RECEIVED:
            case ABORT_OXIDIZER_PRESSURE:
            case ABORT_UNSPECIFIED_REASON:
            case ABORT_COMMUNICATION_ERROR:
                // do nothing
                osDelay(MONITOR_FOR_PARACHUTES_PERIOD);
                break;

            default:
                break;
        }
    }
}
