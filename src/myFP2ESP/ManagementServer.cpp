// ======================================================================
// ManagementServer.cpp : myFP2ESP MANAGEMENT SERVER ROUTINES
// (c) Copyright Robert Brown 2014-2021. All Rights Reserved.
// (c) Copyright Holger M, 2019-2021. All Rights Reserved.
// ======================================================================

// ======================================================================
// Includes
// ======================================================================
#include <Arduino.h>
#include "generalDefinitions.h"
#include "focuserconfig.h"                  // boarddefs.h included as part of focuserconfig.h"
#include "myBoards.h"
#include "FocuserSetupData.h"
#include "images.h"

#if defined(ESP8266)                        // this "define(ESP8266)" comes from Arduino IDE
#include <FS.h>                             // include the SPIFFS library  
#else                                       // otherwise assume ESP32
#include "SPIFFS.h"
#endif

#include "displays.h"

// ======================================================================
// Extern Data
// ======================================================================
extern SetupData     *mySetupData;
extern DriverBoard   *driverboard;
extern int           DefaultBoardNumber;
extern int           brdfixedstepmode;
extern OLED_NON      *myoled;
#include "temp.h"
extern TempProbe     *myTempProbe;

extern volatile bool halt_alert;
extern unsigned long ftargetPosition;
extern byte isMoving;
extern int  packetsreceived;
extern int  packetssent;
extern bool mdnsserverstate;                       // states for services, RUNNING | STOPPED
extern bool webserverstate;
extern bool ascomserverstate;
extern bool ascomdiscoverystate;
extern bool managementserverstate;
extern bool tcpipserverstate;
extern bool otaupdatestate;
extern bool duckdnsstate;
extern bool displaystate;
extern int  staticip;
extern int  tprobe1;

// ======================================================================
// Extern functions
// ======================================================================
extern void start_tcpipserver(void);
extern void stop_tcpipserver(void);
extern void start_webserver(void);
extern void stop_webserver(void);
extern void start_ascomremoteserver(void);
extern void stop_ascomremoteserver(void);
extern void software_Reboot(int);
extern long getrssi(void);
extern bool init_leds(void);
extern bool init_homepositionswitch(void);
extern bool init_pushbuttons(void);

#ifdef MDNSSERVER
extern void start_mdns_service(void);
extern void stop_mdns_service(void);
#endif

// ======================================================================
// Defines
// ======================================================================
#ifndef STATICIPON
#define STATICIPON    1
#endif

// ======================================================================
// Forward Declarations
// ======================================================================
void MANAGEMENT_sendadminpg5(void);
void MANAGEMENT_sendadminpg4(void);
void MANAGEMENT_sendadminpg3(void);
void MANAGEMENT_sendadminpg2(void);
void MANAGEMENT_sendadminpg1(void);

// ======================================================================
// MANAGEMENT INTERFACE - CHANGE AT YOUR OWN PERIL
// ======================================================================
#ifdef MANAGEMENT

#if defined(ESP8266)
#include <ESP8266WebServer.h>
#else
#include <WebServer.h>
#endif // if defined(esp8266)

#if defined(ESP8266)
#undef DEBUG_ESP_HTTP_SERVER
ESP8266WebServer mserver(MSSERVERPORT);
#else
WebServer mserver(MSSERVERPORT);
#endif // if defined(esp8266)

String MSpg;
String BoardConfigJson;
File   fsUploadFile;

boolean ishexdigit( char c )
{
  if ( (c >= '0') && (c <= '9') )                 // is a digit
  {
    return true;
  }
  if ( (c >= 'a') && (c <= 'f') )                 // is a-f
  {
    return true;
  }
  if ( (c >= 'A') && (c <= 'F') )                 // is A-F
  {
    return true;
  }
  return false;
}

// convert the file extension to the MIME type
String MANAGEMENT_getcontenttype(String filename)
{
  String retval = "text/plain";
  if (filename.endsWith(".html"))
  {
    retval = "text/html";
  }
  else if (filename.endsWith(".css"))
  {
    retval = "text/css";
  }
  else if (filename.endsWith(".js"))
  {
    retval = "application/javascript";
  }
  else if (filename.endsWith(".ico"))
  {
    retval = "image/x-icon";
  }
  //retval = "application/octet-stream";
  return retval;
}

// sends html header to client
void MANAGEMENT_sendmyheader(void)
{
  //mserver.sendHeader(F(CACHECONTROLSTR), F(NOCACHENOSTORESTR));
  //mserver.sendHeader(F(PRAGMASTR), F(NOCACHESTR));
  //mserver.sendHeader(F(EXPIRESSTR), "-1");
  //mserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  //mserver.send(NORMALWEBPAGE, F(TEXTPAGETYPE), "");
  mserver.client().println("HTTP/1.1 200 OK");
  mserver.client().println("Content-type:text/html");
  //mserver.client().println("Connection: close");       // only valid on http/1.0
  mserver.client().println();
}

// sends html page to web client
void MANAGEMENT_sendmycontent()
{
  mserver.client().print(MSpg);
}

// send the requested file to the client (if it exists)
bool MANAGEMENT_handlefileread(String path)
{
  MSrvr_DebugPrintln("handleFileRead: " + path);
  if (path.endsWith("/"))
  {
    path += "index.html";                               // if a folder is requested, send the index file
  }
  String contentType = MANAGEMENT_getcontenttype(path); // get the MIME type
  if ( SPIFFS.exists(path) )                            // if the file exists
  {
    File file = SPIFFS.open(path, "r");                 // open it
    if ( mySetupData->get_forcedownload() == 1)         // should the file be downloaded or displayed?
    {
      if ( path.indexOf(".html") == -1)
      {
        // if not an html file, force download : html files will be displayed in browser
        mserver.sendHeader("Content-Type", "application/octet-stream");
        mserver.sendHeader("Content-Disposition", "attachment");
      }
    }
    mserver.streamFile(file, contentType);              // and send it to the client
    file.close();                                       // then finish by closing the file
    return true;
  }
  else
  {
    TRACE();
    MSrvr_DebugPrintln("file not found");
    return false;                                       // if the file doesn't exist, return false
  }
}

// checks POST data for request reboot controller and performs reboot if required
void MANAGEMENT_checkreboot(void)
{
  String msg = mserver.arg("srestart");                 // if reboot controller
  if ( msg != "" )
  {
    String WaitPage = "<html><meta http-equiv=refresh content=\"" + String(MSREBOOTPAGEDELAY) + "\"><head><title>Management Server></title></head><body><p>Please wait, controller rebooting.</p></body></html>";
    mserver.send(NORMALWEBPAGE, TEXTPAGETYPE, WaitPage );
    WaitPage = "";
    delay(1000);                                        // wait for page to be sent
    software_Reboot(REBOOTDELAY);
  }
}

// handles POST from msdelete page
void MANAGEMENT_handledeletefile()
{
  String df = mserver.arg("fname");                     // check server arguments, df has filename
  if ( df != "" )                                       // check for file in spiffs
  {
    // Filesystem was started earlier when server was started so assume it has started
    // df = "/" + df;
    if ( df[0] != '/')
    {
      df = '/' + df;
    }
    // load the msdeleteok.html file
    if ( SPIFFS.exists("/msdeleteok.html"))             // load page from fs - wsnotfound.html
    {
      // open file for read
      File file = SPIFFS.open("/msdeleteok.html", "r");
      // read contents into string
      MSpg = file.readString();
      file.close();

      String bcol = mySetupData->get_wp_backcolor();
      MSpg.replace("%BKC%", bcol);
      String txtcol = mySetupData->get_wp_textcolor();
      MSpg.replace("%TXC%", txtcol);
      String ticol = mySetupData->get_wp_titlecolor();
      MSpg.replace("%TIC%", ticol);
      String hcol = mySetupData->get_wp_headercolor();
      MSpg.replace("%HEC%", hcol);
      MSpg.replace("%VER%", String(programVersion));
      MSpg.replace("%NAM%", mySetupData->get_brdname());

      MSpg.replace("%FIL%", df );
      // add code to handle reboot controller
      MSpg.replace("%BT%", String(CREBOOTSTR));
      MSpg.replace("%HEA%", String(ESP.getFreeHeap()));

      if ( SPIFFS.exists(df))
      {
        if ( SPIFFS.remove(df))
        {
          MSpg.replace("%STA%", "deleted.");
        }
        else
        {
          MSpg.replace("%STA%", "could not be deleted.");
        }
      }
      else
      {
        MSpg.replace("%STA%", "does not exist.");
      }
    }
    else // if ( SPIFFS.exists("/msdeleteok.html"))
    {
      // spiffs file msdeleteok.html did not exist
      MSpg = "<html><head><title>Management Server</title></head><body><p>msdeleteok.html not found</p><p><form action=\"/\" method=\"GET\"><input type=\"submit\" value=\"HOMEPAGE\"></form></p></body></html>";
    }
  }
  else
  {
    // null argument has been passed
    MSpg = "<html><head><title>Management Server</title></head><body><p>Null argument found</p><p><form action=\"/\" method=\"GET\"><input type=\"submit\" value=\"HOMEPAGE\"></form></p></body></html>";
  }
  MANAGEMENT_sendmyheader();
  MANAGEMENT_sendmycontent();
  MSpg = "";
}

// builds msdelete page
void MANAGEMENT_deletepage()
{
  // Filesystem was started earlier when server was started so assume it has started
  if ( SPIFFS.exists("/msdelete.html") )                // check for the webpage
  {
    File file = SPIFFS.open("/msdelete.html", "r");     // open it
    MSpg = file.readString();                           // read contents into string
    file.close();

    // Web page colors
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
  }
  else
  {
    // msdelete.html not found
    TRACE();
    MSrvr_DebugPrintln("file not found");
    MSpg = "file not found";
  }
  mserver.send(NORMALWEBPAGE, F(TEXTPAGETYPE), MSpg);
}

// lists all files in file system
void MANAGEMENT_listFSfiles(void)
{
  // spiffs was started earlier when server was started so assume it has started
  // example code taken from FSBrowser
  String path = "/";
  DebugPrintln("MANAGEMENT_listFSfiles: " + path);
#if defined(ESP8266)
  String output = "{[";
  Dir dir = SPIFFS.openDir("/");
  while (dir.next())
  {
    output += "{" + dir.fileName() + "}, ";
  }
  output += "]}";
  mserver.send(NORMALWEBPAGE, String(JSONTEXTPAGETYPE), output);
#else // ESP32
  File root = SPIFFS.open(path);
  path = String();

  String output = "{[";
  if (root.isDirectory())
  {
    File file = root.openNextFile();
    while (file)
    {
      if (output != "[")
      {
        output += ',';
      }
      output += "{\"type\":\"";
      output += (file.isDirectory()) ? "dir" : "file";
      output += "\",\"name\":\"";
      output += String(file.name()).substring(1);
      output += "\"}";
      file = root.openNextFile();
    }
  }
  output += "]}";
  mserver.send(NORMALWEBPAGE, String(JSONTEXTPAGETYPE), output);
#endif // esp32
}

// builds /msnotfound page
void MANAGEMENT_buildnotfound(void)
{
  // Filesystem was started earlier when server was started so assume it has started
  if ( SPIFFS.exists("/msnotfound.html"))               // load page from fs - wsnotfound.html
  {
    // open file for read
    File file = SPIFFS.open("/msnotfound.html", "r");
    // read contents into string
    MSpg = file.readString();
    file.close();

    // process for dynamic data
    // Web page colors
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
    MSpg.replace("%VER%", String(programVersion));
    MSpg.replace("%NAM%", mySetupData->get_brdname());
    // add code to handle reboot controller
    MSpg.replace("%BT%", String(CREBOOTSTR));
    MSpg.replace("%HEA%", String(ESP.getFreeHeap()));
  }
  else
  {
    TRACE();
    MSrvr_DebugPrintln("file not found");
    MSpg = "file not found";
  }
  delay(10);                                            // small pause so background tasks can run
}

// handler when page or url not found
void MANAGEMENT_handlenotfound(void)
{
  MANAGEMENT_checkreboot();                             // if reboot controller;
  MANAGEMENT_buildnotfound();
  mserver.send(NOTFOUNDWEBPAGE, TEXTPAGETYPE, MSpg);
  MSpg = "";
}

// builds msupload page
void MANAGEMENT_buildupload(void)
{
  // Filesystem was started earlier when server was started so assume it has started
  if ( SPIFFS.exists("/msupload.html"))                 // load page from fs - wsupload.html
  {
    File file = SPIFFS.open("/msupload.html", "r");     // open file for read
    MSpg = file.readString();                           // read contents into string
    file.close();
    // process for dynamic data
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
    MSpg.replace("%VER%", String(programVersion));
    MSpg.replace("%NAM%", mySetupData->get_brdname());
    // add code to handle reboot controller
    MSpg.replace("%BT%", String(CREBOOTSTR));
  }
  else
  {
    TRACE();
    MSrvr_DebugPrintln("file not found");
    MSpg = "file not found";
  }
  delay(10);                                            // small pause so background tasks can run
}

// handles file upload selection by user
void MANAGEMENT_handlefileupload(void)
{
  HTTPUpload& upload = mserver.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    String filename = upload.filename;
    if (!filename.startsWith("/"))
    {
      filename = "/" + filename;
    }
    MSrvr_DebugPrint("handleFileUpload Name: ");
    MSrvr_DebugPrintln(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (fsUploadFile)
    {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (fsUploadFile)
    {
      // If the file was successfully created
      fsUploadFile.close();
      MSrvr_DebugPrint("handleFileUpload Size: ");
      MSrvr_DebugPrintln(upload.totalSize);
      mserver.sendHeader("Location", "/mssuccess");
      mserver.send(301);
    }
    else
    {
      mserver.send(INTERNALSERVERERROR, String(PLAINTEXTPAGETYPE), "Err: create file");
    }
  }
}

// handles a file upload success
void MANAGEMENT_fileuploadsuccess(void)
{
  // mssuccess.html
  if ( SPIFFS.exists("/mssuccess.html"))                // constructs mssuccess page of management server
  {
    File file = SPIFFS.open("/mssuccess.html", "r");    // open file for read
    MSpg = file.readString();                           // read contents into string
    file.close();

    // process for dynamic data
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
    MSpg.replace("%VER%", String(programVersion));
    MSpg.replace("%NAM%", mySetupData->get_brdname());

    MSpg.replace("%BT%", String(CREBOOTSTR));           // add code to handle reboot controller

    // display heap memory for tracking memory loss?
    // only esp32?
    MSpg.replace("%HEA%", String(ESP.getFreeHeap()));
  }
  else
  {
    // could not read file
    TRACE();
    MSrvr_DebugPrintln("file not found");
    MSpg = "file not found";
  }
  MANAGEMENT_sendmyheader();
  MANAGEMENT_sendmycontent();
  MSpg = "";
}

// show msupload - file upload page
void MANAGEMENT_fileupload(void)
{
  MANAGEMENT_buildupload();
  mserver.send(NORMALWEBPAGE, TEXTPAGETYPE, MSpg);
  MSpg = "";
}

// builds msindex5 - admin page 5 - colorpicker
void MANAGEMENT_buildadminpg5(void)
{
#ifdef TIMEMSBUILDPG5
  Serial.print("ms_buildpg5: ");
  Serial.println(millis());
#endif
  if ( SPIFFS.exists("/msindex5.html"))                 // constructs admin page 5 of management server
  {
    File file = SPIFFS.open("/msindex5.html", "r");     // open file for read
    MSpg = file.readString();                           // read contents into string
    file.close();

    // process for dynamic data
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
    MSpg.replace("%VER%", String(programVersion));
    MSpg.replace("%NAM%", mySetupData->get_brdname());

    // Display image for color picker

    MSpg.replace("%BT%", String(CREBOOTSTR));           // add code to handle reboot controller

    // display heap memory for tracking memory loss?
    // only esp32?
    MSpg.replace("%HEA%", String(ESP.getFreeHeap()));
  }
  else
  {
    // could not read file
    TRACE();
    MSrvr_DebugPrintln("file not found");
    MSpg = "file not found";
  }
#ifdef TIMEMSBUILDPG5
  Serial.print("ms_buildpg5: ");
  Serial.println(millis());
#endif
  delay(10);                                            // small pause so background tasks can run
}

// handler for msindex5 - color picker
void MANAGEMENT_handleadminpg5(void)
{
#ifdef TIMEMSHANDLEPG5
  Serial.print("ms_handlepg5: ");
  Serial.println(millis());
#endif
  MANAGEMENT_checkreboot();                             // if reboot controller;

  // there are no parameters to check

  MANAGEMENT_sendadminpg5();
#ifdef TIMEMSHANDLEPG5
  Serial.print("ms_handlepg5: ");
  Serial.println(millis());
#endif
}

// builds msindex4 - admin page 4 - web page colors
void MANAGEMENT_buildadminpg4(void)
{
#ifdef TIMEMSBUILDPG4
  Serial.print("ms_buildpg4: ");
  Serial.println(millis());
#endif
  if ( SPIFFS.exists("/msindex4.html"))                 // constructs admin page 4 of management server
  {
    File file = SPIFFS.open("/msindex4.html", "r");     // open file for read
    MSpg = file.readString();                           // read contents into string
    file.close();
    // process for dynamic data
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
    MSpg.replace("%VER%", String(programVersion));
    MSpg.replace("%NAM%", mySetupData->get_brdname());

    // Background color %BC% default #333333
    // Main Page Title Color %MP% default #8e44ad
    // Header Group Color %HC% default #3399ff
    // Text Color %TC% default #5d6d7e
    String bcolor = "<form action=\"/msindex4\" method=\"post\"><input type=\"text\" name=\"bc\" size=\"6\" value=\"" + String(mySetupData->get_wp_backcolor()) + "\"><input type=\"submit\" name=\"bc\" value=\"SET\"></form>";
    MSpg.replace("%BC%", String(bcolor));
    String ticolor = "<form action=\"/msindex4\" method=\"post\"><input type=\"text\" name=\"ti\" size=\"6\" value=\"" + String(mySetupData->get_wp_titlecolor()) + "\"><input type=\"submit\" name=\"ti\" value=\"SET\"></form>";
    MSpg.replace("%TI%", String(ticolor));
    String hcolor = "<form action=\"/msindex4\" method=\"post\"><input type=\"text\" name=\"hc\" size=\"6\" value=\"" + String(mySetupData->get_wp_headercolor()) + "\"><input type=\"submit\" name=\"hc\" value=\"SET\"></form>";
    MSpg.replace("%HC%", String(hcolor));
    String tcolor = "<form action=\"/msindex4\" method=\"post\"><input type=\"text\" name=\"tc\" size=\"6\" value=\"" + String(mySetupData->get_wp_textcolor()) + "\"><input type=\"submit\" name=\"tc\" value=\"SET\"></form>";
    MSpg.replace("%TC%", String(tcolor));

    MSpg.replace("%BT%", String(CREBOOTSTR));           // add code to handle reboot controller

    // display heap memory for tracking memory loss?
    // only esp32?
    MSpg.replace("%HEA%", String(ESP.getFreeHeap()));
  }
  else
  {
    // could not read index file from SPIFFS
    TRACE();
    MSrvr_DebugPrintln("file not found");
    MSpg = "file not found";
  }
#ifdef TIMEMSBUILDPG4
  Serial.print("ms_buildpg4: ");
  Serial.println(millis());
#endif
  delay(10);                                            // small pause so background tasks can run
}

// handler for msindex4 - admin page 4 - web page colors
void MANAGEMENT_handleadminpg4(void)
{
#ifdef TIMEMSHANDLEPG4
  Serial.print("ms_handlepg4: ");
  Serial.println(millis());
#endif
  // code here to handle a put request
  String msg;

  MANAGEMENT_checkreboot();                             // if reboot controller;

  // Handle update of webpage colors
  // Main Page Title Color <span id="TI">%MP%</span></p> #8e44ad
  // <p>Background Color <span id="BC">%BC%</span></p> #333333
  // <p>Header Group Color ><span id="HC">%HC%</span></p> #3399ff
  // <p>Text Color ><span id="TC">%TC%</span></p> #5d6d7e
  // get background color
  msg = mserver.arg("bc");
  if ( msg != "" )
  {
    boolean flag = true;
    String str = msg;
    int len = str.length();
    for ( int i = 0; i < len; i++ )
    {
      char ch = str[i];
      if ( ishexdigit(ch) == false )
      {
        flag = false;
      }
    }
    if ( flag == false )
    {
      MSrvr_DebugPrintln(BACKCOLORINVALIDSTR);
    }
    else
    {
      mySetupData->set_wp_backcolor(str);               // set the new background color
    }
  }

  // get text color
  msg = mserver.arg("tc");
  if ( msg != "" )
  {
    boolean flag = true;
    String str = msg;
    int len = str.length();
    for ( int i = 0; i < len; i++ )
    {
      char ch = str[i];
      if ( ishexdigit(ch) == false )
      {
        flag = false;
      }
    }
    if ( flag == false )
    {
      MSrvr_DebugPrintln(TEXTCOLORINVALIDSTR);
    }
    else
    {
      MSrvr_DebugPrint(NEWTEXTCOLORSTR);
      MSrvr_DebugPrintln(str);
      mySetupData->set_wp_textcolor(str);               // set the new text color
    }
  }

  // get header color
  msg = mserver.arg("hc");
  if ( msg != "" )
  {
    boolean flag = true;
    String str = msg;
    int len = str.length();
    for ( int i = 0; i < len; i++ )
    {
      char ch = str[i];
      if ( ishexdigit(ch) == false )
      {
        flag = false;
      }
    }
    if ( flag == false )
    {
      MSrvr_DebugPrintln(HEADERCOLORINVALIDSTR);
    }
    else
    {
      MSrvr_DebugPrint(NEWHEADERCOLORSTR);
      MSrvr_DebugPrintln(str);
      mySetupData->set_wp_headercolor(str);             // set the new header color
    }
  }

  // get page title color
  msg = mserver.arg("ti");
  if ( msg != "" )
  {
    boolean flag = true;
    String str = msg;
    int len = str.length();
    for ( int i = 0; i < len; i++ )
    {
      char ch = str[i];
      if ( ishexdigit(ch) == false )
      {
        flag = false;
      }
    }
    if ( flag == false )
    {
      MSrvr_DebugPrintln(TITLECOLORINVALIDSTR);
    }
    else
    {
      MSrvr_DebugPrint(NEWTITLECOLORSTR);
      MSrvr_DebugPrintln(str);
      mySetupData->set_wp_titlecolor(str);              // set the new header color
    }
  }
  MANAGEMENT_sendadminpg4();
#ifdef TIMEMSHANDLEPG4
  Serial.print("ms_handlepg4: ");
  Serial.println(millis());
#endif
}

// builder for msindex3 - admin page 3 - backlash + motor-speed-delay + push buttons
void MANAGEMENT_buildadminpg3(void)
{
#ifdef TIMEMSBUILDPG3
  Serial.print("ms_buildpg3: ");
  Serial.println(millis());
#endif
  // Filesystem was started earlier when server was started so assume it has started
  if ( SPIFFS.exists("/msindex3.html"))                 // constructs admin page 3 of management server
  {
    File file = SPIFFS.open("/msindex3.html", "r");     // open file for read
    MSpg = file.readString();                           // read contents into string
    file.close();

    // process for dynamic data
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
    MSpg.replace("%VER%", String(programVersion));
    MSpg.replace("%NAM%", mySetupData->get_brdname());

    if ( mySetupData->get_backlash_in_enabled() )
    {
      MSpg.replace("%BIE%", String(DISABLEBKINSTR));
      MSpg.replace("%STI%", "Enabled");
    }
    else
    {
      MSpg.replace("%BIE%", String(ENABLEBKINSTR));
      MSpg.replace("%STI%", "Disabled");
    }
    if ( mySetupData->get_backlash_out_enabled() )
    {
      MSpg.replace("%BOE%", String(DISABLEBKOUTSTR));
      MSpg.replace("%STO%", "Enabled");
    }
    else
    {
      MSpg.replace("%BOE%", String(ENABLEBKOUTSTR));
      MSpg.replace("%STO%", "Disabled");
    }

    MSpg.replace("%BIS%",  String(BLINSTEPSTR));
    MSpg.replace("%BOS%",  String(BLOUTSTEPSTR));
    MSpg.replace("%bins%", String(mySetupData->get_backlashsteps_in()));
    MSpg.replace("%bous%", String(mySetupData->get_backlashsteps_out()));

    // motor speed delay
    MSpg.replace("%MS%", "<form action=\"/msindex3\" method=\"post\">Delay: <input type=\"text\" name=\"msd\" size=\"6\" value=" + String(mySetupData->get_brdmsdelay()) + "> <input type=\"submit\" name=\"setmsd\" value=\"Set\"></form>");

    MSpg.replace("%BT%", String(CREBOOTSTR));           // add code to handle reboot controller

    // PUSHBUTTONS
    if ( mySetupData->get_pbenable() == 1)
    {
      MSpg.replace("%PBN%", String(DISABLEPBSTR));      // button
      MSpg.replace("%PBL%", "Enabled");                 // state
    }
    else
    {
      MSpg.replace("%PBN%", String(ENABLEPBSTR));
      MSpg.replace("%PBL%", "Disabled");
    }

    // INDI
    if ( mySetupData->get_indi() == 1)
    {
      MSpg.replace("%INDI%", String(DISABLEINDISTR));   // button
      MSpg.replace("%INI%", "Enabled");                 // state
    }
    else
    {
      MSpg.replace("%INDI%", String(ENABLEINDISTR));
      MSpg.replace("%INI%", "Disabled");
    }

    // display heap memory for tracking memory loss?
    // only esp32?
    MSpg.replace("%HEA%", String(ESP.getFreeHeap()));
  }
  else
  {
    // could not read file
    TRACE();
    MSrvr_DebugPrintln("file not found");
    MSpg = "file not found";
  }
#ifdef TIMEMSBUILDPG3
  Serial.print("ms_buildpg3: ");
  Serial.println(millis());
#endif
  delay(10);                                            // small pause so background tasks can run
}

// handler for msindex3 - admin page 3 - backlash + motor-speed-delay + push buttons
void MANAGEMENT_handleadminpg3(void)
{
#ifdef TIMEMSHANDLEPG3
  Serial.print("ms_handlepg3: ");
  Serial.println(millis());
#endif
  // code here to handle a put request
  String msg;

  MANAGEMENT_checkreboot();                             // if reboot controller;

  // handle backlash
  // backlash in enable/disable, enin, diin
  msg = mserver.arg("enin");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("adminpg3: enin: ");
    mySetupData->set_backlash_in_enabled(1);
  }
  msg = mserver.arg("diin");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("adminpg3: diin: ");
    mySetupData->set_backlash_in_enabled(0);
  }

  // backlash out enable/disable, enou, diou
  msg = mserver.arg("enou");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("adminpg3: enou: ");
    mySetupData->set_backlash_out_enabled(1);
  }
  msg = mserver.arg("diou");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("adminpg3: diou: ");
    mySetupData->set_backlash_out_enabled(0);
  }

  // backlash in steps, setbis, bis,
  msg = mserver.arg("setbis");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("adminpg3: setbis: ");
    String st = mserver.arg("bis");
    MSrvr_DebugPrint("adminpg3: bis: ");
    MSrvr_DebugPrintln(st);
    byte steps = st.toInt();                            // no need to test for <0 and > 255 as it is a byte value
    mySetupData->set_backlashsteps_in(steps);
  }

  // backlash out steps, setbos, bos
  msg = mserver.arg("setbos");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("adminpg3: setbos: ");
    String st = mserver.arg("bos");
    MSrvr_DebugPrint("adminpg3: bos: ");
    MSrvr_DebugPrintln(st);
    byte steps = st.toInt();
    mySetupData->set_backlashsteps_out(steps);
  }

  // motor speed delay
  msg = mserver.arg("setmsd");
  if ( msg != "" )
  {
    MSrvr_DebugPrint("set motor speed delay: ");
    MSrvr_DebugPrintln(msg);
    String ms = mserver.arg("msd");                      // process new motor speed delay
    if ( ms != "" )
    {
      unsigned long newdelay = 4000;
      newdelay = ms.toInt();
      newdelay = (newdelay < 1000) ? 1000 : newdelay;   // ensure it is not too low
      mySetupData->set_brdmsdelay(newdelay);
    }
  }

  // push buttons enable/disable
  msg = mserver.arg("pbon");
  if ( msg != "" )
  {
    if ( (mySetupData->get_brdpb1pin() == -1) && (mySetupData->get_brdpb1pin() == -1) )
    {
      MSrvr_DebugPrintln("push button pins are -1. Cannot enable pins");
      mySetupData->set_pbenable(0);
    }
    else
    {
      // if current state is disabled then enable
      if ( mySetupData->get_pbenable() == 0)
      {
        mySetupData->set_pbenable(1);
        init_pushbuttons();
      }
    }
  }
  msg = mserver.arg("pboff");
  if ( msg != "" )
  {
    mySetupData->set_pbenable(0);
  }

  // indi enable/disable
  msg = mserver.arg("indion");
  if ( msg != "" )
  {
    mySetupData->set_indi(1);
  }
  msg = mserver.arg("indioff");
  if ( msg != "" )
  {
    mySetupData->set_indi(0);
  }

  MANAGEMENT_sendadminpg3();
#ifdef TIMEMSHANDLEPG3
  Serial.print("ms_handlepg3: ");
  Serial.println(millis());
#endif
}

// builder for msindex2 - admin page 2 - servers + temp-probe + leds + hpsw
void MANAGEMENT_buildadminpg2(void)
{
#ifdef TIMEMSBUILDPG2
  Serial.print("ms_buildpg2: ");
  Serial.println(millis());
#endif
  // Filesystem was started earlier when server was started so assume it has started
  MSrvr_DebugPrintln("management: FS mounted");               // constructs admin page 2 of management server
  if ( SPIFFS.exists("/msindex2.html"))
  {
    File file = SPIFFS.open("/msindex2.html", "r");     // open file for read
    MSpg = file.readString();                           // read contents into string
    file.close();

    // process for dynamic data
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
    MSpg.replace("%VER%", String(programVersion));
    MSpg.replace("%NAM%", mySetupData->get_brdname());

    // tcp/ip server
    if ( tcpipserverstate == RUNNING )
    {
      MSpg.replace("%TBT%", String(STOPTSSTR));
      MSpg.replace("%TST%", "Running");
    }
    else
    {
      MSpg.replace("%TBT%", String(STARTTSSTR));
      MSpg.replace("%TST%", "Stopped");
    }
#if defined(ESP8266)
    // esp8266 cannot change port of server
    String portstr = "Port: " + String(mySetupData->get_tcpipport());
    MSpg.replace("%TPO%", portstr );
#else
    // esp32
    MSpg.replace("%TPO%", "<form action=\"/msindex2\" method =\"post\">Port: <input type=\"text\" name=\"tp\" size=\"6\" value=" + String(mySetupData->get_tcpipport()) + "> <input type=\"submit\" name=\"settsport\" value=\"Set\"></form>");
#endif // #if defined(ESP8266)
    MSpg.replace("%TPO%", "Port: " + String(mySetupData->get_tcpipport()));
    MSpg.replace("%TBT%", String(NOTDEFINEDSTR));

    // Webserver status %WST%
    if ( webserverstate == RUNNING )
    {
      MSpg.replace("%WBT%", String(STOPWSSTR));
      MSpg.replace("%WST%", "Running");
    }
    else
    {
      MSpg.replace("%WBT%", String(STARTWSSTR));
      MSpg.replace("%WST%", "Stopped");
    }
    // Webserver Port number %WPO%, %WBT%refresh Rate %WRA%
    MSpg.replace("%WPO%", "<form action=\"/msindex2\" method =\"post\">Port: <input type=\"text\" name=\"wp\" size=\"6\" value=" + String(mySetupData->get_webserverport()) + "> <input type=\"submit\" name=\"setwsport\" value=\"Set\"></form>");
    MSpg.replace("%WRA%", "<form action=\"/msindex2\" method =\"post\">Refresh Rate: <input type=\"text\" name=\"wr\" size=\"6\" value=" + String(mySetupData->get_webpagerefreshrate()) + "> <input type=\"submit\" name=\"setwsrate\" value=\"Set\"></form>");

    // ascom server start/stop service, Status %AST%, Port %APO%, Button %ABT%
    if ( ascomserverstate == RUNNING )
    {
      MSpg.replace("%AST%", String(STOPASSTR));
      MSpg.replace("%ABT%", "Running");
    }
    else
    {
      MSpg.replace("%AST%", String(STARTASSTR));
      MSpg.replace("%ABT%", "Stopped");
    }
    MSpg.replace("%APO%", "<form action=\"/msindex2\" method =\"post\">Port: <input type=\"text\" name=\"ap\" size=\"8\" value=" + String(mySetupData->get_ascomalpacaport()) + "> <input type=\"submit\" name=\"setasport\" value=\"Set\"></form>");

    // TEMPERATURE PROBE ENABLE/DISABLE, State %TPE%, Button %TPO%
    if ( mySetupData->get_temperatureprobestate() == 1 )
    {
      MSpg.replace("%TPP%", String(DISABLETEMPSTR));    // button
      MSpg.replace("%TPE%", "Enabled");                 // state
    }
    else
    {
      MSpg.replace("%TPP%", String(ENABLETEMPSTR));
      MSpg.replace("%TPE%", "Disabled");
    }
    // Temperature Mode %TEM%
    // Celcius=1, Fahrenheit=0
    if ( mySetupData->get_tempmode() == 1 )
    {
      // celsius - Change to Fahrenheit
      MSpg.replace("%TEM%", "Fahrenheit");
    }
    else
    {
      // Fahrenheit - change to celsius
      MSpg.replace("%TEM%", "Celsius");
    }

    // INOUT LEDS ENABLE/DISABLE, State %INL%, Button %INO%
    if ( mySetupData->get_inoutledstate() == 1)
    {
      MSpg.replace("%INO%", String(DISABLELEDSTR));     // button
      MSpg.replace("%INL%", "Enabled");                 // state
    }
    else
    {
      MSpg.replace("%INO%", String(ENABLELEDSTR));
      MSpg.replace("%INL%", "Disabled");
    }

    if ( mySetupData->get_hpswitchenable() == 1)
    {
      MSpg.replace("%HPO%", String(DISABLEHPSWSTR));   // button
      MSpg.replace("%HPL%", "Enabled");                 // state
    }
    else
    {
      MSpg.replace("%HPO%", String(ENABLEHPSWSTR));
      MSpg.replace("%HPL%", "Disabled");
    }

    // add code to handle reboot controller %BT%
    MSpg.replace("%BT%", String(CREBOOTSTR));

    // display heap memory for tracking memory loss, %HEA%
    // only esp32?
    MSpg.replace("%HEA%", String(ESP.getFreeHeap()));
  }
  else
  {
    // could not read file
    TRACE();
    MSrvr_DebugPrintln("file not found");
    MSpg = "file not found";
  }
#ifdef TIMEMSBUILDPG2
  Serial.print("ms_buildpg2: ");
  Serial.println(millis());
#endif
}

// handler for msindex2 - admin page 2 - servers + temp-probe + leds + hpsw
void MANAGEMENT_handleadminpg2(void)
{
#ifdef TIMEMSHANDLEPG2
  Serial.print("ms_handlepg2: ");
  Serial.println(millis());
#endif
  // code here to handle a put request
  String msg;

  MANAGEMENT_checkreboot();                             // if reboot controller;

  // TCP/IP server START STOP
  msg = mserver.arg("startts");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("adminpg2: startts: ");
    start_tcpipserver();
  }
  msg = mserver.arg("stopts");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("adminpg2: stopts: ");
    stop_tcpipserver();
  }
  // tcpip server change port
  msg = mserver.arg("settsport");
  if ( msg != "" )
  {
    MSrvr_DebugPrint("set tcpip server port: ");
    MSrvr_DebugPrintln(msg);
    String tp = mserver.arg("tp");                      // process new webserver port number
    if ( tp != "" )
    {
      unsigned long newport = 0;
      MSrvr_DebugPrint("tp:");
      MSrvr_DebugPrintln(tp);
      newport = tp.toInt();
      if ( tcpipserverstate == STOPPED )
      {
        unsigned long currentport = mySetupData->get_tcpipport();
        if ( newport == currentport)
        {
          // port is the same so do not bother to change it
          MSrvr_DebugPrintln("tp err: new Port = current port");
        }
        else
        {
          if ( newport == MSSERVERPORT )                              // if same as management server
          {
            MSrvr_DebugPrintln("wp err: new Port = MSSERVERPORT");
          }
          else if ( newport == mySetupData->get_ascomalpacaport() )   // if same as ASCOM REMOTE server
          {
            MSrvr_DebugPrintln("wp err: new Port = ALPACAPORT");
          }
          else if ( newport == mySetupData->get_mdnsport() )          // if same as mDNS server
          {
            MSrvr_DebugPrintln("wp err: new Port = MDNSSERVERPORT");
          }
          else if ( newport == mySetupData->get_webserverport() )     // if same as mDNS server
          {
            MSrvr_DebugPrintln("wp err: new Port = WEBSERVERPORT");
          }
          else
          {
            MSrvr_DebugPrintln("New tcpipserver port = " + String(newport));
            mySetupData->set_tcpipport(newport);                      // assign new port and save it
          }
        }
      }
      else
      {
        MSrvr_DebugPrintln("Attempt to change tcpipserver port when tcpipserver running");
      }
    }
  }

  // webserver START STOP service
  msg = mserver.arg("startws");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("adminpg2: startws: ");
    if ( webserverstate == STOPPED)
    {
      start_webserver();
      mySetupData->set_webserverstate(1);
    }
  }
  msg = mserver.arg("stopws");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("adminpg2: stopws: ");
    if ( webserverstate == RUNNING )
    {
      stop_webserver();
      mySetupData->set_webserverstate(0);
    }
  }
  // webserver change port - we should be able to change port if not running or enabled or not enabled
  msg = mserver.arg("setwsport");
  if ( msg != "" )
  {
    MSrvr_DebugPrint("set web server port: ");
    MSrvr_DebugPrintln(msg);
    String wp = mserver.arg("wp");                                    // process new webserver port number
    if ( wp != "" )
    {
      unsigned long newport = 0;
      MSrvr_DebugPrint("wp:");
      MSrvr_DebugPrintln(wp);
      newport = wp.toInt();
      if ( webserverstate == STOPPED )
      {
        unsigned long currentport = mySetupData->get_webserverport();
        if ( newport == currentport)
        {
          MSrvr_DebugPrintln("wp err: new Port = current port");            // port is the same so do not bother to change it
        }
        else
        {
          if ( newport == MSSERVERPORT )                              // if same as management server
          {
            MSrvr_DebugPrintln("wp err: new Port = MSSERVERPORT");
          }
          else if ( newport == mySetupData->get_ascomalpacaport() )   // if same as ASCOM REMOTE server
          {
            MSrvr_DebugPrintln("wp err: new Port = ALPACAPORT");
          }
          else if ( newport == mySetupData->get_mdnsport() )          // if same as mDNS server
          {
            MSrvr_DebugPrintln("wp err: new Port = MDNSSERVERPORT");
          }
          else if ( newport == mySetupData->get_tcpipport() )         // if same as tcpip server
          {
            MSrvr_DebugPrintln("wp err: new Port = SERVERPORT");
          }
          else
          {
            MSrvr_DebugPrintln("New webserver port = " + String(newport));
            mySetupData->set_webserverport(newport);                  // assign new port and save it
          }
        }
      }
      else
      {
        MSrvr_DebugPrintln("Attempt to change webserver port when webserver running");
      }
    }
  }
  // web page refresh rate - should be able to change at any time
  msg = mserver.arg("setwsrate");
  if ( msg != "" )
  {
    MSrvr_DebugPrint("set web server page refresh rate: ");
    MSrvr_DebugPrintln(msg);
    String wr = mserver.arg("wr");                      // process new webserver page refresh rate
    if ( wr != "" )
    {
      int newrate = 0;
      MSrvr_DebugPrint("wr:");
      MSrvr_DebugPrintln(wr);
      newrate = wr.toInt();
      int currentrate = mySetupData->get_webpagerefreshrate();
      if ( newrate == currentrate)
      {
        // port is the same so do not bother to change it
        MSrvr_DebugPrintln("wr err: new page refresh rate = current page refresh rate");
      }
      else
      {
        if ( newrate < MINREFRESHPAGERATE )
        {
          MSrvr_DebugPrintln("wr err: Page refresh rate too low");
          newrate = MINREFRESHPAGERATE;
        }
        else if ( newrate > MAXREFRESHPAGERATE )
        {
          MSrvr_DebugPrintln("wr err: Page refresh rate too high");
          newrate = MAXREFRESHPAGERATE;
        }
        MSrvr_DebugPrintln("New page refresh rate = " + String(newrate));
        mySetupData->set_webpagerefreshrate(newrate);                // assign new refresh rate and save it
      }
    }
  }

  // ascomserver start/stop service
  msg = mserver.arg("startas");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("adminpg2: startas: ");
    if ( ascomserverstate == STOPPED )
    {
      start_ascomremoteserver();
      mySetupData->set_ascomserverstate(1);
    }
  }
  msg = mserver.arg("stopas");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("adminpg2: stopas: ");
    if ( ascomserverstate == RUNNING)
    {
      stop_ascomremoteserver();
      mySetupData->set_ascomserverstate(0);
    }
  }
  // ascom server port
  msg = mserver.arg("setasport");
  if ( msg != "" )
  {
    MSrvr_DebugPrint("set ascom server port: ");
    MSrvr_DebugPrintln(msg);
    String ap = mserver.arg("ap");                                    // process new ascomalpaca port number
    if ( ap != "" )
    {
      unsigned long newport = 0;
      MSrvr_DebugPrint("ap:");
      MSrvr_DebugPrintln(ap);
      newport = ap.toInt();
      if ( ascomserverstate == STOPPED )
      {
        unsigned long currentport = mySetupData->get_ascomalpacaport();
        if ( newport == currentport)
        {
          // port is the same so do not bother to change it
          MSrvr_DebugPrintln("ap error: new Port = current port");
        }
        else
        {
          if ( newport == MSSERVERPORT )                              // if same as management server
          {
            MSrvr_DebugPrintln("wp err: new Port = MSSERVERPORT");
          }
          else if ( newport == mySetupData->get_webserverport() )     // if same as webserver
          {
            MSrvr_DebugPrintln("wp err: new Port = ALPACAPORT");
          }
          else if ( newport == mySetupData->get_mdnsport() )          // if same as mDNS server
          {
            MSrvr_DebugPrintln("wp err: new Port = MDNSSERVERPORT");
          }
          else if ( newport == mySetupData->get_tcpipport() )          // if same as tcpip server
          {
            MSrvr_DebugPrintln("wp err: new Port = SERVERPORT");
          }
          else
          {
            MSrvr_DebugPrintln("New ascomalpaca port = " + String(newport));
            mySetupData->set_ascomalpacaport(newport);                  // assign new port and save it
          }
        }
      }
      else
      {
        MSrvr_DebugPrintln("Attempt to change ascomalpaca port when ascomserver running");
      }
    }
  }

  // Temperature Probe ENABLE/DISABLE, starttp, stoptp
  msg = mserver.arg("starttp");
  if ( msg != "" )
  {
    if ( mySetupData->get_brdtemppin() == -1)
    {
      MSrvr_DebugPrintln("temp pin is -1. Cannot enable pin");
      mySetupData->set_temperatureprobestate(0);
      tprobe1 = 0;
    }
    else
    {
      // check if already enabled
      if (mySetupData->get_temperatureprobestate() == 1)                // if probe is enabled
      {
        if ( tprobe1 == 0 )                                             // if probe not found
        {
          myTempProbe = new TempProbe;                                  // attempt to start probe and search for probe
        }
      }
      else
      {
        mySetupData->set_temperatureprobestate(1);                      // enable probe
        if ( tprobe1 == 0 )                                             // if probe not found
        {
          myTempProbe = new TempProbe;                                  // attempt to start probe and search for probe
        }
      }
    }
  }
  msg = mserver.arg("stoptp");
  if ( msg != "" )
  {
    if ( mySetupData->get_brdtemppin() == -1)
    {
      MSrvr_DebugPrintln("temp pin is -1. Cannot enable pin");
      mySetupData->set_temperatureprobestate(0);
      tprobe1 = 0;
    }
    else
    {
      if ( mySetupData->get_temperatureprobestate() == 1 )
      {
        // there is no destructor call
        tprobe1 = 0;
        mySetupData->set_temperatureprobestate(0);
      }
      else
      {
        // do nothing, already disabled
        tprobe1 = 0;
      }
    }
  }

  // Temperature probe celsius/farentheit
  msg = mserver.arg("tm");
  if ( msg != "" )
  {
    MSrvr_DebugPrint("Set temp mode: ");
    MSrvr_DebugPrintln(msg);
    if ( msg == "cel" )
    {
      mySetupData->set_tempmode(1);
    }
    else if ( msg == "fah" )
    {
      mySetupData->set_tempmode(0);
    }
  }

  // LEDS ENABLE/DISABLE startle, stople
  msg = mserver.arg("startle");
  if ( msg != "" )
  {
    if ( (mySetupData->get_brdinledpin() == -1) || (mySetupData->get_brdinledpin() == -1) )
    {
      MSrvr_DebugPrintln("led pins are -1. Cannot enable pins");
      mySetupData->set_inoutledstate(0);
    }
    else
    {
      // if disabled then enable
      if ( mySetupData->get_inoutledstate() == 0)
      {
        mySetupData->set_inoutledstate(1);
        // reinitialise pins
        String drvbrd = mySetupData->get_brdname();
        if (drvbrd.equals("PRO2ESP32ULN2003") || drvbrd.equals("PRO2ESP32L298N") || drvbrd.equals("PRO2ESP32L293DMINI") || drvbrd.equals("PRO2ESP32L9110S") || drvbrd.equals("PRO2ESP32DRV8825") )
        {
          init_leds();
        }
      }
    }
  }
  msg = mserver.arg("stople");
  if ( msg != "" )
  {
    // if enabled then disable
    if ( mySetupData->get_inoutledstate() == 1)
    {
      mySetupData->set_inoutledstate(0);
    }
  }

  // HOME POSITION SWITCH, enable, disable
  msg = mserver.arg("hpswon");
  if ( msg != "" )
  {
    if ( mySetupData->get_brdhpswpin() == -1 )
    {
      MSrvr_DebugPrintln("hpsw pin is -1. Cannot enable pin");
      mySetupData->set_hpswitchenable(0);
    }
    else
    {
      // if current state is disabled then enable
      if ( mySetupData->get_hpswitchenable() == 0)
      {
        mySetupData->set_hpswitchenable(1);
        init_homepositionswitch();
      }
    }
  }
  msg = mserver.arg("hpswoff");
  if ( msg != "" )
  {
    mySetupData->set_hpswitchenable(0);
  }

  MANAGEMENT_sendadminpg2();
#ifdef TIMEMSHANDLEPG2
  Serial.print("ms_handlepg2: ");
  Serial.println(millis());
#endif
}

// builder for msindex1 - admin page 1
void MANAGEMENT_buildadminpg1(void)
{
#ifdef TIMEMSBUILDPG1
  Serial.print("ms_buildpg1: ");
  Serial.println(millis());
#endif
  // Filesystem was started earlier when server was started so assume it has started
  if ( SPIFFS.exists("/msindex1.html"))                 // constructs home page of management server
  {
    File file = SPIFFS.open("/msindex1.html", "r");     // open file for read
    MSpg = file.readString();                           // read contents into string
    file.close();

    // process for dynamic data
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
    MSpg.replace("%VER%", String(programVersion));
    MSpg.replace("%NAM%", mySetupData->get_brdname());
#if (CONTROLLERMODE == BLUETOOTHMODE)
    MSpg.replace("%MOD%", "BLUETOOTH : " + String(BLUETOOTHNAME));
#elif (CONTROLLERMODE == ACCESSPOINT)
    MSpg.replace("%MOD%", "ACCESSPOINT");
#elif (CONTROLLERMODE == STATIONMODE)
    MSpg.replace("%MOD%", "STATIONMODE");
#elif (CONTROLLERMODE == LOCALSERIAL)
    MSpg.replace("%MOD%", "LOCALSERIAL");
#endif

#ifdef MDNSSERVER
    if ( mdnsserverstate == RUNNING)
    {
      MSpg.replace("%MST%", "RUNNING");
    }
    else
    {
      MSpg.replace("%MST%", "STOPPED");
    }
    MSpg.replace("%MPO%", "<form action=\"/\" method =\"post\">Port: <input type=\"text\" name=\"mdnsp\" size=\"8\" value=" + String(mySetupData->get_mdnsport()) + "> <input type=\"submit\" name=\"setmdnsport\" value=\"Set\"></form>");
    if ( mdnsserverstate == RUNNING)
    {
      MSpg.replace("%MBT%", String(MDNSTOPSTR));
    }
    else
    {
      MSpg.replace("%MBT%", String(MDNSSTARTSTR));
    }
#else
    MSpg.replace("%MST%", String(NOTDEFINEDSTR));
    MSpg.replace("%MPO%", "Port: " + String(mySetupData->get_mdnsport()));
    MSpg.replace("%MBT%", " ");
#endif

#ifdef OTAUPDATES
    if ( otaupdatestate == RUNNING )
    {
      MSpg.replace("%OST%", "RUNNING");
    }
    else
    {
      MSpg.replace("%OST%", "STOPPED");
    }
#else
    MSpg.replace("%OST%", String(NOTDEFINEDSTR));
#endif

#ifdef USEDUCKDNS
    if ( duckdnsstate == RUNNING )
    {
      MSpg.replace("%DST%", "RUNNING");
    }
    else
    {
      MSpg.replace("%DST%", "STOPPED");
    }
#else
    MSpg.replace("%DST%", String(NOTDEFINEDSTR));
#endif

    // staticip %IPS%
    if ( staticip == STATICIPON )
    {
      MSpg.replace("%IPS%", "ON");
    }
    else
    {
      MSpg.replace("%IPS%", "OFF");
    }

    // display %OLE%
    MSrvr_DebugPrint("Display state: ");
    MSrvr_DebugPrintln(displaystate);
    if ( displaystate == true)
    {
      if ( mySetupData->get_displayenabled() == 1 )
      {
        MSpg.replace("%OLE%", String(DISPLAYONSTR));                // checked already
      }
      else
      {
        MSpg.replace("%OLE%", String(DISPLAYOFFSTR));               // not checked
      }
    }
    else
    {
      MSpg.replace("%OLE%", "<b>DISPLAY: </b>" + String(NOTDEFINEDSTR)); // not checked
    }

    // if oled display page group option update
    // %PG% is current page option, %PGO% is option binary string
    MSpg.replace("%PG%", mySetupData->get_oledpageoption() );
    String oled;
    oled = "<form action=\"/\" method=\"post\"><input type=\"text\" name=\"pg\" size=\"12\" value=" + String(mySetupData->get_oledpageoption()) + "> <input type=\"submit\" name=\"setpg\" value=\"Set\"></form>";
    MSpg.replace("%PGO%", oled );

    // page display time
    MSpg.replace("%PT%", String(mySetupData->get_lcdpagetime()) );
    oled = "<form action=\"/\" method=\"post\"><input type=\"text\" name=\"pt\" size=\"12\" value=" + String(mySetupData->get_lcdpagetime()) + "> <input type=\"submit\" name=\"setpt\" value=\"Set\"></form>";
    MSpg.replace("%PGT%", oled );

    // startscreen %SS%
    if ( mySetupData->get_showstartscreen() == 1 )
    {
      // checked already
      MSpg.replace("%SS%", String(STARTSCREENONSTR));
    }
    else
    {
      // not checked
      MSpg.replace("%SS%", String(STARTSCREENOFFSTR));
    }

    // FORCEMANAGEMENTDOWNLOAD %MDL%
    if ( mySetupData->get_forcedownload() == 1 )
    {
      // checked already
      MSpg.replace("%MDL%", String(STARTFMDLONSTR));
    }
    else
    {
      // not checked
      MSpg.replace("%MDL%", String(STARTFMDLOFFSTR));
    }

    // SHOWHPSWMESSAGES %HPM%
    if ( mySetupData->get_showhpswmsg() == 1 )
    {
      // checked already
      MSpg.replace("%HPM%", String(STARTHPSWMONSTR));
    }
    else
    {
      // not checked
      MSpg.replace("%HPM%", String(STARTHPSWMOFFSTR));
    }

    MSpg.replace("%BT%", String(CREBOOTSTR));           // add code to handle reboot controller

    // display heap memory for tracking memory loss?
    // only esp32?
    MSpg.replace("%HEA%", String(ESP.getFreeHeap()));
  }
  else
  {
    // could not read file
    TRACE();
    MSrvr_DebugPrintln("File not found");
    MSpg = "File not found";
  }
#ifdef TIMEMSBUILDPG1
  Serial.print("msbuildpg1: ");
  Serial.println(millis());
#endif
  delay(10);                                            // small pause so background tasks can run
}

// handler for msindex1 - admin page 1
void MANAGEMENT_handleadminpg1(void)
{
#ifdef TIMEMSHANDLEPG1
  Serial.print("mshandlepg1: ");
  Serial.println(millis());
#endif
  // code here to handle a put request
  String msg;

  MANAGEMENT_checkreboot();                              // if reboot controller;

  // mdns server
  msg = mserver.arg("startmdns");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("MANAGEMENT_handleadminpg1: startmdns: ");
#ifdef MDNSSERVER
    start_mdns_service();
#else
    MSrvr_DebugPrintln("Err: MSDNSSERVER not defined");
#endif
  }
  msg = mserver.arg("stopmdns");
  if ( msg != "" )
  {
    MSrvr_DebugPrintln("MANAGEMENT_handleadminpg1: stopmdns: ");
#ifdef MDNSSERVER
    stop_mdns_service();
#else
    MSrvr_DebugPrintln("Err: MSDNSSERVER not defined");
#endif
  }
  // mdns port
  msg = mserver.arg("setmdnsport");
  if ( msg != "" )
  {
    MSrvr_DebugPrint("set web server port: ");
    MSrvr_DebugPrintln(msg);
    String mp = mserver.arg("mdnsp");                                 // process new webserver port number
    if ( mp != "" )
    {
      unsigned long newport = 0;
      MSrvr_DebugPrint("mp:");
      MSrvr_DebugPrintln(mp);
      newport = mp.toInt();
      if ( mdnsserverstate == STOPPED )
      {
        unsigned long currentport = mySetupData->get_mdnsport();
        if ( newport == currentport)
        {
          // port is the same so do not bother to change it
          MSrvr_DebugPrintln("mp err: new Port = current port");
        }
        else
        {
          if ( newport == MSSERVERPORT )                              // if same as management server
          {
            MSrvr_DebugPrintln("mp err: new Port = MSSERVERPORT");
          }
          else if ( newport == mySetupData->get_ascomalpacaport() )   // if same as ASCOM REMOTE server
          {
            MSrvr_DebugPrintln("mp err: new Port = ALPACAPORT");
          }
          else if ( newport == mySetupData->get_webserverport() )     // if same as web server
          {
            MSrvr_DebugPrintln("mp err: new Port = WEBSERVERPORT");
          }
          else if ( newport == mySetupData->get_tcpipport() )        // if same as tcpip server
          {
            MSrvr_DebugPrintln("wp err: new Port = SERVERPORT");
          }
          else
          {
            MSrvr_DebugPrintln("New webserver port = " + String(newport));
            mySetupData->set_mdnsport(newport);                       // assign new port and save it
          }
        }
      }
      else
      {
        MSrvr_DebugPrintln("Attempt to change mdnsserver port when mdnsserver running");
      }
    }
  }

  if ( displaystate == true)
  {
    // if update display state
    msg = mserver.arg("di");
    if ( msg != "" )
    {
      MSrvr_DebugPrint("Set display state: ");
      MSrvr_DebugPrintln(msg);
      if ( msg == "don" )
      {
        mySetupData->set_displayenabled(1);
        myoled->display_on();
      }
      else
      {
        mySetupData->set_displayenabled(0);
        myoled->display_off();
      }
    }
  }
  else
  {
    MSpg.replace("%OLE%", "Display not defined in firmware");     // not checked
  }

  // if oled display page group option update
  // OLED Page Group Option [State] [options][SET]
  msg = mserver.arg("setpg");
  if ( msg != "" )
  {
    String tp = mserver.arg("pg");
    if ( tp == "" )                                     // check for null
    {
      tp = OLEDPGOPTIONALL;
    }
    if ( tp.length() != 3  )                            // check for 3 digits
    {
      tp = OLEDPGOPTIONALL;
    }
    for ( unsigned int i = 0; i < tp.length(); i++)     // check for 0 or 1
    {
      if ( (tp[i] != '0') && (tp[i] != '1') )
      {
        tp = OLEDPGOPTIONALL;
        break;
      }
    }
    MSrvr_DebugPrint(SETPGOPTIONSTR);
    MSrvr_DebugPrintln(msg);
    mySetupData->set_oledpageoption(tp);
  }

  // if oled page time update
  msg = mserver.arg("setpt");
  if ( msg != "" )
  {
    String tp = mserver.arg("pt");
    if ( tp != "" )
    {
      long pgtime = tp.toInt();
      if ( pgtime < LCDPAGETIMEMIN )
      {
        pgtime = LCDPAGETIMEMIN;                       // at least 2s
      }
      else if ( pgtime > LCDPAGETIMEMAX )
      {
        pgtime = LCDPAGETIMEMAX;
      }
      MSrvr_DebugPrint(SETPGTIMESTR);
      MSrvr_DebugPrintln(msg);
      mySetupData->set_lcdpagetime((byte)pgtime);
    }
    else
    {
      // ignore
    }
  }

  // if update start screen
  msg = mserver.arg("ss");
  if ( msg != "" )
  {
    MSrvr_DebugPrint("Set start screen state: ");
    MSrvr_DebugPrintln(msg);
    if ( msg == "sson" )
    {
      mySetupData->set_showstartscreen(1);
    }
    else
    {
      mySetupData->set_showstartscreen(0);
    }
  }

  // if update home position switch messages
  msg = mserver.arg("hp");
  if ( msg != "" )
  {
    MSrvr_DebugPrint("Set hpswmsg state: ");
    MSrvr_DebugPrintln(msg);
    if ( msg == "hpon" )
    {
      mySetupData->set_showhpswmsg(1);
    }
    else
    {
      mySetupData->set_showhpswmsg(0);
    }
  }

  // if update force management server download
  msg = mserver.arg("fd");
  if ( msg != "" )
  {
    MSrvr_DebugPrint("Set ms_force download state: ");
    MSrvr_DebugPrintln(msg);
    if ( msg == "fdon" )
    {
      mySetupData->set_forcedownload(1);
    }
    else
    {
      mySetupData->set_forcedownload(0);
    }
  }

  // OTA, DuckDNS, StaticIP are status only so no need to check or update here

  MANAGEMENT_sendadminpg1();
#ifdef TIMEMSHANDLEPG1
  Serial.print("ms_handlepg1: ");
  Serial.println(millis());
#endif
}

// sender for pg5 color
void MANAGEMENT_sendadminpg5(void)
{
#ifdef TIMEMSSENDPG5
  Serial.print("ms_sendpg5: ");
  Serial.println(millis());
#endif
  MANAGEMENT_buildadminpg5();
  MSrvr_DebugPrintln("root() - send admin pg5");
  MANAGEMENT_sendmyheader();
  MANAGEMENT_sendmycontent();
  MSpg = "";
#ifdef TIMEMSSENDPG5
  Serial.print("ms_sendpg5: ");
  Serial.println(millis());
#endif
  delay(10);
}

// sender for pg4
void MANAGEMENT_sendadminpg4(void)
{
#ifdef TIMEMSSENDPG4
  Serial.print("ms_sendpg4: ");
  Serial.println(millis());
#endif
  MANAGEMENT_buildadminpg4();
  MSrvr_DebugPrintln("root() - send admin pg4");
  MANAGEMENT_sendmyheader();
  MANAGEMENT_sendmycontent();
  MSpg = "";
#ifdef TIMEMSSENDPG4
  Serial.print("ms_sendpg4: ");
  Serial.println(millis());
#endif
  delay(10);
}

// sender for pg3
void MANAGEMENT_sendadminpg3(void)
{
#ifdef TIMEMSSENDPG3
  Serial.print("ms_sendpg3: ");
  Serial.println(millis());
#endif
  MANAGEMENT_buildadminpg3();
  MSrvr_DebugPrintln("root() - send admin pg3");
  MANAGEMENT_sendmyheader();
  MANAGEMENT_sendmycontent();
  MSpg = "";
#ifdef TIMEMSSENDPG3
  Serial.print("ms_sendpg3: ");
  Serial.println(millis());
#endif
  delay(10);
}

// sender for pg2
void MANAGEMENT_sendadminpg2(void)
{
#ifdef TIMEMSSENDPG2
  Serial.print("ms_sendpg2: ");
  Serial.println(millis());
#endif
  MANAGEMENT_buildadminpg2();
  MSrvr_DebugPrintln("root() - send admin pg2");
  MANAGEMENT_sendmyheader();
  MANAGEMENT_sendmycontent();
  MSpg = "";
#ifdef TIMEMSSENDPG2
  Serial.print("ms_sendpg2: ");
  Serial.println(millis());
#endif
  delay(10);
}

// sender for pg1
void MANAGEMENT_sendadminpg1(void)
{
#ifdef TIMEMSSENDPG1
  Serial.print("ms_sendpg1: ");
  Serial.println(millis());
#endif
  MANAGEMENT_buildadminpg1();
  MSrvr_DebugPrintln("root() - send admin pg1");
  MANAGEMENT_sendmyheader();
  MANAGEMENT_sendmycontent();
  MSpg = "";
#ifdef TIMEMSSENDPG1
  Serial.print("ms_sendpg1: ");
  Serial.println(millis());
#endif
  delay(10);
}

// send header for XHTML to client
void MANAGEMENT_sendACAOheader(void)
{
  mserver.sendHeader("Access-Control-Allow-Origin", "*");
}

// send json string to client
void MANAGEMENT_sendjson(String str)
{
  //if ( mySetupData->get_crossdomain() == 1 )
  {
    MANAGEMENT_sendACAOheader();                               // add a cross origin header
  }
  mserver.send(NORMALWEBPAGE, JSONPAGETYPE, str );
}

// generic get handler for client requests
void MANAGEMENT_handleget(void)
{
  // return json string of state, on or off or value
  // ascom, leds, temp, webserver, position, ismoving, display, motorspeed, coilpower, reverse, fixedstepmode, indi
  String jsonstr;

  if ( mserver.argName(0) == "ascom" )
  {
    jsonstr = "{\"ascomserver\":" + String(mySetupData->get_ascomserverstate()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "leds" )
  {
    jsonstr = "{\"ledstate\":" + String(mySetupData->get_inoutledstate()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "tempprobe" )
  {
    jsonstr = "{\"tempprobe\":" + String(mySetupData->get_temperatureprobestate()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "webserver" )
  {
    jsonstr = "{\"webserver\":" + String(mySetupData->get_webserverstate()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "position" )
  {
    jsonstr = "{ \"position\":" + String(mySetupData->get_fposition()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "ismoving" )
  {
    jsonstr = "{ \"ismoving\":" + String(isMoving) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "display" )
  {
    jsonstr = "{ \"display\":" + String(mySetupData->get_displayenabled()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "motorspeed" )
  {
    jsonstr = "{ \"motorspeed\":" + String(mySetupData->get_motorspeed()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "coilpower" )
  {
    jsonstr = "{ \"coilpower\":" + String(mySetupData->get_coilpower()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "reverse" )
  {
    jsonstr = "{ \"reverse\":" + String(mySetupData->get_reversedirection()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "rssi" )
  {
    long rssi = getrssi();
    jsonstr = "{ \"rssi\":" + String(rssi) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "hpsw" )
  {
    jsonstr = "{ \"hpsw\":" + String(mySetupData->get_hpswitchenable()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "fixedstepmode" )
  {
    jsonstr = "{ \"fixedstepmode\":" + String(mySetupData->get_brdfixedstepmode()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "indi" )
  {
    jsonstr = "{ \"indi\":" + String(mySetupData->get_indi()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "coilpowertimeout" )
  {
    jsonstr = "{ \"coilpowertimeout\":" + String(mySetupData->get_coilpower_timeout()) + " }";
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "boardconfig" )
  {
    // send board configuration
    delay(10);
    // Open board_config.jsn file for reading
    File bfile = SPIFFS.open("/board_config.jsn", "r");
    if (!bfile)
    {
      jsonstr = "{ \"err\":\"unable to read file\" }";
    }
    else
    {
      delay(10);
      // Reading board_config.jsn
      jsonstr = bfile.readString();                                // read content of the text file
      MSrvr_DebugPrint("LoadConfiguration(): Board_data= ");
      MSrvr_DebugPrintln(jsonstr);                             // ... and print on serial
      bfile.close();
    }
    MANAGEMENT_sendjson(jsonstr);
  }
  else if ( mserver.argName(0) == "dataconfig" )
  {
    // send controller configuration
    delay(10);
    // Open board_config.jsn file for reading
    File bfile = SPIFFS.open("/data_per.jsn", "r");
    if (!bfile)
    {
      jsonstr = "{ \"err\":\"unable to read file\" }";
    }
    else
    {
      delay(10);
      // Reading data_per.jsn
      jsonstr = bfile.readString();                                // read content of the text file
      MSrvr_DebugPrint("LoadConfiguration(): Board_data= ");
      MSrvr_DebugPrintln(jsonstr);                             // ... and print on serial
      bfile.close();
    }
    MANAGEMENT_sendjson(jsonstr);
  }
  else
  {
    jsonstr = "{ \"error\":\"unknown-command\" }";
    MANAGEMENT_sendjson(jsonstr);
  }
}

// generic set handler for client commands
void MANAGEMENT_handleset(void)
{
  // get parameter after ?
  String jsonstr;
  String value;
  String drvbrd = mySetupData->get_brdname();
  // ascom, leds, tempprobe, webserver, position, move, display, motorspeed, coilpower, reverse, fixedstepmode, indi, coilpowertimeout.
  // boardconfig, dataconfig

  // ascom remote server
  value = mserver.arg("ascom");
  if ( value != "" )
  {
    if ( value == "on" )
    {
      MSrvr_DebugPrintln("ASCOM server:" + value);
      if ( ascomserverstate == STOPPED)
      {
        MSrvr_DebugPrintln("current state off: set on");
        start_ascomremoteserver();
      }
      else
      {
        // already on
        MSrvr_DebugPrintln("current state on");
      }
      jsonstr = "{ \"ascom\":\"on\" }";
    }
    else if ( value == "off" )
    {
      MSrvr_DebugPrintln("ASCOM server:" + value);
      if ( ascomserverstate == RUNNING)
      {
        MSrvr_DebugPrintln("current state on: set off");
        stop_ascomremoteserver();
      }
      else
      {
        // already off
        MSrvr_DebugPrintln("current state off");
      }
      jsonstr = "{ \"ascom\":\"off\" }";
    }
  }

  // in out leds
  value = mserver.arg("leds");
  if ( value != "" )
  {
    if ( value == "on" )
    {
      if ( (mySetupData->get_brdinledpin() == -1) || (mySetupData->get_brdoutledpin() == -1) )
      {
        MSrvr_DebugPrintln("led pins are -1. Cannot enable leds");
        mySetupData->set_inoutledstate(0);
        jsonstr = "{ \"err\":\"off\" }";
      }
      else
      {
        MSrvr_DebugPrintln("LED's: ON");
        if ( mySetupData->get_inoutledstate() == 0)
        {
          mySetupData->set_inoutledstate(1);
          // reinitialise pins
          if (drvbrd.equals("PRO2ESP32ULN2003") || drvbrd.equals("PRO2ESP32L298N") || drvbrd.equals("PRO2ESP32L293DMINI") || drvbrd.equals("PRO2ESP32L9110S") || drvbrd.equals("PRO2ESP32DRV8825") )
          {
            init_leds();
          }
          else
          {
            // already on
          }
          jsonstr = "{ \"err\":\"off\" }";
        }
      }
    }
    else if ( value == "off" )
    {
      MSrvr_DebugPrintln("LED's: OFF");
      if ( mySetupData->get_inoutledstate() == 1)
      {
        mySetupData->set_inoutledstate(0);
      }
      else
      {
        // already off
      }
      jsonstr = "{ \"leds\":\"off\" }";
    }
  }

  // temperature probe
  value = mserver.arg("tempprobe");
  if ( value != "" )
  {
    if ( value == "on" )
    {
      if ( mySetupData->get_brdtemppin() == -1)
      {
        MSrvr_DebugPrintln("temp pin is -1. Cannot enable temp probe");
        mySetupData->set_temperatureprobestate(0);
        jsonstr = "{ \"err\":\"off\" }";
      }
      else
      {
        MSrvr_DebugPrintln("Tempprobe: ON");
        if ( mySetupData->get_temperatureprobestate() == 0)
        {
          mySetupData->set_temperatureprobestate(1);
          myTempProbe = new TempProbe;
        }
        else
        {
          // already enabled
        }
        jsonstr = "{ \"tempprobe\":\"on\" }";
      }
    }
    else if ( value == "off" )
    {
      MSrvr_DebugPrintln("Tempprobe: OFF");
      if ( mySetupData->get_temperatureprobestate() == 1)
      {
        // there is no destructor call
        mySetupData->set_temperatureprobestate(0);
      }
      else
      {
        // already off
      }
      jsonstr = "{ \"tempprobe\":\"off\" }";
    }
  }

  // web server
  value = mserver.arg("webserver");
  if ( value != "" )
  {
    if ( value == "on" )
    {
      MSrvr_DebugPrintln("weberver: ON");
      if ( mySetupData->get_webserverstate() == 0)
      {
        start_webserver();
      }
      else
      {
        // already on
      }
      jsonstr = "{ \"webserver\":\"on\" }";
    }
    else if ( value == "off" )
    {
      MSrvr_DebugPrintln("webserver: OFF");
      if ( mySetupData->get_webserverstate() == 1)
      {
        stop_webserver();
      }
      else
      {
        // already off
      }
      jsonstr = "{ \"webserver\":\"off\" }";
    }
  }

  // position - does not move focuser
  value = mserver.arg("position");
  if ( value != "" )
  {
    unsigned long temp = value.toInt();
    MSrvr_DebugPrint("Set position: ");
    MSrvr_DebugPrintln(temp);
    ftargetPosition = ( temp > mySetupData->get_maxstep()) ? mySetupData->get_maxstep() : temp;
    mySetupData->set_fposition(ftargetPosition);      // current position in SPIFFS
    driverboard->setposition(ftargetPosition);        // current position in driver board
    jsonstr = "{ \"position\":" + String(ftargetPosition) + " }";
  }

  // move - moves focuser position
  value = mserver.arg("move");
  if ( value != "" )
  {
    unsigned long temp = value.toInt();
    MSrvr_DebugPrint("Move to position: ");
    MSrvr_DebugPrintln(temp);
    ftargetPosition = ( temp > mySetupData->get_maxstep()) ? mySetupData->get_maxstep() : temp;
    jsonstr = "{ \"move\":" + String(ftargetPosition) + " }";
  }

  if ( displaystate == true )
  {
    // display
    value = mserver.arg("display");
    if ( value != "" )
    {
      if ( value == "on" )
      {
        MSrvr_DebugPrintln("display: ON");
        mySetupData->set_displayenabled(1);
        myoled->display_on();
        jsonstr = "{ \"display\":\"on\" }";
      }
      else if ( value == "off" )
      {
        MSrvr_DebugPrintln("display: OFF");
        if ( mySetupData->get_displayenabled() == 1)
        {
          mySetupData->set_displayenabled(0);
          myoled->display_off();
        }
        else
        {
          // already off
        }
        jsonstr = "{ \"display\":\"off\" }";
      }
    }
  }

  // motorspeed
  value = mserver.arg("motorspeed");
  if ( value != "" )
  {
    int tmp = value.toInt();
    MSrvr_DebugPrint("Motorspeed: ");
    MSrvr_DebugPrintln(tmp);
    if ( tmp < SLOW )
    {
      tmp = SLOW;
    }
    if ( tmp > FAST )
    {
      tmp = FAST;
    }
    mySetupData->set_motorspeed(tmp);
    jsonstr = "{ \"motorspeed\":\"" + String(tmp) + " }";
  }

  // coilpower
  value = mserver.arg("coilpower");
  if ( value != "" )
  {
    MSrvr_DebugPrint("coilpower:");
    MSrvr_DebugPrintln(value);
    if ( value == "on" )
    {
      mySetupData->set_coilpower(1);
      driverboard->enablemotor();
      jsonstr = "{ \"coilpower\":\"on\" }";
    }
    else if ( value == "off" )
    {
      mySetupData->set_coilpower(0);
      driverboard->releasemotor();
      jsonstr = "{ \"coilpower\":\"off\" }";
    }
  }

  // if reversedirection
  value = mserver.arg("reverse");
  if ( value != "" )
  {
    MSrvr_DebugPrint("reverse:");
    MSrvr_DebugPrintln(value);
    if ( value == "on" )
    {
      mySetupData->set_reversedirection(1);
      jsonstr = "{ \"reverse\":\"on\" }";
    }
    else if ( value == "off" )
    {
      mySetupData->set_reversedirection(0);
      jsonstr = "{ \"reverse\":\"off\" }";
    }
  }

  // home position switch
  value = mserver.arg("hpsw");
  if ( value != "" )
  {
    MSrvr_DebugPrint("hpsw:");
    MSrvr_DebugPrintln(value);
    if ( mySetupData->get_brdhpswpin() == -1 )
    {
      MSrvr_DebugPrintln("hpsw pin is -1. Cannot enable hpsw");
      mySetupData->set_hpswitchenable(0);
      jsonstr = "{ \"err\":\"off\" }";
    }
    else
    {
      if ( value == "on" )
      {
        mySetupData->set_hpswitchenable(1);
        init_homepositionswitch();
        jsonstr = "{ \"hpsw\":\"on\" }";
      }
      else if ( value == "off" )
      {
        mySetupData->set_hpswitchenable(0);
        jsonstr = "{ \"hpsw\":\"off\" }";
      }
    }
  }

  // fixedstepmode for esp8266 boards
  value = mserver.arg("fixedstepmode");
  if ( value != "" )
  {
    int temp = value.toInt();
    MSrvr_DebugPrint("Fixedstepmode: ");
    MSrvr_DebugPrintln(temp);

    mySetupData->set_brdfixedstepmode(temp);
    jsonstr = "{ \"fixedstepmode\"" + String(temp) + " }";
  }

  // INDI
  value = mserver.arg("indi");
  if ( value != "" )
  {
    MSrvr_DebugPrint("indi:");
    MSrvr_DebugPrintln(value);
    if ( value == "on" )
    {
      mySetupData->set_indi(1);
      jsonstr = "{ \"indi\":\"on\" }";
    }
    else if ( value == "off" )
    {
      mySetupData->set_indi(0);
      jsonstr = "{ \"indi\":\"off\" }";
    }
  }

  // CoilPowerTimeout
  value = mserver.arg("coilpowertimeout");
  if ( value != "" )
  {
    MSrvr_DebugPrint("coilpowertimeout:");
    MSrvr_DebugPrintln(value);
    unsigned long temp = value.toInt();
    mySetupData->set_coilpower_timeout(temp);
    jsonstr = "{ \"coilpowertimeout\":" + String(temp) + " }";
  }

  if ( jsonstr != "" )
  {
    MANAGEMENT_sendjson(jsonstr);
  }
  else
  {
    MANAGEMENT_sendjson("{ \"err:\":\"not set\" }");
  }
}

// return network signal strength
void MANAGEMENT_rssi(void)
{
  long rssi = getrssi();
  mserver.send(NORMALWEBPAGE, PLAINTEXTPAGETYPE, String(rssi) );
}

// reboot controller
void MANAGEMENT_reboot(void)
{
  String WaitPage = "<html><meta http-equiv=refresh content=\"" + String(MSREBOOTPAGEDELAY) + "\"><head><title>Management Server></title></head><body><p>Please wait, controller rebooting.</p></body></html>";
  mserver.send(NORMALWEBPAGE, TEXTPAGETYPE, WaitPage );
  WaitPage = "";
  delay(1000);                                          // wait for page to be sent
  software_Reboot(REBOOTDELAY);
}

// called from MANAGEMENT_custombrd()
// display board as json string [for confirmation of new config and give option to write to file - overwrite custom
void MANAGEMENT_genbrd()
{
  // POST event handler
  // to handle reboot option
  MANAGEMENT_checkreboot();                             // if reboot controller;

  String value;
  value = mserver.arg("wrbrd");
  if ( value != "")
  {
    // save board config in custom config file /boards/99.jsn
    // if custom file exists then remove it
    if ( SPIFFS.exists("/boards/99.jsn"))
    {
      // delete existing custom file
      MSrvr_DebugPrintln("File /boards/99.jsn exists");
      SPIFFS.remove("/boards/99.jsn");
    }
    else
    {
      // does not exist so ignore and continue
      MSrvr_DebugPrintln("File /boards/99.jsn does not exist");
    }

    // now write new 99.jsn file from  BoardConfigJson
    delay(10);
    // Open file for writing
    File dfile = SPIFFS.open("/boards/99.jsn", "w");
    if (dfile)
    {
      MSrvr_DebugPrintln("Write new custom file /boards/99.jsn");
      dfile.print(BoardConfigJson);
      dfile.close();
      // file has been written so redirect

      MSpg = "<html><head><title>Management Server</title></head><body><p>Config Board file /boards/99.jsn written</p><p><form action=\"/\" method=\"GET\"><input type=\"submit\" value=\"HOMEPAGE\"></form></p></body></html>";
      MANAGEMENT_sendmyheader();
      MANAGEMENT_sendmycontent();
      MSpg = "";
      return;
    }
    else
    {
      TRACE();
      MSrvr_DebugPrintln(CREATEFILEFAILSTR);
      MSpg = "<html><head><title>Management Server</title></head><body><p>Err: Config Board file not written</p><p><form action=\"/\" method=\"GET\"><input type=\"submit\" value=\"HOMEPAGE\"></form></p></body></html>";
      MANAGEMENT_sendmyheader();
      MANAGEMENT_sendmycontent();
      MSpg = "";
      return;
    }
  }

  // GET handler
  if ( SPIFFS.exists("/genbrd.html"))
  {
    String jsonstr;
    String value;

    jsonstr.reserve(100);
    File file = SPIFFS.open("/genbrd.html", "r");       // open file for read
    MSpg = file.readString();                           // read contents into string
    file.close();

    // process for dynamic data
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
    MSpg.replace("%VER%", String(programVersion));
    MSpg.replace("%NAM%", mySetupData->get_brdname());

    value = mserver.arg("brd");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"board\":\"" + value + "\",";
    }

    value = mserver.arg("max");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"maxstepmode\":\"" + value + "\",";
    }

    value = mserver.arg("stm");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"stepmode\":\"" + value + "\",";
    }

    value = mserver.arg("sda");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"sda\":\"" + value + "\",";
    }

    value = mserver.arg("sck");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"sck\":\"" + value + "\",";
    }

    value = mserver.arg("enp");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"enpin\":\"" + value + "\",";
    }

    value = mserver.arg("stp");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"steppin\":\"" + value + "\",";
    }

    value = mserver.arg("dip");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"dirpin\":\"" + value + "\",";
    }

    value = mserver.arg("tep");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"temppin\":\"" + value + "\",";
    }

    value = mserver.arg("hpp");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"hpswpin\":\"" + value + "\",";
    }

    value = mserver.arg("inp");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"inledpin\":\"" + value + "\",";
    }

    value = mserver.arg("oup");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"outledpin\":\"" + value + "\",";
    }

    value = mserver.arg("pb1");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"pb1pin\":\"" + value + "\",";
    }

    value = mserver.arg("pb2");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"pb2pin\":\"" + value + "\",";
    }

    value = mserver.arg("irp");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"irpin\":\"" + value + "\",";
    }

    value = mserver.arg("str");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"stepsrev\":\"" + value + "\",";
    }

    value = mserver.arg("fim");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"fixedsmode\":\"" + value + "\",";
    }

    value = mserver.arg("brp");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"brdpins\":\"" + value + "\",";
    }

    value = mserver.arg("msd");
    if ( value != "" )
    {
      jsonstr = jsonstr + "\"msdelay\":\"" + value + "\"}";
    }

    MSpg.replace("%BRD%", jsonstr );
    MSpg.replace("%GEN%", "<form action=\"/genbrd\" method=\"POST\"><input type=\"hidden\" name=\"wrbrd\" value=\"true\"><input type=\"submit\" value=\"Write to file\"></form>");

    BoardConfigJson = jsonstr;
    // display heap memory for tracking memory loss, %HEA%
    // only esp32?
    MSpg.replace("%HEA%", String(ESP.getFreeHeap()));

    // add code to handle reboot controller %BT%
    MSpg.replace("%BT%", String(CREBOOTSTR));
  }
  else
  {
    // could not find spiffs file
    TRACE();
    MSrvr_DebugPrintln("File not found");
    MSpg = "File not found";

  }
  MANAGEMENT_sendmyheader();
  MANAGEMENT_sendmycontent();
  MSpg = "";
}

// called from MANAGEMENT_config
// Displays form input values of board config and allow user to update values
// when user clicks generate new custom board config goes to MANAGEMENT_genboard()
void MANAGEMENT_custombrd()
{
  // to handle reboot option
  MANAGEMENT_checkreboot();                             // if reboot controller;

  String value;

  if ( SPIFFS.exists("/custombrd.html"))
  {
    File file = SPIFFS.open("/custombrd.html", "r");    // open file for read
    MSpg = file.readString();                           // read contents into string
    file.close();

    // process for dynamic data
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
    MSpg.replace("%VER%", String(programVersion));
    MSpg.replace("%NAM%", mySetupData->get_brdname());

    MSpg.replace("%STA%", "<form action=\"/genbrd\" method=\"post\"><table><tr>");
    MSpg.replace("%BRD%", "<td>Board Name : </td><td><input type=\"text\" name=\"brd\" value=\"" + mySetupData->get_brdname() + "\"></td></tr><tr>");
    MSpg.replace("%MAX%", "<td>MaxStepMode: </td><td><input type=\"text\" name=\"max\" value=\"" + String(mySetupData->get_brdmaxstepmode()) + "\"></td></tr><tr>");
    MSpg.replace("%STM%", "<td>Step Mode : </td><td><input type=\"text\" name=\"stm\" value=\"" + String(mySetupData->get_brdstepmode()) + "\"></td></tr><tr>");
    MSpg.replace("%SDA%", "<td>I2C SDA : </td><td><input type=\"text\" name=\"sda\" value=\"" + String(mySetupData->get_brdsda()) + "\"></td></tr><tr>");
    MSpg.replace("%SCK%", "<td>I2C SCK : </td><td><input type=\"text\" name=\"sck\" value=\"" + String(mySetupData->get_brdsck()) + "\"></td></tr><tr>");
    MSpg.replace("%ENP%", "<td>Enable pin : </td><td><input type=\"text\" name=\"enp\" value=\"" + String(mySetupData->get_brdenablepin()) + "\"></td></tr><tr>");
    MSpg.replace("%STP%", "<td>Step pin : </td><td><input type=\"text\" name=\"stp\" value=\"" + String(mySetupData->get_brdsteppin()) + "\"></td></tr><tr>");
    MSpg.replace("%DIP%", "<td>Dir pin : </td><td><input type=\"text\" name=\"dip\" value=\"" + String(mySetupData->get_brddirpin()) + "\"></td></tr><tr>");
    MSpg.replace("%TEP%", "<td>Temp pin : </td><td><input type=\"text\" name=\"tep\" value=\"" + String(mySetupData->get_brdtemppin()) + "\"></td></tr><tr>");
    MSpg.replace("%HPP%", "<td>HPSW pin : </td><td><input type=\"text\" name=\"hpp\" value=\"" + String(mySetupData->get_brdhpswpin()) + "\"></td></tr><tr>");
    MSpg.replace("%INP%", "<td>In led pin : </td><td><input type=\"text\" name=\"inp\" value=\"" + String(mySetupData->get_brdinledpin()) + "\"></td></tr><tr>");
    MSpg.replace("%OUP%", "<td>Out led pin : </td><td><input type=\"text\" name=\"oup\" value=\"" + String(mySetupData->get_brdoutledpin()) + "\"></td></tr><tr>");
    MSpg.replace("%PB1%", "<td>PB1 pin : </td><td><input type=\"text\" name=\"pb1\" value=\"" + String(mySetupData->get_brdpb1pin()) + "\"></td></tr><tr>");
    MSpg.replace("%PB2%", "<td>PB2 pin : </td><td><input type=\"text\" name=\"pb2\" value=\"" + String(mySetupData->get_brdpb2pin()) + "\"></td></tr><tr>");
    MSpg.replace("%IRP%", "<td>IR pin : </td><td><input type=\"text\" name=\"irp\" value=\"" + String(mySetupData->get_brdirpin()) + "\"></td></tr><tr>");
    MSpg.replace("%STR%", "<td>Steps per rev : </td><td><input type=\"text\" name=\"str\" value=\"" + String(mySetupData->get_brdstepsperrev()) + "\"></td></tr><tr>");
    MSpg.replace("%FIM%", "<td>Fixed Step Mode: </td><td><input type=\"text\" name=\"fim\" value=\"" + String(mySetupData->get_brdfixedstepmode()) + "\"></td></tr><tr>");
    String boardpins;
    boardpins.reserve(20);
    boardpins = "[";
    for ( int i = 0; i < 4; i++ )
    {
      boardpins = boardpins + String( mySetupData->get_brdboardpins(i) );
      if ( i < 3 )
      {
        boardpins = boardpins + ", ";
      }
    }
    boardpins = boardpins + "]";
    MSpg.replace("%BRP%", "<td>Board pins : </td><td><input type=\"text\" name=\"brp\" value=\"" + boardpins + "\"></td></tr><tr>");
    MSpg.replace("%MSD%", "<td>MS Delay : </td><td><input type=\"text\" name=\"msd\" value=\"" + String(mySetupData->get_brdmsdelay()) + "\"></td></tr></table>");
    MSpg.replace("%END%", "<input type=\"submit\" value=\"GENERATE NEW BOARD CONFIG\"></form>");

    // display heap memory for tracking memory loss, %HEA%
    // only esp32?
    MSpg.replace("%HEA%", String(ESP.getFreeHeap()));

    // add code to handle reboot controller %BT%
    MSpg.replace("%BT%", String(CREBOOTSTR));
  }
  else
  {
    // could not find spiffs file
    TRACE();
    MSrvr_DebugPrintln("File not found");
    MSpg = "File not found";
  }
  MANAGEMENT_sendmyheader();
  MANAGEMENT_sendmycontent();
  MSpg = "";
}

// show current board config as json string
void MANAGEMENT_showboardconfig()
{
  // to handle reboot option
  MANAGEMENT_checkreboot();                             // if reboot controller;

  // try to load showconfig.html
  if ( SPIFFS.exists("/showconfig.html"))
  {
    File file = SPIFFS.open("/showconfig.html", "r");   // open file for read
    MSpg = file.readString();                           // read contents into string
    file.close();

    // process for dynamic data
    // approx size of page 3310
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
    MSpg.replace("%VER%", String(programVersion));
    MSpg.replace("%NAM%", mySetupData->get_brdname());

    MSpg.replace("%BDN%", mySetupData->get_brdname());
    MSpg.replace("%MSM%", String(mySetupData->get_brdmaxstepmode()));
    MSpg.replace("%SMO%", String(mySetupData->get_brdstepmode()));
    MSpg.replace("%SDA%", String(mySetupData->get_brdsda()));
    MSpg.replace("%SCK%", String(mySetupData->get_brdsck()));
    MSpg.replace("%ENP%", String(mySetupData->get_brdenablepin()));
    MSpg.replace("%STP%", String(mySetupData->get_brdsteppin()));
    MSpg.replace("%DIP%", String(mySetupData->get_brddirpin()));
    MSpg.replace("%TEP%", String(mySetupData->get_brdtemppin()));
    MSpg.replace("%HPP%", String(mySetupData->get_brdhpswpin()));
    MSpg.replace("%INP%", String(mySetupData->get_brdinledpin()));
    MSpg.replace("%OUP%", String(mySetupData->get_brdoutledpin()));
    MSpg.replace("%IRP%", String(mySetupData->get_brdirpin()));

    String boardpins;
    boardpins.reserve(20);
    for ( int i = 0; i < 4; i++ )
    {
      boardpins = boardpins + String( mySetupData->get_brdboardpins(i) );
      if ( i < 3 )
      {
        boardpins = boardpins + ",";
      }
    }
    MSpg.replace("%BRP%", boardpins);

    MSpg.replace("%SPR%", String(mySetupData->get_brdstepsperrev()));
    MSpg.replace("%FSM%", String(mySetupData->get_brdfixedstepmode()));
    MSpg.replace("%PB1%", String(mySetupData->get_brdpb1pin()));
    MSpg.replace("%PB2%", String(mySetupData->get_brdpb2pin()));
    MSpg.replace("%MSD%", String(mySetupData->get_brdmsdelay()));

    // display heap memory for tracking memory loss, %HEA%
    // only esp32?
    MSpg.replace("%HEA%", String(ESP.getFreeHeap()));

    // add code to handle reboot controller %BT%
    MSpg.replace("%BT%", String(CREBOOTSTR));
  }
  else
  {
    // could not find spiffs file
    TRACE();
    MSrvr_DebugPrintln("File not found");
    MSpg = "File not found";

  }
  MANAGEMENT_sendmyheader();
  MANAGEMENT_sendmycontent();
  MSpg = "";
}

// build config page
void MANAGEMENT_buildconfigpg()
{
  // Filesystem was started earlier when server was started so assume it has started
  MSrvr_DebugPrintln("buildconfigpg: Start");
  if ( SPIFFS.exists("/config.html"))
  {
    File file = SPIFFS.open("/config.html", "r");       // open file for read
    MSpg = file.readString();                           // read contents into string
    file.close();

    // process for dynamic data
    String bcol = mySetupData->get_wp_backcolor();
    MSpg.replace("%BKC%", bcol);
    String txtcol = mySetupData->get_wp_textcolor();
    MSpg.replace("%TXC%", txtcol);
    String ticol = mySetupData->get_wp_titlecolor();
    MSpg.replace("%TIC%", ticol);
    String hcol = mySetupData->get_wp_headercolor();
    MSpg.replace("%HEC%", hcol);
    MSpg.replace("%VER%", String(programVersion));
    MSpg.replace("%NAM%", mySetupData->get_brdname());

    // add code to handle reboot controller %BT%
    MSpg.replace("%BT%", String(CREBOOTSTR));

    // display heap memory for tracking memory loss, %HEA%
    // only esp32?
    MSpg.replace("%HEA%", String(ESP.getFreeHeap()));
  }
  else
  {
    // could not read file
    TRACE();
    MSrvr_DebugPrintln("File not found");
    MSpg = "File not found";
  }
}

// handler for config page
void MANAGEMENT_confighandler()
{
  MSrvr_DebugPrintln("confighandler");

  // to handle reboot option
  MANAGEMENT_checkreboot();                              // if reboot controller;

  MANAGEMENT_buildconfigpg();
  MANAGEMENT_sendmyheader();
  MANAGEMENT_sendmycontent();
  MSpg = "";
}

// send config to client
void MANAGEMENT_config()
{
  // This will be start of config options
  // After post this will goto either customconfig() or predefinedconfig() or showboardconfig()
  MSrvr_DebugPrintln("config");
  MANAGEMENT_buildconfigpg();
  MANAGEMENT_sendmyheader();
  MANAGEMENT_sendmycontent();
  MSpg = "";
}

// start management server
void start_management(void)
{
  if ( !SPIFFS.begin() )
  {
    TRACE();
    MSrvr_DebugPrintln("Err: spiffs not started");
    MSrvr_DebugPrintln("stop management server");
    managementserverstate = STOPPED;
    return;
  }
  MSpg.reserve(MAXMANAGEMENTPAGESIZE);        // largest page is MANAGEMENT_buildpredefinedboard() = 3469
  BoardConfigJson.reserve(MAXCUSTOMBRDJSONSIZE);

  mserver.on("/",         HTTP_GET,  MANAGEMENT_sendadminpg1);
  mserver.on("/",         HTTP_POST, MANAGEMENT_handleadminpg1);
  mserver.on("/msindex1", HTTP_GET,  MANAGEMENT_sendadminpg1);
  mserver.on("/msindex1", HTTP_POST, MANAGEMENT_handleadminpg1);
  mserver.on("/msindex2", HTTP_GET,  MANAGEMENT_sendadminpg2);
  mserver.on("/msindex2", HTTP_POST, MANAGEMENT_handleadminpg2);
  mserver.on("/msindex3", HTTP_GET,  MANAGEMENT_sendadminpg3);
  mserver.on("/msindex3", HTTP_POST, MANAGEMENT_handleadminpg3);
  mserver.on("/msindex4", HTTP_GET,  MANAGEMENT_sendadminpg4);            // web colors
  mserver.on("/msindex4", HTTP_POST, MANAGEMENT_handleadminpg4);
  mserver.on("/color",    HTTP_GET,  MANAGEMENT_sendadminpg5);
  mserver.on("/color",    HTTP_POST, MANAGEMENT_handleadminpg5);          // color picker
  mserver.on("/delete",   HTTP_GET,  MANAGEMENT_deletepage);
  mserver.on("/delete",   HTTP_POST, MANAGEMENT_handledeletefile);
  mserver.on("/list",     HTTP_GET,  MANAGEMENT_listFSfiles);
  mserver.on("/upload",   HTTP_GET,  MANAGEMENT_fileupload);
  mserver.on("/mssuccess",           MANAGEMENT_fileuploadsuccess);
  mserver.on("/rssi",     HTTP_GET,  MANAGEMENT_rssi);
  mserver.on("/set",                 MANAGEMENT_handleset);               // generic set function
  mserver.on("/get",                 MANAGEMENT_handleget);               // generic get function

  mserver.on("/config",    HTTP_GET,  MANAGEMENT_config);
  mserver.on("/config",   HTTP_POST, MANAGEMENT_confighandler);
  mserver.on("/showconfig",          MANAGEMENT_showboardconfig);
  mserver.on("/custombrd",           MANAGEMENT_custombrd);
  mserver.on("/genbrd",              MANAGEMENT_genbrd);

  mserver.on("/upload",   HTTP_POST, []() {
    mserver.send(NORMALWEBPAGE);
  }, MANAGEMENT_handlefileupload );
  mserver.onNotFound([]() {                             // if the client requests any URI
    if (!MANAGEMENT_handlefileread(mserver.uri()))      // send file if it exists
    {
      MANAGEMENT_handlenotfound();                      // otherwise, respond with a 404 (Not Found) error
    }
  });
  mserver.begin();
  managementserverstate = RUNNING;
  TRACE();
  MSrvr_DebugPrintln("management server started");
  delay(10);                                            // small pause so background tasks can run
}

// stop management server
void stop_management(void)
{
  if ( managementserverstate == RUNNING )
  {
    mserver.stop();
    managementserverstate = STOPPED;
    TRACE();
    MSrvr_DebugPrintln("management server stopped");
  }
  else
  {
    MSrvr_DebugPrintln("management server not running");
  }
}

#endif // #ifdef MANAGEMENT
