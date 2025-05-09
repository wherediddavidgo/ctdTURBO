/*
  Software serial multple serial test

 Receives from the two software serial ports,
 sends to the hardware serial port.

 In order to listen on a software port, you call port.listen().
 When using two software serial ports, you have to switch ports
 by listen()ing on each one in turn. Pick a logical time to switch
 ports, like the end of an expected transmission, or when the
 buffer is empty. This example switches ports when there is nothing
 more to read from a port

 The circuit:
 Two devices which communicate serially are needed.
 * First serial device's TX attached to digital pin 9, RX to pin 11
 * Second serial device's TX attached to digital pin 8, RX to pin 10

 This example code is in the public domain.

 */

#include <SoftwareSerial.h>
// software serial #1: TX = digital pin 11, RX = digital pin 9
SoftwareSerial portOne(9, 11);

// software serial #2: TX = digital pin 10, RX = digital pin 8
SoftwareSerial portTwo(8, 10);

void setup()
{
  // Open serial communications and wait for port to open:
  SerialUSB.begin(115200);

  // Start each software serial port
  portOne.begin(9600);
  portTwo.begin(9600);
}

void loop()
{
  // By default, the last intialized port is listening.
  // when you want to listen on a port, explicitly select it:
  portOne.listen();

  SerialUSB.println("Data from port one:");
  // while there is data coming in, read it
  // and send to the hardware serial port:
  while (portOne.available() > 0) {
    char inByte = portOne.read();
    SerialUSB.write(inByte);
  }

  // blank line to separate data from the two ports:
  SerialUSB.println("");

  // Now listen on the second port
  portTwo.listen();
  // while there is data coming in, read it
  // and send to the hardware serial port:

  SerialUSB.println("Data from port two:");
  while (portTwo.available() > 0) {
    char inByte = portTwo.read();
    SerialUSB.write(inByte);
  }

  // blank line to separate data from the two ports:
  SerialUSB.println();
}

