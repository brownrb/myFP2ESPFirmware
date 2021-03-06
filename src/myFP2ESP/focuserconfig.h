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
//#define DRVBRD 	WEMOSDRV8825H                    // driver definition for Holger
//#define DRVBRD 	WEMOSDRV8825
//#define DRVBRD 	PRO2EULN2003
//#define DRVBRD 	PRO2EDRV8825
//#define DRVBRD 	PRO2EL293DNEMA
//#define DRVBRD 	PRO2EL293D28BYJ48
//#define DRVBRD 	PRO2EL298N
//#define DRVBRD 	PRO2EL293DMINI
//#define DRVBRD 	PRO2EL9110S
//#define DRVBRD 	CUSTOMBRD
// ESP32 Boards
//#define DRVBRD 	PRO2ESP32DRV8825
#define DRVBRD 	PRO2ESP32ULN2003
//#define DRVBRD 	PRO2ESP32L298N
//#define DRVBRD 	PRO2ESP32L293DMINI
//#define DRVBRD 	PRO2ESP32L9110S
//#define DRVBRD 	PRO2ESP32R3WEMOS
//#define DRVBRD  PRO2ESP32TMC2225
//#define DRVBRD  PRO2ESP32TMC2209
//#define DRVBRD  PRO2ESP32TMC2209P              // this is for Paul using TMC2209 - 58.jsn
//#define DRVBRD  PRO2ESP32ST6128                // This is board for CLOSED LOOP ST6128 driver
//#define DRVBRD 	CUSTOMBRD

// On bootup following a controller firmware update, a default driver board file 
// is created based on your board selection above.
// In the MANAGEMENT server you can edit the board pin numbers and save the 
// config [only if you know what you are doing].
// No changes are necessary if you are using one of the available PCBoards for this project
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
//#define STEPSPERREVOLUTION   200        // NEMA17 FULL STEPPED
//#define STEPSPERREVOLUTION   400        // NEMA14HM11-0404S 0.9 motor FULL STEPPED
//#define STEPSPERREVOLUTION 	1028        // 17HS13-0404S-PG5
//#define STEPSPERREVOLUTION 	5370        // NEMA17HS13-0404S-PG27
//#define STEPSPERREVOLUTION 	1036        // NEMA14HS13-0804S-PG5
//#define STEPSPERREVOLUTION 	1036        // NEMA16HS13-0604S-PG5

// ======================================================================
// 4: SPECIFY OLED DISPLAY AND DRIVER TYPE: 
// ======================================================================
// If NOT using an OLED skip this section. 
// To enable the OLED_TEXT or OLED_GRAPHIC display uncomment the related line below. 

//#define OLED_MODE 	OLED_TEXT       // OLED text only mode
//#define OLED_MODE 	OLED_GRAPHIC    // OLED graphic mode

// and
// only uncomment one of the following USE_SSxxxx lines depending upon your lcd type
// For the OLED 128x64 0.96" display using the SSD1306 driver, uncomment the following line
#define USE_SSD1306   1

// For the OLED 128x64 1.3" display using the SSH1106 driver, uncomment the following line
//#define USE_SSH1106   2

// ======================================================================
// 5: SPECIFY HARDWARE OPTIONS
// ======================================================================
// To enable the 2-Axis Joystick [ESP32 only], uncomment the next line
//#define JOYSTICK1 	1

// To enable the Keyes KY-023 PS2 2-Axis Joystick [ESP32 only],
// uncomment the next line
//#define JOYSTICK2 	2

// To enable the Infrared remote controller [ESP32 only], uncomment the next line
//#define INFRAREDREMOTE

// ======================================================================
// 6: CONTROLLER MODE
// ======================================================================
// Note: defines for Controller Modes are in generalDefinitions.h
// which should be included by anycode before including focuserconfig.h

// The following controller modes are MUTUALLY EXCLUSIVE and cannot be combined

// to work as an access point, define accesspoint - cannot use DUCKDNS
#define CONTROLLERMODE  ACCESSPOINT

// to work as a station accessing a AP, define stationmode
//#define CONTROLLERMODE  STATIONMODE

// to work only via USB cable as Serial port, uncomment the next line
//#define CONTROLLERMODE  LOCALSERIAL

// to enable Bluetooth mode, uncomment the next line [ESP32 only]
//#define CONTROLLERMODE  BLUETOOTHMODE

// ======================================================================
// 7: SPECIFY CONTROLLER OPTIONS
// ======================================================================
// To enable OTA updates, uncomment the next line [only works in STATIONMODE]
//#define OTAUPDATES 	5

// mdns support [myfp2eap.local:8080]
// to enable multicast DNS, uncomment the next line [only works in STATIONMODE]
//#define MDNSSERVER 	8

// Management Server Control Interface [Port 6060] - DO NOT CHANGE
#define MANAGEMENT 	9

// Cannot use DuckDNS with ACCESSPOINT, BLUETOOTHMODE or LOCALSERIAL mode
// To enable DUCKDNS [STATIONMODE only]
//#define USEDUCKDNS 	1

// to enable reading SSID and PASSWORD from SPIFFS file wificonfig at 
// boot time, uncomment the following file
//#define READWIFICONFIG 	1

// ======================================================================
// 8: CONTROLLER PROTOCOL
// ======================================================================
// Enable ONE of the following NOT BOTH
// to talk to a myFocuserPro2, myFP2M, myFP2N or myFP2ESP controller
// uncomment the next line
#define PROTOCOL  MYFP2ESP_PROTOCOL

// to talk to a myFocuserPro or Moonlite controller uncomment the next line
//#define PROTOCOL  MOONLITE_PROTOCOL

// ======================================================================
// TMC2209 HOMEPOSITIONSWITCH OPTIONS
// ======================================================================
// If your DRVBRD is NOT TMC2209 or TMC2209P THEN DO NOT ENABLE THESE OPTIONS
// If using STALLGUARD or HOMEPOSITIONSWITCH, uncomment one of the following (not both)
//#define USE_STALL_GUARD 1

#define USE_PHYSICAL_SWITCH 2
// A physical home switch for TMC2209 requires different jumper settings on the PCB
// Please refer to documentation PDF for wiring and other options

// ======================================================================
// DO NOT CHANGE
// CHECK BOARD AND HW OPTIONS
// ======================================================================
#ifndef DRVBRD
#error No DRVBRD defined
#endif

#if defined(USE_STALL_GUARD) && defined(USE_PHYSICAL_SWITCH)
#halt // ERROR you cannot have both USE_STALL_GUARD and USE_PHYSICAL SWITCH defined - must be one or the other
#endif

#if (DRVBRD == PRO2ESP32TMC2209 || DRVBRD == PRO2ESP32TMC2209P)
#if !defined(USE_STALL_GUARD) && !defined(USE_PHYSICAL_SWITCH)
#error You must define either USE_STALL_GUARD or USE_PHYSICAL_SWITCH when using a hpsw with TMC2209 driver
#endif
#endif

#if (DRVBRD == WEMOSDRV8825H     || DRVBRD == WEMOSDRV8825     || DRVBRD == PRO2EULN2003      || DRVBRD == PRO2EDRV8825 \
  || DRVBRD == PRO2EDRV8825BIG   || DRVBRD == PRO2EL293DNEMA   || DRVBRD == PRO2EL293D28BYJ48 || DRVBRD == PRO2EL298N \
  || DRVBRD == PRO2EL293DMINI    || DRVBRD == PRO2EL9110S      || DRVBRD == PRO2EL9110S       || DRVBRD == CUSTOMBRD \
  || DRVBRD == PRO2ESP32DRV8825  || DRVBRD == PRO2ESP32ULN2003 || DRVBRD == PRO2ESP32L298N    || DRVBRD == PRO2ESP32L293DMINI \
  || DRVBRD == PRO2ESP32L9110S   || DRVBRD == PRO2ESP32R3WEMOS || DRVBRD == PRO2ESP32TMC2225  || DRVBRD == PRO2ESP32ST6128 )
#if defined(USE_STALL_GUARD) || defined(USE_PHYSICAL_SWITCH)
#error You cannot use USE_STALL_GUARD or USE_PHYSICAL_SWITCH with your Board
#endif
#endif

#if !defined(PROTOCOL)
#error Protocol has not been defined, must be MYFP2ESP_PROTOCOL or MOONLITE_PROTOCOL
#endif

#if (PROTOCOL == MOONLITE_PROTOCOL)
#if (CONTROLLERMODE == ACCESSPOINT || CONTROLLERMODE == STATIONMODE)
#error You cannot use ACCESSPOINT or STATIONMODE with the MOONLITE Protocol
#endif
#if (CONTROLLERMODE != LOCALSERIAL)
#error MOONLITE Protocol requires LOCALSERIAL
#endif // #if (CONTROLLERMODE != LOCALSERIAL)
#endif // #if (PROTOCOL == MOONLITE_PROTOCOL)

#if defined(USE_SSD1306) && defined(USE_SSH1106)
#error Define either USE_SSD1306 or USE_SSH1106 if using an OLEDDISPLAY
#endif

#ifndef USE_SSD1306
#ifndef USE_SSH1106
#error Either USE_SSD1306 or USE_SSH1106 must be enabled if using an OLED DISPLAY
#endif
#endif

// DO NOT CHANGE
#if (DRVBRD == WEMOSDRV8825 || DRVBRD == PRO2EDRV8825 || DRVBRD == PRO2EDRV8825BIG \
  || DRVBRD == PRO2EULN2003 || DRVBRD == PRO2EL298N   || DRVBRD == PRO2EL293DMINI \
  || DRVBRD == PRO2EL9110S  || DRVBRD == PRO2EL293D   || DRVBRD == PRO2ESP32R3WEMOS )
// no support for pushbuttons, inout leds, irremote
#ifdef PUSHBUTTONS
#error PUSHBUTTONS not supported for WEMOS or NODEMCUV1 ESP8266 chips
#endif
#ifdef INFRAREDREMOTE
#error INFRAREDREMOTE not supported for WEMOS or NODEMCUV1 ESP8266 chips
#endif
#if defined(JOYSTICK1) || defined(JOYSTICK2)
#error JOYSTICK not supported for WEMOS or NODEMCUV1 ESP8266 chips
#endif
#endif // #if defined(JOYSTICK1) || defined(JOYSTICK2)

// Check board availability for a specific controller mode
#if (DRVBRD == WEMOSDRV8825 || DRVBRD == PRO2EDRV8825 || DRVBRD == PRO2EDRV8825BIG \
  || DRVBRD == PRO2EULN2003 || DRVBRD == PRO2EL298N   || DRVBRD == PRO2EL293DMINI \
  || DRVBRD == PRO2EL9110S  || DRVBRD == PRO2EL293D )
// no support for bluetooth mode
#if (CONTROLLERMODE == BLUETOOTHMODE)
#error BLUETOOTHMODE not supported for WEMOS or NODEMCUV1 ESP8266 chips
#endif
#endif

#if (DRVBRD == PRO2EL293DNEMA) || (DRVBRD == PRO2EL293D28BYJ48)
#if (CONTROLLERMODE == LOCALSERIAL)
#error LOCALSERIAL not supported L293D Motor Shield [ESP8266] boards
#endif
#endif // #if ((DRVBRD == PRO2EL293DNEMA) || (DRVBRD == PRO2EL293D28BYJ48))

#if defined(JOYSTICK1) || defined(JOYSTICK2)
#ifdef PUSHBUTTONS
#error PUSHBUTTONS and JOYSTICK cannot be enabled at the same time
#endif
#endif // #if defined(JOYSTICK1) || defined(JOYSTICK2)

#ifdef JOYSTICK1
#ifdef JOYSTICK2
#error JOYSTICK1 and JOYSTICK2 cannot be defined at the same time
#endif
#endif // #ifdef JOYSTICK1

// ======================================================================
// CHECK CONTROLLER OPTIONS
// ======================================================================

#if defined(OTAUPDATES)
#if (CONTROLLERMODE == BLUETOOTHMODE) || (CONTROLLERMODE == LOCALSERIAL)
#error OTAUPDATES cannot be defined together with BLUETOOTHMODE or LOCALSERIAL
#endif
#if (CONTROLLERMODE == ACCESSPOINT)
#error OTAUPDATES cannot be defined together with ACCESSPOINT
#endif
#endif // #if defined(OTAUPDATES)

#if defined(MDNSSERVER)
#if (CONTROLLERMODE == BLUETOOTHMODE) || (CONTROLLERMODE == LOCALSERIAL) || (CONTROLLERMODE == ACCESSPOINT)
#error mDNS only available with CONTROLLERMODE == STATIONMODE
#endif
#endif // MDNSSERVER

// Check management server only available in accesspoint or stationmode
#ifdef MANAGEMENT
#if (CONTROLLERMODE == BLUETOOTHMODE) || (CONTROLLERMODE == LOCALSERIAL)
#error MANAGEMENT service cannot be enabled using BLUETOOTHMODE or LOCALSERIAL
#endif
#endif

// cannot use DuckDNS with ACCESSPOINT, BLUETOOTHMODE or LOCALSERIAL mode
#ifdef USEDUCKDNS
#if (CONTROLLERMODE == BLUETOOTHMODE) || (CONTROLLERMODE == LOCALSERIAL) || (CONTROLLERMODE == ACCESSPOINT)
#error DUCKDNS only works with STATIONMODE
#endif
#ifndef STATIONMODE
#error DUCKDNS only works with STATIONMODE, you must enable STATIONMODE
#endif
#endif

// DO NOT CHANGE
#if defined(READWIFICONFIG)
#if (CONTROLLERMODE == BLUETOOTH) || (CONTROLLERMODE == LOCALSERIAL) || (CONTROLLERMODE == ACCESSPOINT)
#error READWIFICONFIG only available in STATIONMODE
#endif
#endif // #if defined(READWIFICONFIG)


// ======================================================================
// CHECK CONTROLLER MODES
// ======================================================================

#if !defined(CONTROLLERMODE) 
#error CONTROLLERMODE NOT DEFINED
#endif

// check bluetooth mode, cannot be used for esp8266 and accesspoint and stationmode and localserial
#if (CONTROLLERMODE == BLUETOOTHMODE)
#if defined(ESP8266)
#error Bluetooth only available on ESP32 boards
#endif
#ifdef OTAUPDATES
#error OTAUPDATES cannot work in BLUETOOTHMODE
#endif
#ifdef MDNSSERVER
#error MDNSSERVER cannot work in BLUETOOTHMODE
#endif
#ifdef MANAGEMENT
#error MANAGEMENT cannot work in BLUETOOTHMODE
#endif
#ifdef READWIFICONFIG
#error READWIFICONFIG cannot work in BLUETOOTHMODE
#endif
#ifdef USEDUCKDNS
#error DUCKDNS cannot work in BLUETOOTHMODE
#endif
#endif // #if (CONTROLLERMODE == BLUETOOTHMODE)

// check localserial mode
#if (CONTROLLERMODE == LOCALSERIAL)
#ifdef OTAUPDATES
#error Cannot enable OTAUPDATES with LOCALSERIAL
#endif
#ifdef MDNSSERVER
#error Cannot enable MDNSSERVER with LOCALSERIAL
#endif
#ifdef MANAGEMENT
#error Cannot enable MANAGEMENT with LOCALSERIAL
#endif
#ifdef DEBUG
#error Cannot enable DEBUG with LOCALSERIAL
#endif
#ifdef READWIFICONFIG
#error READWIFICONFIG only available with CONTROLLERMODE == STATIONMODE
#endif
#ifdef USEDUCKDNS
#error Cannot enable DUCKDNS with BLUETOOTHMODE
#endif
#endif // #if (CONTROLLERMODE == LOCALSERIAL)

// check accesspoint mode
#if (CONTROLLERMODE == ACCESSPOINT)
#ifdef MDNSSERVER
#error Cannot enable MDNSSERVER with ACCESSPOINT
#endif
#ifdef OTAUPDATES
#error Cannot enable OTAUPDATES with ACCESSPOINT
#endif
#ifdef USEDUCKDNS
#error Cannot enable DUCKDNS with ACCESSPOINT
#endif
#endif // #if (CONTROLLERMODE == ACCESSPOINT)


// ======================================================================
// DO NOT CHANGE: THESE DEFINITIONS NOW DEPRECATED
// ======================================================================
// To enable In and Out Pushbuttons [ESP32 only], uncomment the next line
// This has moved to MANAGEMENT SERVER

// do NOT uncomment HOMEPOSITIONSWITCH if you do not have the switch fitted
// To enable the HOMEPOSITION SWITCH [ESP32 only], uncomment the next line
// This has moved to MANAGEMENT SERVER

// To enable TEMPERATUREPROBE, uncomment the next line
// This has moved to MANAGEMENT SERVER

// To enable BACKLASH IN/OUT in this firmware, uncomment the next line
// This has moved to MANAGEMENT SERVER

// To enable INOUTLEDS [ESP32 only], uncomment the next line
// This has moved to MANAGEMENT SERVER

// To enable SHOWSTARTSCRN - the start boot screen showing startup messages, 
// uncomment the next line
// This has moved to MANAGEMENT SERVER

// To enable SHOWHPSWMESSAGES - display Home Position Switch Messages 
// on the display, uncomment the next line
// This has moved to MANAGEMENT SERVER

// to enable this focuser for ASCOMREMOTE support [Port 4040], uncomment the next line
// This has moved to MANAGEMENT SERVER

// [recommend use Internet Explorer or Microsoft Edge Browser]
// to enable WEBSERVER interface [Port 80], uncomment the next line 
// This has moved to MANAGEMENT SERVER

// To download the file [MANAGEMENTFORCEDOWNLOAD] instead of displaying file 
// content in web browser, uncomment the next line
// This has moved to MANAGEMENT SERVER

#endif
