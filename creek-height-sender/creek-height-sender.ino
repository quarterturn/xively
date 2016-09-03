// rf69_reliable_datagram_client.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple addressed, reliable messaging client
// with the RHReliableDatagram class, using the RH_RF69 driver to control a RF69 radio.
// It is designed to work with the other example rf69_reliable_datagram_server
// Tested on Moteino with RFM69 http://lowpowerlab.com/moteino/
// Tested on miniWireless with RFM69 www.anarduino.com/miniwireless
// Tested on Teensy 3.1 with RF69 on PJRC breakout board

#include <RHReliableDatagram.h>
#include <RH_RF69.h>
#include <SPI.h>
// added these since I want to test sleep/wake
#include <Wire.h>
#include <Time.h>
#include <MCP7940RTC.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <SparkFunHTU21D.h>
#include <NewPing.h>

#define MCP7940_CTRL_ID 0x6F 

#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

#define LED_PIN 9
#define WAKE_PIN 3
// used by the ultrasonic sensor for data
#define PING_PIN 4
// used to power the ultrasonic sensor on/off via a pfet
#define PING_PWR_PIN 5
// used to tell NewPing how long to wait for a return ping
// max distance in cm; about 13 ft
#define MAX_DISTANCE 400

// new sonar object
NewPing sonar(PING_PIN, PING_PIN, MAX_DISTANCE);
uint16_t distance;

// Singleton instance of the radio driver
RH_RF69 driver;
//RH_RF69 driver(15, 16); // For RF69 on PJRC breakout board with Teensy 3.1
//RH_RF69 rf69(4, 2); // For MoteinoMEGA https://lowpowerlab.com/shop/moteinomega

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram manager(driver, CLIENT_ADDRESS);

// used to store 16 bit int in two bytes
union itag {
  uint8_t b[2];
  uint16_t i;
}it;

// sleep for 10 minutes (600 seconds)
long sleepIntervalSec=600;
long loopCnt=0;

int gStat=0;
uint8_t d[8];
uint8_t dAlarm[8];
int d1[8];
int iContext=0;

// temp/humidity sensor 
HTU21D myHumidity;
float humd; // = myHumidity.readHumidity();
float temp; // = myHumidity.readTemperature()

void setup() 
{

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(WAKE_PIN,INPUT);
  Wire.begin();

  // default to no power to ping sensor
  // via pfet, so low == off
  pinMode(PING_PWR_PIN, OUTPUT);
  digitalWrite(PING_PWR_PIN, HIGH);

  
  Serial.begin(115200);
  if (!manager.init())
    Serial.println("init failed");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM

  // If you are using a high power RF69, you *must* set a Tx power in the
  // range 14 to 20 like this:
  driver.setTxPower(20);

  myHumidity.begin();
}

uint8_t data[6];
// Dont put this on the stack:
uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];

void loop()
{
  Serial.println("Sending to rf69_reliable_datagram_server");

  // get temperature and humidity
  humd = myHumidity.readHumidity();
  temp = myHumidity.readTemperature();

  // power up ping sensor
  digitalWrite(PING_PWR_PIN, LOW);

  // wait a bit so the ping sensor can stabilize
  delay(1000);

  // turn on LED
  digitalWrite(LED_PIN, HIGH);

  
  // get distance to water surface
  distance = sonar.ping_cm();
  // power down ping sensor
  digitalWrite(PING_PWR_PIN, HIGH);

  Serial.println(humd, DEC);
  Serial.println(temp, DEC);
  Serial.println(distance, DEC);

  // humidity
  // shift the temp data two decimal places to the right
  // and use union to convert to bytes
  it.i = int(humd * 100.0);
  data[0] = it.b[0];
  data[1] = it.b[1];
  // temperature
  // shift the temp data two decimal places to the right
  // and use union to convert to bytes
  // add 100 to temp to avoid ever sending a negative value
  // we will have to subtract 100 on the receiver side 
  it.i = int(100 + temp * 100.0);
  data[2] = it.b[0];
  data[3] = it.b[1];
  // distance
  // use union to convert to bytes
  it.i = distance;
  data[4] = it.b[0];
  data[5] = it.b[1];
    
  // Send a message to manager_server
  // try up to five times to send data
  for (uint8_t retryCount = 0; retryCount < 5; retryCount++)
  {
    if (manager.sendtoWait(data, sizeof(data), SERVER_ADDRESS))
    {
      // Now wait for a reply from the server
      uint8_t len = sizeof(buf);
      uint8_t from;   
      if (manager.recvfromAckTimeout(buf, &len, 2000, &from))
      {
        Serial.print("got reply from : 0x");
        Serial.print(from, HEX);
        Serial.print(": ");
        Serial.println((char*)buf);
        // exit the for loop
        break;
      }
      else
      {
        Serial.println("No reply, is creek height receiver on?");
      }
    }
    else
      Serial.println("sendtoWait failed");
  }

  // turn off LED
  digitalWrite(LED_PIN, LOW);

  // put the radio into sleep mode
  driver.sleep();

  
  // testing the use of the RTC to sleep for longer periods of time
  // vs watchdog timer and a counter
  //Serial.println("setting new sleep time: 60-sec.");
  clearAlarm();
  delay(20);
  setNewAlarm();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  
  sleep_enable();
  interrupts();
  attachInterrupt(1,sleepHandler, FALLING);
  //Serial.println("going to sleep...");
  delay(20);
  sleep_mode();  //sleep now
  //--------------- ZZZZZZ sleeping here
  sleep_disable(); //fully awake now
  //Serial.println("waking up...");
  detachInterrupt(1);//);
  
}

// Convert Decimal to Binary Coded Decimal (BCD)
uint8_t dec2bcd(uint8_t num)
{
  return ((num/10 * 16) + (num % 10));
}

// Convert Binary Coded Decimal (BCD) to Decimal
uint8_t bcd2dec(uint8_t num)
{
  return ((num/16 * 10) + (num % 16));
}

void setNewAlarm() {
  static MCP7940RTC rtc;
  time_t t = rtc.get();
  //Serial.print("dayofweek=");
  //Serial.println(dayOfWeek(t));
  // must get proper wday... TODO: Fix in library
  
  //Serial.print(" ");
  Serial.print("t=");
  Serial.print(t);
  Serial.print(" ");
  tmElements_t tm1;
  breakTime(t,tm1);
  uint8_t b=0;
  // check tm1.Wday for differences...
  // Must ensure Wday consistency in RTC chip!!!
    Wire.beginTransmission(MCP7940_CTRL_ID);
    Wire.write((uint8_t)0x03);
    Wire.endTransmission();    
    Wire.requestFrom(MCP7940_CTRL_ID, 1);     
    while(Wire.available()) {
      b = Wire.read();
    }
    Wire.endTransmission();
    delay(5);
    b &= 0xf8;
    b |= dec2bcd(dayOfWeek(t));
    Wire.beginTransmission(MCP7940_CTRL_ID);
    Wire.write((uint8_t)0x03);
    Wire.write(b);
    Wire.endTransmission();
  //end of RTC setting of Wday

  Serial.print("tm1=");
  Serial.print(tm1.Year);//+1970);
  Serial.print(" ");
  Serial.print(tm1.Month);
  Serial.print(" ");
  Serial.print(tm1.Wday);
  Serial.print(" ");
  Serial.print(tm1.Day);
  Serial.print(" ");
  Serial.print(tm1.Hour);
  Serial.print(" ");
  Serial.print(tm1.Minute);
  Serial.print(" ");
  Serial.print(tm1.Second);
  t += sleepIntervalSec;
  Serial.print("  tA=");
  Serial.print(t);  
  Serial.println();
  
  //rtc.setAlarm0(t);
  setAlarm(t);
  delay(10);
  compareAlarm0();
  loopCnt++;
}

void compareAlarm0() {
  uint8_t dmask[] = { 0x7f, 0x7f, 0x7f, 0x07, 0x3f, 0x1f };

  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x00);
  Wire.endTransmission();
  Wire.requestFrom(MCP7940_CTRL_ID, 7); 
  memset(d,0,8);
  int i=0;
  while(Wire.available()) {
    if(i==7) break;
    d[i] = Wire.read() & dmask[i];
    i++;
    delay(1);
  }
  Wire.endTransmission();
  
  Serial.print("0-6: ");
  for(int i=0; i<7; i++) {
    if(d[i]<0x10) Serial.print("0");
    Serial.print((uint8_t)d[i],HEX);
    Serial.print(" ");
  }
  Serial.println(" (rtc time)");  
  
  memset(d,0,8);
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x0a);
  Wire.endTransmission();
  Wire.requestFrom(MCP7940_CTRL_ID, 6); 
  memset(d,0,8);
  i=0;
  while(Wire.available()) {
    if(i==6) break;
    d[i] = Wire.read() & dmask[i];
    i++;
    delay(1);
  }
  Wire.endTransmission();
  
  Serial.print("0-5: ");
  for(int i=0; i<6; i++) {
    if(d[i]<0x10) Serial.print("0");
    Serial.print((uint8_t)d[i],HEX);
    Serial.print(" ");
  }
  Serial.println(" (alarm0 time)");
  delay(10);
}

void setAlarm(time_t t) {
  tmElements_t tm;
  breakTime(t, tm);
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x0a);
  Wire.write((uint8_t)dec2bcd(tm.Second) & 0x7f);       // Seconds
  Wire.write((uint8_t)dec2bcd(tm.Minute) & 0x7f);       // Minutes
  Wire.write((uint8_t)dec2bcd(tm.Hour) & 0x3f);         // Hour
  Wire.write((uint8_t)(dec2bcd(tm.Wday) | 0x70) & 0xf7);// wDay, trigger on minutes matching
  Wire.write((uint8_t)dec2bcd(tm.Day) & 0x3f);          // Day
  Wire.write((uint8_t)dec2bcd(tm.Month) & 0x1f);        // Hour
  Wire.endTransmission();  
  delay(2);
  // turn on alarm  
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x07);
  Wire.write((uint8_t)0x17);
  Wire.endTransmission();
}
  
void clearAlarm() {
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x07);
  Wire.write((uint8_t)0x80);
  Wire.endTransmission();
  delay(20);

// reset the interrupt flag
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x0d);
  Wire.endTransmission();
  delay(20);
  Wire.requestFrom(MCP7940_CTRL_ID, 1);
  byte b1 = Wire.read();
  Wire.endTransmission();
  delay(20);
  b1 &= 0xf7;
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x0d);
  Wire.write((uint8_t)b1 & 0xf7);
  Wire.endTransmission();
  delay(20);
}

void sleepHandler() {
  Serial.println("Processing...");  
  gStat=1;
}    
