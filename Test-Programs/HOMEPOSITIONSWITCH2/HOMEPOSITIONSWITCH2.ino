// ----------------------------------------------------------------------------------------------
// TITLE: myFP2ESP FIRMWARE OFFICIAL RELEASE
// TEST HOMEPOSITIONSWITCH-2
// ----------------------------------------------------------------------------------------------

// Output is shown in serial port window
// This program checks for the HPSW - you must close and open the switch by hand
// The steps you should follow are listed below
// 1. Make sure motor is not connected to focuser and home position switch is connected and state is OPEN
// 2. Motor position will be set to position 1000 and speed to slow
// 3. After a small delay motor will begin moving: direction is not important
// 4. When the motor moves, press and hold the HPSW closed
// 5. The motor should stop. 
// 6. After a 1s delay you will be told the motor is now moving out, this is in opposite directio
// 7. As motor moves out, release HPSW as OPEN
// 8. Motor will stop immediately, and position will be set to 0


// ----------------------------------------------------------------------------------------------
// COPYRIGHT
// ----------------------------------------------------------------------------------------------
// (c) Copyright Robert Brown 2014-2021. All Rights Reserved.
// (c) Copyright Holger M, 2019-2021. All Rights Reserved.

// ----------------------------------------------------------------------------------------------
// 1: SET BOARD TYPE
// ----------------------------------------------------------------------------------------------
// SET BOARD TYPE in myBoards.h
// Currently set to PRO2ESP32DRV8825

#include "myboards.h"

#define HPSWPIN       4
byte    isMoving;                           // is the motor currently moving
byte    motorspeed = 0;                     // slow speed 

unsigned long fcurrentPosition;
unsigned long ftargetPosition;              // target position
volatile bool halt_alert;

DriverBoard* driverboard;

byte TimeCheck(unsigned long x, unsigned long Delay)
{
  unsigned long y = x + Delay;
  unsigned long z = millis();                           // pick current time

  if ((x > y) && (x < z))
    return 0;                                           // overflow y
  if ((x < y) && ( x > z))
    return 1;                                           // overflow z

  return (y < z);                                       // no or (z and y) overflow
}

// move motor without updating position, used by sethomeposition
void steppermotormove(byte dir )                // direction moving_in, moving_out ^ reverse direction
{
  driverboard->movemotor(dir, false);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println("Init HPSW");
  pinMode(HPSWPIN, INPUT_PULLUP);
  Serial.println("Init focuserPosition to 1000");
  fcurrentPosition = ftargetPosition = 1000;
  isMoving = 0;
  halt_alert = false;
  Serial.println("Setup DRVBRD");
  driverboard = new DriverBoard(DRVBRD, fcurrentPosition );
  driverboard->setposition(fcurrentPosition);
  Serial.println("Setting motor speed to slow");
  motorspeed = 0;           // slow
  Serial.println("5s delay: After which target position will be changed to 0");
  delay(5000);
  Serial.println("Starting move to 0: During this move - and before motor stops, close the Home Position Switch");
  delay(1000);
  ftargetPosition = 0;      // home
}

extern volatile uint32_t stepcount;             // number of steps to go in timer interrupt service routine, in myBoards.cpp
extern volatile bool     timerSemaphore;        // in myBoards.cpp

void loop()
{
  static StateMachineStates MainStateMachine = State_Idle;
  static bool     DirOfTravel = moving_in;                          // really does not matter in this example?
  static uint32_t steps = 0;
  static uint32_t TimeStampDelayAfterMove = 0;
  
  int stepstaken = 0;
  bool hpswstate = false;
  
  switch (MainStateMachine)
  {
    case State_Idle:
      if (driverboard->getposition() != ftargetPosition)
      {
        isMoving = 1;
        driverboard->enablemotor();
        MainStateMachine = State_InitMove;
        Serial.println("Change state to InitMove");
      }
      else
      {
        // focuser stationary. isMoving is 0
        driverboard->releasemotor();

      }
    case State_InitMove:
      delay(1000);
      isMoving = 1;
      DirOfTravel = moving_in;
      driverboard->enablemotor();

      // if target pos > current pos then steps = target pos - current pos
      // if target pos < current pos then steps = current pos - target pos
      steps = (ftargetPosition > driverboard->getposition()) ? ftargetPosition - driverboard->getposition() : driverboard->getposition() - ftargetPosition;
      Serial.println("initmove: We are now moving the motor to 0 at slow speed");
      driverboard->initmove(DirOfTravel, steps, 0, 0);
      MainStateMachine = State_Moving;
      break;

    //_______________________________State_Moving

    case State_Moving:
      if ( timerSemaphore == true )
      {
        // move has completed, the driverboard keeps track of focuser position
        Serial.println("Move completed. We have reached 0 - did you forget to close HPSW?");
        Serial.println("Going to State_DelayAfterMove");
        MainStateMachine = State_DelayAfterMove;
        delay(1000);
      }
      else
      {
        // timer semaphore is false. still moving, we need to check for halt and hpsw closure
        
        // halt_alert is generated by a comms.h when an application sends a halt request, wo no need to check for it here

        // HPS_alert occurs when the HPSW is closed and comes from myBoards.cpp when moving the motor
        
        if (HPS_alert)                                    // check if home position sensor activated?
        {
          if (driverboard->getposition() > 0)
          {
            Serial.println(HPCLOSEDFPNOT0STR);
          }
          else
          {
            Serial.println(HPCLOSEDFP0STR);
          } // if (driverboard->getposition() > 0)
          Serial.println(STATESETHOMEPOSITION);
          ftargetPosition = 0;
          driverboard->setposition(0);
          Serial.println("HPSW Closed: Now setting Home Position. Moving to State_SetHomePosition");
          MainStateMachine = State_SetHomePosition;
          delay(1000);
        } // if (HPS_alert)
      }
      break;

    case State_SetHomePosition:                         // move out till home position switch opens
      Serial.println("State_SetHomePosition");
      Serial.println("Will now move out till HPSW opens.");
      Serial.println("You will need to do this manually as soon as motor starts moving out");

      // HOME POSITION SWITCH IS CLOSED - Step out till switch opens then set position = 0
      stepstaken = 0;                                   // Count number of steps to prevent going too far
      DirOfTravel = !DirOfTravel;                       // We were going in, now we need to reverse and go out
      hpswstate = HPSWCLOSED;                           // We know we got here because switch was closed
      while ( hpswstate == HPSWCLOSED )
      {
        steppermotormove(DirOfTravel);                  // take 1 step
        stepstaken++;                                   // increment steps taken
        hpswstate = !(digitalRead(HPSWPIN));            // read state of HPSW
      }
      Serial.println("HPSW detected as open: Will stop motor and set position as 0");;
      ftargetPosition = 0;
      driverboard->setposition(0);
      Serial.println("State now set to State_DelayAfterMove");
      MainStateMachine = State_DelayAfterMove;
      TimeStampDelayAfterMove = millis();
      break;

    //_______________________________State_DelayAfterMove

    case State_DelayAfterMove:
      if (TimeCheck(TimeStampDelayAfterMove , 200))     // set delayaftermove time to 200mS
      {
        isMoving = 0;
        Serial.println("State now set to State_Idle");
        MainStateMachine = State_Idle;
      }
      break;

    default:
      DebugPrintln("Error: wrong State => State_Idle");
      MainStateMachine = State_Idle;
      break;
  }
} // end Loop()
