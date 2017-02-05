/**
 Remote Controller and Logger
 
 Monitors karkanya.net
 
 */


import processing.net.*;
import processing.serial.*;

PFont f;

Serial myPort;      // The serial port
int whichKey = -1;  // Variable to hold keystoke values
int lastMode;
String modeStr;

float irTemp;
String temp;

Client c;
String data;
String[] token;
String filename;
String TimeStamp;
String DateStamp;
String[] logs = {};
int lastState;
static int standbyMode = 0;
static int protectMode = 1;
static int fireMode = 2;
static int checkStatusDelayDay = 1;   // Wait 1 minute during daytime hours
static int checkStatusDelayNight = 15;   // Wait 15 minutes during nighttime hours
int fastCycle;                          // if the Alert mode set, go to fast checking of the web site - 10 seconds

int bufferIndex;
int[] buffer = new int[20];
String postString;

PrintWriter output;

void setup() {
  size(640, 360);
  modeStr = "NotReadYet";
  temp = "0.00";

  // printArray(PFont.list());
  //f = createFont("SourceCodePro-Regular.ttf", 12);
  //textFont(f);
  //textAlign(LEFT);
  lastState = -1;
  lastMode = -1;

  background(50);
  fill(200);
  bufferIndex = 0;
  fastCycle = 0;    // normal cycle times by time of day
  int d = day();    // Values from 1 - 31
  int m = month();  // Values from 1 - 12
  int y = year();   // 2003, 2004, 2005, etc.



  String DayS = String.valueOf(d);
  String MonthS = String.valueOf(m);
  String YearS = String.valueOf(y);
  int hour = hour();
  int min = minute();
  int sec = second();
  String HourS = String.valueOf(hour);
  String MinS = String.valueOf(min);
  String SecS = String.valueOf(sec);
  DateStamp = YearS + "-" + MonthS + "-" + DayS + "-" + HourS + "-" + MinS + "-" + SecS;
  filename = "logfile" + DateStamp + ".csv";
  output = createWriter(filename); 
  printArray(Serial.list());
  String portName = Serial.list()[1]; // 1 for COM3 on DMZ, 7 for Mega test board on Mac
  println("Using port " + portName);
  myPort = new Serial(this, portName, 9600);
}

void draw() {
  postString = "controllerState=" + modeStr + "&IRTemp=" + temp;
  c = new Client(this, "www.karkanya.net", 80); // Connect to server on port 80
  c.write("POST /readstateV2.php HTTP/1.0\r\n"); // Use the HTTP "GET" command to ask for a Web page
  c.write("Host: karkanya.net\n"); // Be polite and say who we are
  c.write("Content-Type: application/x-www-form-urlencoded\n");
  c.write("Content-Length: " + postString.length() + "\n");
  c.write("\n");
  c.write("controllerState=" + modeStr + "&irTemp=" + temp); // Send the controller state to the web site
  delay(4000);
  if (c.available() > 0) { // If there's incoming data from the web site...
    data = c.readString(); // ...then grab it and print it
    token = splitTokens(data, "<>");
    // printArray(token);
    println("State from web site: " + token[16] + "\n");
    String[] standby = match(token[16], "Standby");
    String[] protect = match(token[16], "Protect");
    String[] allon = match(token[16], "AllOn");
    String[] alert = match(token[16], "Alert");
    if (standby != null && lastState != standbyMode) {
      myPort.write(0);
      lastState = standbyMode;
      fastCycle = 0;
    }
    if (protect != null && lastState != protectMode) {
      myPort.write(1);
      lastState = protectMode;
      fastCycle = 30000;
    }
    if (allon != null && lastState != fireMode) {
      myPort.write(2);
      lastState = fireMode;
      fastCycle = 30000;
    }
    if (alert != null) {
      fastCycle = 10000; // 10 seconds when "Alert"
    }
  }
  if (fastCycle > 0) {
    delay(fastCycle);
  } else {
    if ((hour() > 6) && (hour() < 18)) {
      delay(checkStatusDelayDay * 60000);
    } else {
      delay(checkStatusDelayNight * 60000);
    }
  }
}

void serialEvent(Serial myPort) {
  int inByte = -1;    // Incoming serial data
  inByte = myPort.read();
  if (inByte == '?') {
    bufferIndex = 0;
  }
  if ((inByte != '?') && (inByte != '$')) {
    buffer[bufferIndex] = inByte;
    bufferIndex = bufferIndex + 1;
  }
  if (inByte == '$') {
    if (buffer[0] == 'M') {
      int mode = buffer[1];
      if (mode != lastMode) {
        if (mode == 0) { 
          println("Controller mode has changed to Standby"); 
          modeStr = "Standby";
        }
        if (mode == 1) { 
          println("Controller mode has changed to Protect"); 
          modeStr = "Protect";
        }      
        if (mode == 2) { 
          println("Controller mode has changed to AllOn"); 
          modeStr = "AllOn";
        }      
        lastMode = mode;
      // Immediate update to site to refelct change in state
      postString = "controllerState=" + modeStr + "&IRTemp=" + temp;
      c = new Client(this, "www.karkanya.net", 80); // Connect to server on port 80
      c.write("POST /readstateV2.php HTTP/1.0\r\n"); // Use the HTTP "GET" command to ask for a Web page
      c.write("Host: karkanya.net\n"); // Be polite and say who we are
      c.write("Content-Type: application/x-www-form-urlencoded\n");
      c.write("Content-Length: " + postString.length() + "\n");
      c.write("\n");
      c.write("controllerState=" + modeStr + "&irTemp=" + temp); // Send the controller state to the web site
      }
      myPort.write('$');  // Signal ready to accept another temperature reading
 
    }
    if (buffer[0] == 'T') {
      int irTempInt = (int) buffer[2];
      int irTempFrac = (int) buffer[3];
      irTemp = irTempInt + ((float) irTempFrac / 100);
      if (buffer[1] == '-') {
        irTemp = -1 * irTemp;
      }
      int d = day();    // Values from 1 - 31
      int m = month();  // Values from 1 - 12
      int y = year();   // 2003, 2004, 2005, etc.
      int hour = hour();
      int min = minute();
      int sec = second();
      String DayS = String.valueOf(d);
      String MonthS = String.valueOf(m);
      String YearS = String.valueOf(y);
      String HourS = String.valueOf(hour);
      String MinS = String.valueOf(min);
      String SecS = String.valueOf(sec);
      if (hour < 10) { 
        HourS = "0" + HourS;
      }
      if (min < 10) { 
        MinS = "0" + MinS;
      }
      if (sec < 10) { 
        SecS = "0" + SecS;
      }
      TimeStamp = YearS + "/" + MonthS + "/" + DayS + "," + HourS + ":" + MinS + ":" + SecS;
      temp = str(irTemp);
      String log = TimeStamp + "," + modeStr +  "," + temp;
      output.println(log);
      output.flush();
      background(50);
      fill(255);
      text(log, 10, 50);
      text('Z', 50, 10);
      println(log);
      myPort.write('$');  // Signal ready to accept another temperature reading
    }
  }
}