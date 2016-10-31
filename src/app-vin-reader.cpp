/* VIN reader for Carloop
 *
 * Reads the Vehicle Identification Number (BIN) at 500 kbit and outputs it
 * to the USB serial port and as Particle events.
 *
 * Type 'r' on the serial port to start reading VIN.
 * Mac OSX: screen /dev/tty.usbmodem1411 (update for your port number)
 * Linux: screen /dev/ttyACM*
 * Windows: Use PuTTY
 *
 * To read codes through the network run these 2 commands in different terminals:
 * particle subscribe mine
 * particle call my_carloop readVIN
 *
 * Copyright 2016 1000 Tools, Inc
 *
 * Distributed under the MIT license. See LICENSE.txt for more details.
 */

#include "application.h"
#include "carloop.h"

#include "OBDMessage.h"

SYSTEM_THREAD(ENABLED);
#define OBD_CAN_BROADCAST_ID 0x7DF

void readVIN();
int startReadVIN(String unused);
void processSerial();

// Set up the Carloop hardware
Carloop<CarloopRevision2> carloop;

void readVIN() {
  Serial.println("Reading VIN");

  // Send request to read VIN
  CANMessage message;
  message.id = OBD_CAN_BROADCAST_ID;
  message.len = 8; // just always use 8
  message.data[0] = 0x02; // 0 = single-frame format, 2  = num data bytes
  message.data[1] = 0x09;
  message.data[2] = 0x02;
  carloop.can().transmit(message);

  OBDMessage obd;
  unsigned long start = millis();
  unsigned long timeout = 200; // ms
  while(millis() - start < timeout) {
    if (carloop.can().receive(message)) {
      if (message.id == 0x7E8) {
        Serial.println("Got reply from ECU");

        // Add the data to our OBD message
        bool needsFlowControl = obd.addMessageData(message);

        if (needsFlowControl) {
          // Sending flow control
          CANMessage flowControl = obd.flowControlMessage();
          carloop.can().transmit(flowControl);
          Serial.println("Sent flow control");
        }

        // Use the data when the message is complete
        if (obd.complete()) {
          String vin = "";
          // VIN is 17 character and can be left-padded with zeros
          for (int i = obd.size() - 17; i < obd.size(); i++) {
            vin += String((char)obd.data()[i]);
          }
          Serial.println("VIN: " + vin);
          if (Particle.connected()) {
            Particle.publish("vin/result", vin, PRIVATE);
          }

          obd.clear();
          Serial.println("Done");
          return;
        }
      }
    }
  }

  Serial.println("Timeout");
  if (Particle.connected()) {
    Particle.publish("vin/error", PRIVATE);
  }
}

void setup() {
  Serial.begin(9600);
  carloop.begin();
  Particle.function("readVIN", startReadVIN);
}

// Remote function to start reading codes
int startReadVIN(String unused) {
  readVIN();
  return 0;
}

void loop() {
  processSerial();
}

void processSerial() {
  // Type letter on the serial port to read VIN
  switch (Serial.read()) {
    case 'r':
      readVIN();
      break;
  }
}
