// ======================================================================
// displays.cpp : myFP2ESP OLED DISPLAY ROUTINES
// (c) Copyright Robert Brown 2014-2021. All Rights Reserved.
// (c) Copyright Holger M, 2021. All Rights Reserved.
// ======================================================================

// ======================================================================
// EXTERNALS
// ======================================================================

#include <Arduino.h>
#include "generalDefinitions.h"
#include "focuserconfig.h"                  // boarddefs.h included as part of focuserconfig.h"
#include "myBoards.h"
#include "FocuserSetupData.h"
#include "images.h"
#include "displays.h"

// ======================================================================
// EXTERNALS - PROTOTYPES
// ======================================================================

extern unsigned long ftargetPosition;       // target position
extern bool  displaystate;

extern DriverBoard  *driverboard;
extern SetupData    *mySetupData;
extern bool TimeCheck(unsigned long, unsigned long);

//__ helper function

bool CheckOledConnected(void)
{
#if defined(ESP8266)
#if (DRVBRD == PRO2EL293DNEMA) || (DRVBRD == PRO2EL293D28BYJ48)
  Wire.begin(mySetupData->get_brdsda(), mySetupData->get_brdsck());        // l293d esp8266 shield
  DebugPrintln("Setup PRO2EL293DNEMA/PRO2EL293D28BYJ48 I2C");
#else
  DebugPrintln("Setup esp8266 I2C");
  Wire.begin();
#endif
#else
  DebugPrintln("Setup esp32 I2C");
  //  Wire.begin(mySetupData->get_brdsda(), mySetupData->get_brdsck());          // esp32
  Wire.begin();
#endif

  Wire.setClock(400000L);                               // 400 kHZ max. speed on I2C

  Wire.beginTransmission(OLED_ADDR);                    // check if OLED display is present
  if (Wire.endTransmission() != 0)
  {
    DebugPrintln("Display not found");
    return false;
  }
  return true;
}

// ======================================================================
// CODE NON OLED
// ======================================================================

//void OLED_NON::oledgraphicmsg(String &str, int val, boolean clrscr) {}
//void OLED_NON::oled_draw_Wifi(int j) {}

void OLED_NON::oledtextmsg(String str, int val, boolean clrscr, boolean nl) {}
void OLED_NON::update_oledtext_position(void) {}
void OLED_NON::update_oledtextdisplay(void) {}
void OLED_NON::Update_Oled(const oled_state x, const connection_status y) {}
void OLED_NON::oled_draw_reboot(void) {}
void OLED_NON::display_on(void) {}
void OLED_NON::display_off(void) {}

OLED_NON::OLED_NON()  {}

// ======================================================================
// CODE OLED GRAPHIC MODE
// ======================================================================

//__constructor
// TODO HOLGER - THIS HAS TO BE CHANGED
// I2CDATAPIN is actually mySetupData->get_brdsda()
// I2CCLKPIN is actually mySetupData->get_brdsck()
// These pins are not known at compile time so has to be setup inside the constructor

// OLED_GRAPHIC::OLED_GRAPHIC(uint8_t _address, uint8_t _sda, uint8_t _scl) : SSD1306Wire(_address, _sda,_scl, GEOMETRY_128_64)
#ifdef USE_SSD1306
OLED_GRAPHIC::OLED_GRAPHIC()  :  SSD1306Wire(OLED_ADDR, 4, 5, GEOMETRY_128_64) , OLED_NON()
#else // Assume USE_SSH1106
OLED_GRAPHIC::OLED_GRAPHIC()  :  SH1106Wire(OLED_ADDR, 4, 5, GEOMETRY_128_64) , OLED_NON()
#endif // #ifdef USE_SSD1306
{
  DebugPrintln(F("start init()"));
  this->init();             // init SSD1306Wire
  delay(1000);

  flipScreenVertically();
  setFont(ArialMT_Plain_10);
  setTextAlignment(TEXT_ALIGN_LEFT);
  clear();
  if (mySetupData->get_showstartscreen())
  {
    drawString(0, 0, "myFocuserPro2 v:" + String(programVersion));
    drawString(0, 12, ProgramAuthor);
  }
  display();

  timestamp = millis();
}

void OLED_GRAPHIC::Update_Oled(const oled_state oled, const connection_status ConnectionStatus)
{
  if (TimeCheck(timestamp, 750))
  {
    timestamp = millis();

    if (oled == oled_on)
    {
      oled_draw_main_update(ConnectionStatus);
    }
    else
    {
      clear();
      display();
    }
  }
}

void OLED_GRAPHIC::oledgraphicmsg(String &str, int val, boolean clrscr)
{
  setTextAlignment(TEXT_ALIGN_LEFT);
  setFont(ArialMT_Plain_10);

  if (clrscr == true)
  {
    clear();
    linecount = 0;
  }
  if (val != -1)
  {
    str += String(val);
  }
  drawString(0, linecount * 12, str);
  display();
  linecount++;
}

void OLED_GRAPHIC::oled_draw_Wifi(int j)
{
  clear();
  setTextAlignment(TEXT_ALIGN_CENTER);
  setFont(ArialMT_Plain_10);
  drawXbm(34, 14, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits); // draw wifi logo

  for (int i = 1; i < 10; i++)
    drawXbm(12 * i, 56, 8, 8, (i == j) ? activeSymbol : inactiveSymbol);

  display();
}

const char heartbeat[] = { '-', '/' , '|', '\\'};

void OLED_GRAPHIC::oled_draw_main_update(const connection_status ConnectionStatus)
{
  char buffer[80];

  clear();
  setTextAlignment(TEXT_ALIGN_CENTER);
  setFont(ArialMT_Plain_24);

  if (ConnectionStatus == disconnected)
  {
    drawString(64, 28, F("offline"));

    setFont(ArialMT_Plain_10);
    //    drawString(64, 0, mySetupData->get_brdname());
    drawString(64, 0, mySetupData->get_brdname());
  }
  else
  {
    char dir = (mySetupData->get_focuserdirection() == moving_in ) ? '<' : '>';
    snprintf(buffer, sizeof(buffer), "%lu:%i %c", driverboard->getposition(), (int)(driverboard->getposition() % mySetupData->get_brdstepmode()), dir);
    drawString(64, 28, buffer);

    setFont(ArialMT_Plain_10);
    snprintf(buffer, sizeof(buffer), "µSteps: %i MaxPos: %lu", mySetupData->get_brdstepmode(), mySetupData->get_maxstep());
    drawString(64, 0, buffer);
    snprintf(buffer, sizeof(buffer), "TargetPos:  %lu", ftargetPosition);
    drawString(64, 12, buffer);
  }

  setTextAlignment(TEXT_ALIGN_LEFT);

  snprintf(buffer, sizeof(buffer), "BL: %i", mySetupData->get_backlashsteps_out());
  drawString(0, 54, buffer);

  snprintf(buffer, sizeof(buffer), "%c", heartbeat[++count_hb % 4]);
  drawString(8, 14, buffer);

  display();
}

void OLED_GRAPHIC::oled_draw_reboot(void)
{
  clear();
  setTextAlignment(TEXT_ALIGN_CENTER);
  setFont(ArialMT_Plain_24);
  drawString(64, 28, "REBOOT"); // Print currentPosition
  display();
}

void OLED_GRAPHIC::display_on(void)
{
  // do nothing here - sort out code later
}

void OLED_GRAPHIC::display_off(void)
{
  // do nothing here - sort out code later
}


// ======================================================================
// Section OLED TEXT
// ======================================================================

void OLED_TEXT::oledtextmsg(String str, int val, boolean clrscr, boolean nl)
{
  if ( clrscr == true)                                  // clear the screen?
  {
    clear();
    setCursor(0, 0);
  }
  if ( nl == true )                                     // need to print a new line?
  {
    if ( val != -1)                                     // need to print a value?
    {
      print(str);
      println(val);
    }
    else
    {
      println(str);
    }
  }
  else
  {
    print(str);
    if ( val != -1 )
    {
      print(val);
    }
  }
  //  display();
}

void OLED_TEXT::displayoledpage0(void)      // display screen
{
  char tempString[20];

  setCursor(0, 0);
  print(CURRENTPOSSTR);
  println(driverboard->getposition());
  print(TARGETPOSSTR);
  println(ftargetPosition);

  print(COILPWRSTR);
  println(mySetupData->get_coilpower());

  print(REVDIRSTR);
  println(mySetupData->get_reversedirection());

  // stepmode setting
  print(STEPMODESTR);
  println(mySetupData->get_brdstepmode());

  //Motor Speed
  print(MOTORSPEEDSTR);
  println(mySetupData->get_motorspeed());

  //MaxSteps
  print(MAXSTEPSSTR);
  ltoa(mySetupData->get_maxstep(), tempString, 10);
  println(tempString);
  //  display();
}

void OLED_TEXT::displayoledpage1(void)
{
  setCursor(0, 0);
  // temperature compensation
  print(TCOMPSTEPSSTR);
  println(mySetupData->get_tempcoefficient());

  print(TCOMPSTATESTR);
  println(mySetupData->get_tempcompenabled());

  print(TCOMPDIRSTR);
  println(mySetupData->get_tcdirection());

  print(BACKLASHINSTR);
  println(mySetupData->get_backlash_in_enabled());

  print(BACKLASHOUTSTR);
  println(mySetupData->get_backlash_out_enabled());

  print(BACKLASHINSTEPSSTR);
  println(mySetupData->get_backlashsteps_in());

  print(BACKLASHOUTSTEPSSTR);
  println(mySetupData->get_backlashsteps_out());
  //  display();
}

void OLED_TEXT::displayoledpage2(void)
{
  setCursor(0, 0);
  println("Local serial");
}

void OLED_TEXT::update_oledtextdisplay(void)
{
  static unsigned long currentMillis;
  static unsigned long olddisplaytimestampNotMoving = millis();
  static byte displaypage = 0;

  currentMillis = millis();                       // see if the display needs updating
  if (((currentMillis - olddisplaytimestampNotMoving) > ((int)mySetupData->get_oledpagetime() * 1000)) || (currentMillis < olddisplaytimestampNotMoving))
  {
    olddisplaytimestampNotMoving = currentMillis; // update the timestamp
    clear();                              // clrscr OLED
    switch (displaypage % 3)
    {
      case 2:   display_oledtext_page2();
        break;
      case 1:   display_oledtext_page1();
        break;
      case 0:
      default:  display_oledtext_page0();
        break;
    }
    displaypage++;
  }
}

void OLED_TEXT::UpdatePositionOledText(void)
{
  setCursor(0, 0);
  print(CURRENTPOSSTR);
  println(driverboard->getposition());

  print(TARGETPOSSTR);
  println(ftargetPosition);
  //  display();
}

void OLED_TEXT::update_oledtext_position(void)
{
  setCursor(0, 0);
  print(CURRENTPOSSTR);
  print(driverboard->getposition());
  clearToEOL();
  println();

  print(TARGETPOSSTR);
  print(ftargetPosition);
  clearToEOL();
  println();
  //  display();
}

void OLED_TEXT::display_on(void)
{
  Display_On();
}

void OLED_TEXT::display_off(void)
{
  Display_Off();
}

OLED_TEXT::OLED_TEXT(void)
{
  // Setup the OLED
#ifdef USE_SSD1306
  // For the OLED 128x64 0.96" display using the SSD1306 driver
  begin(&Adafruit128x64, OLED_ADDR);
#endif
#ifdef USE_SSH1106
  // For the OLED 128x64 1.3" display using the SSH1106 driver
  begin(&SH1106_128x64, OLED_ADDR);
#endif
  setFont(Adafruit5x7);
  //  setcolor(WHITE);                    // draw white text
  clear();                              // clrscr OLED

  displaystate = true;

  Display_Normal();                     // black on white
  InverseCharOff();

  display_on();                         // display ON
  println(mySetupData->get_brdname());  // print startup screen
  println(programVersion);
  println(ProgramAuthor);
}

void OLED_TEXT::display_oledtext_page0(void)           // display screen
{
  char tempString[20];

  home();
  print(CURRENTPOSSTR);
  print(driverboard->getposition());
  clearToEOL();

  println();
  print(TARGETPOSSTR);
  print(ftargetPosition);
  clearToEOL();
  println();

  print(COILPWRSTR);
  print(mySetupData->get_coilpower());
  clearToEOL();
  println();

  print(REVDIRSTR);
  print(mySetupData->get_reversedirection());
  clearToEOL();
  println();

  // stepmode setting
  print(STEPMODESTR);
  print(mySetupData->get_brdstepmode());
  clearToEOL();
  println();

  //Motor Speed
  print(MOTORSPEEDSTR);
  print(mySetupData->get_motorspeed());
  clearToEOL();
  println();

  //MaxSteps
  print(MAXSTEPSSTR);
  ltoa(mySetupData->get_maxstep(), tempString, 10);
  print(tempString);
  clearToEOL();
  println();
}

void OLED_TEXT::display_oledtext_page1(void)
{
  // temperature compensation
  print(TCOMPSTEPSSTR);
  print(mySetupData->get_tempcoefficient());
  clearToEOL();
  println();

  print(TCOMPSTATESTR);
  print(mySetupData->get_tempcompenabled());
  clearToEOL();
  println();

  print(TCOMPDIRSTR);
  print(mySetupData->get_tcdirection());
  clearToEOL();
  println();

  print(BACKLASHINSTR);
  print(mySetupData->get_backlash_in_enabled());
  clearToEOL();
  println();

  print(BACKLASHOUTSTR);
  print(mySetupData->get_backlash_out_enabled());
  clearToEOL();
  println();

  print(BACKLASHINSTEPSSTR);
  print(mySetupData->get_backlashsteps_in());
  clearToEOL();
  println();

  print(BACKLASHOUTSTEPSSTR);
  print(mySetupData->get_backlashsteps_out());
  clearToEOL();
  println();
}

void OLED_TEXT::display_oledtext_page2(void)
{
  setCursor(0, 0);
  println("Local Serial");
}
