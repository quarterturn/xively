// rf69_reliable_datagram_server.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple addressed, reliable messaging server
// with the RHReliableDatagram class, using the RH_RF69 driver to control a RF69 radio.
// It is designed to work with the other example rf69_reliable_datagram_client
// Tested on Moteino with RFM69 http://lowpowerlab.com/moteino/
// Tested on miniWireless with RFM69 www.anarduino.com/miniwireless
// Tested on Teensy 3.1 with RF69 on PJRC breakout board

#include <RHReliableDatagram.h>
#include <RH_RF69.h>
#include <SPI.h>

#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

// Singleton instance of the radio driver
RH_RF69 driver;
//RH_RF69 driver(15, 16); // For RF69 on PJRC breakout board with Teensy 3.1
//RH_RF69 rf69(4, 2); // For MoteinoMEGA https://lowpowerlab.com/shop/moteinomega

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram manager(driver, SERVER_ADDRESS);

// used to store 16 bit int in two bytes
union itag {
  uint8_t b[2];
  uint16_t i;
}it;

float humd;
float temp;
uint16_t height;

void setup() 
{
  Serial.begin(115200);
  Serial.println("Starting up.");
  if (!manager.init())
    Serial.println("init failed");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM

  // If you are using a high power RF69, you *must* set a Tx power in the
  // range 14 to 20 like this:
  driver.setTxPower(20);
}

uint8_t data[] = "ok";
// Dont put this on the stack:
uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];

void loop()
{
  if (manager.available())
  {
    // Wait for a message addressed to us from the client
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager.recvfromAck(buf, &len, &from))
    {
//      Serial.print("got request from : 0x");
//      Serial.print(from, HEX);
//      Serial.print(": ");
//      Serial.print(" humidity % = ");
      it.b[0] = buf[0];
      it.b[1] = buf[1];
      // convert back to float
      humd = (float)it.i/100.0;
      Serial.print(humd, 2);
//      Serial.print(", temp C = ");
      Serial.print(",");
      it.b[0] = buf[2];
      it.b[1] = buf[3];
      // convert value back to float
      // we add 100 at sender to avoid negative values
      // so remove it here by subtracting 100 
      temp = (float)((it.i - 100.0) / 100.0);
      Serial.print(temp, 2);
      // convert back to uint16_t
//      Serial.print(", height cm = ");
      Serial.print(",");
      it.b[0] = buf[4];
      it.b[1] = buf[5];
      height = it.i;
      // convert to height from stream bed
      // the sensor is mounted approximately 222 cm from the stream bed
      height = 222 - height;
      Serial.println(height);

      // Send a reply back to the originator client
      if (!manager.sendtoWait(data, sizeof(data), from))
        Serial.println("sendtoWait failed");
    }
  }
}

