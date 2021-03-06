// ======================================================================
// focuserconfig.h : myFP2ESP FOCUSER CONFIG DEFINITIONS
// SPECIFY HARDWARE OPTIONS AND CONTROLLER MODES HERE
// (c) Copyright Robert Brown 2014-2021. All Rights Reserved.
// (c) Copyright Holger M, 2019-2021. All Rights Reserved.
// ======================================================================

#include <Arduino.h>
#include "boarddefs.h"
#include "generalDefinitions.h"

#ifndef focuserconfig_h
#define focuserconfig_h

// Caution: Do not enable a feature if the associated hardware circuits are 
// not fitted on the board. Enable or disable the specific options below

// ======================================================================
// 1: BOARD DEFINES [do not change names]
// ======================================================================
// Uncomment only your board - ONLY ONE BOARD SHOULD BE UNCOMMENTED

// ESP8266 Boards
//#define DRVBRD  PRO2EULN2003
// ESP32 Boards
//#define DRVBRD 	PRO2ESP32DRV8825
#define DRVBRD 	PRO2ESP32ULN2003

// ======================================================================
// 2: SPECIFY FIXEDSTEPMODE
// ======================================================================
// For ESP8266 boards, set the fixedmode value to match the stepmode jumper
// settings on the board [only one line can be enabled]
// Applies to boards WEMOSDRV8825H, WEMOSDRV8825, PRO2EDRV8825BIG, PRO2EDRV8825
#define FIXEDSTEPMODE   	1
//#define FIXEDSTEPMODE 	2     
//#define FIXEDSTEPMODE 	4
//#define FIXEDSTEPMODE 	8
//#define FIXEDSTEPMODE 	16
//#define FIXEDSTEPMODE 	32
//#define FIXEDSTEPMODE 	64            // for future release
//#define FIXEDSTEPMODE 	128           // for future release
//#define FIXEDSTEPMODE 	256           // for future release

// ======================================================================
// 3: SPECIFY STEPS PER REVOLUTION
// ======================================================================
// stepper motor steps per full revolution using full steps, applies to boards
// PRO2EULN2003, PRO2ESP32ULN2003, PRO2EL298N, PRO2ESP32L298N
// PRO2EL293DMINI, PRO2ESP32L293MINI, PRO2EL9110S, PRO2ESPL9110S
// PRO2EL293DNEMA, PRO2EL293D28BYJ48

#define STEPSPERREVOLUTION 		2048           // 28BYJ-48 stepper motor unipolar with ULN2003 board
//#define STEPSPERREVOLUTION  	200        // NEMA17 FULL STEPPED
//#define STEPSPERREVOLUTION  	400        // NEMA14HM11-0404S 0.9 motor FULL STEPPED
//#define STEPSPERREVOLUTION 	1028        // 17HS13-0404S-PG5
//#define STEPSPERREVOLUTION 	5370        // NEMA17HS13-0404S-PG27
//#define STEPSPERREVOLUTION 	1036        // NEMA14HS13-0804S-PG5
//#define STEPSPERREVOLUTION 	1036        // NEMA16HS13-0604S-PG5

// ======================================================================
// 6: CONTROLLER MODE
// ======================================================================
// to work only via USB cable as Serial port, uncomment the next line
#define CONTROLLERMODE  LOCALSERIAL



#endif
