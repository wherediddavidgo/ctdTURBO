//Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include <Wire.h>
#include <RTClib.h>
// #include <DS3231.h>
//#include <DateTime.h>
#include <Adafruit_SleepyDog.h>

//serial peripheral interface and library for SD card reader
#include <SPI.h>  //serial peripheral interface for SD card reader
#include <SD.h>   //library for SD card reader

//communication protocols for temperature sensors
#include <OneWire.h>
#include <DallasTemperature.h>

//Software libary for the pressure sensor
#include <MS5803_14.h>
//#include <SparkFun_MS5803_I2C.h>


//EC and O2 Circuits use software serial
#include <SoftwareSerial.h>

// turbidity library
#include <Adafruit_VCNL4010.h>


const DateTime uploadDT = DateTime((__DATE__), (__TIME__));
RTC_DS3231 rtc;  //define real-time clock
//DateTime nextAlarm;
//long startDTE = 1686860940;  //Eastern
//long startDT = startDTE - 14400;
//DateTime startDTE = (2023, 6, 19, 13, 45, 0);
////DateTime startDTE.year() = 2023;
////DateTime startDTE.month() = 6;
////DateTime startDTE.date() = 19;
////DateTime startDTE.hour() = 13;
////DateTime startDTE.minute() = 45;
////DateTime startDTE.second() = 0;
//
//DateTime startDT = DateTime(startDTE.unixtime() - 14400);
DateTime startDT = uploadDT;


//TimeSpan delayedStart;
//unsigned long millisTime;
// long currentTime = 0;
long delayedStart_seconds = 0;
long sleepDuration_seconds = 0;
long samples_per_wake = 5;
long start_dt;

const int chipSelect = 4;  //sets chip select pin for SD card reader
char datalogFileName[12];

File dataFile;

/* This integer specifies how high accuracy you want your pressure sensor to be (oversampling resolution).
   Ok values: 256, 512, 1024, 2048, or 4096 (Higher = more accuracy but slower sampling frequency**)
 * ** There is no reason not to use the highest accuracy. This is because the datalogging rate is set by the
   sampling/response frequency of the ec sensor [default = 1 sec] (this is to avoid the case where both sensors send data at the same time). */

int PRESSURE_SENSOR_RESOLUTION = 4096;
MS_5803 p_sensor = MS_5803(PRESSURE_SENSOR_RESOLUTION);  // Define pressure sensor.

OneWire oneWire(6);                     // Define the OneWire port for temperature.
DallasTemperature t_sensors(&oneWire);  //Define DallasTemperature input based on OneWire.

// Define the SoftwareSerial port for conductivity.
SoftwareSerial ecSerial(12, 13);
SoftwareSerial oSerial(10, 11);  //SoftwareSerial port for DO based on code for EC

//int s = 0;

Adafruit_VCNL4010 vcnl;  //turbidity sensor
int turb_am;
int turb_pr;
//struct {
//  uint32_t logTime;
//  uint32_t abs_P;
//  uint16_t tuAmbient;
//  uint16_t tuBackscatter;
//  int16_t water_temp;
//  uint16_t battery;
//} data; //16 bytes

union {
  byte b;
  struct {
    bool turb : 1;
  } module;
} startup;

double pressure_abs;  //define absolute pressure variable

/* This integer specifies how high resolution you want your temperature sensors to be.
   Ok values: 9,10,11,12 (Higher = more accuracy but slower sampling frequency**)
 * ** There is no reason not to use the highest accuracy. This is because the datalogging rate is set by the
   sampling/response frequency of the ec sensor [default = 1 second] (this is to avoid the case where both sensors send data at the same time). */

#define TEMP_SENSOR_RESOLUTION 12

//Declare global temperature variables.
float tempA;
// float tempB;
// float tempC;
int tempADelayStartTime;  // Define a variable to mark when we requested a temperature mesurement from A so we can wait the required delay before reading the value.
// int tempBDelayStartTime;  // Define a variable to mark when we requested a temperature mesurement from B so we can wait the required delay before reading the value.
// int tempCDelayStartTime;  // Define a variable to mark when we requested a temperature mesurement from C so we can wait the required delay before reading the value.
int requiredMesurementDelay = t_sensors.millisToWaitForConversion(TEMP_SENSOR_RESOLUTION);

//Declare global variables for eletrical conductivity
float EC_float = 0;
char EC_init_data[48];      // A 48 byte character array to hold incoming data from the conductivity circuit.
char* EC;                   // Character pointer for string parsing.
byte received_from_ec = 0;  // How many characters have been received.
byte ec_received = 0;       // Whether it received a string from the EC circuit.
char EC_data[48] = {};



byte received_from_o = 0;
char DO_init_data[48];
char* DO;
char DO_data[48] = {};


char r[15];

#define EC_SAMPLING_FREQUENCY 1  // Set the requested sampling frequency of the conductivity probe in seconds (NO Decimals) (this by extension sets the overall frequency of logging).

String o_in_string = "";
String o_sen_string = "";
boolean o_in_string_complete = false;
boolean o_sen_string_complete = false;



//void parse_data() { // Parses data from the EC Circuit.
//  Serial.println("parsing data");
//  EC = strtok(EC_data, ",");
//
//}


void setup() {
  // while (!Serial) {
  // }
  Serial.begin(9600);
  Serial.println("setup");
  // Serial.println(u)
  int b = 0;

  // while (b < 10) {
  // blink(500, 500);
  // b++;
  // }
  // comment the following three lines out for final deployment
  // #ifndef ESP8266
  //   while (!Serial && millis() < 20000)
  //     ;  //for Leonardo/Micro/Zero - Wait for a computer to connect via serial or until a 20 second timeout has elapsed (This works because millis() starts counting the mlliseconds since the board turns on)
  // #endif



  // if (Serial.available() > 0) {
  //   newSettings();
  // }

  //Initialize SD card reader
  Serial.print("Initializing SD card...");

  while (!SD.begin(chipSelect)) {

    Serial.println("Card failed, or not present");
    delay(1000);
  }

  // This funny function allows the sd-library to set the correct file created & modified dates for all sd card files.
  // (See the SDCardDateTimeCallback function defined at the end of this file)
  SdFile::dateTimeCallback(SDCardDateTimeCallback);

  Serial.println("card initialized.");
  // digitalWrite(LED_BUILTIN, HIGH);

  delay(1000);

  if (!rtc.begin()) {

    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }

  get_numbered_filename(datalogFileName, (char*)"LOG", (char*)"CSV");

  Serial.print("Writing to datalog: ");
  Serial.println(datalogFileName);

  dataFile = SD.open(datalogFileName, FILE_WRITE);

  if (dataFile) {
    // dataFile.print("delay;sleep_interval");
    // dataFile.print(delayedStart_seconds);
    // dataFile.print(";");
    // dataFile.println(sleepDuration_seconds);
    Serial.println("====================================================");
    Serial.println("Date,Time,Pressure,Temp,Conductivity,Ambient,Proximity,DO");
    dataFile.println("If found, contact dego@email.unc.edu");
    dataFile.println("Date,Time,Pressure,Temp,Conductivity,Ambient,Proximity,DO");
    dataFile.close();

  } else {
    Serial.println("Err: Can't open datalog!");
  }



  // if (uploadDT.unixtime() != startDT.unixtime()) {
  //  delayedStart_seconds = startDT - uploadDT.unixtime();
  //  Serial.print("StartDT: "); Serial.println(startDT);
  ////  Serial.println(startDT.year(), DEC);
  ////  Serial.println(startDT.month(), DEC);
  ////  Serial.println(startDT.date(), DEC);
  ////  Serial.println(startDT.hour(), DEC);
  ////  Serial.println(startDT.minute(), DEC);
  ////  Serial.println(startDT.second(), DEC);
  //  Serial.print("uploadDT: "); Serial.println(uploadDT.unixtime());
  //  Serial.print("Starting in "); Serial.print(delayedStart_seconds); Serial.println(" seconds");
  //  //  }

  //Initialize sensors
  //initialize turbidity
  //  startup.module.turb = vcnl.begin();
  //  vcnl.setLEDcurrent(5);
  //  if (!startup.module.turb) {
  //    serialSend("TURBINIT,0");
  // }
  vcnl.begin();

  Serial.println("-- Pressure Sensor Info: --");
  p_sensor.initializeMS_5803();  // Initialize pressure sensor
  Serial.println("---------------------------");

  t_sensors.begin();  // Intialize the temperature sensors.
  Serial.println("temp init");
  t_sensors.setResolution(TEMP_SENSOR_RESOLUTION);  // Set the resolution (accuracy) of the temperature sensors.
  t_sensors.requestTemperatures();                  // on the first pass request all temperatures in a blocking way to start the variables with true data.
  tempA = get_temp_c_by_index(0);
  // tempB = get_temp_c_by_index(1);
  // tempC = get_temp_c_by_index(2);
  // Serial.println("get temp by index");
  t_sensors.setWaitForConversion(false);  // Now tell the Dallas Temperature library to not block this script while it's waiting for the temperature mesurement to happen
  // Serial.println("got conversion");




  ecSerial.begin(9600);  // Set baud rate for conductivity circuit.
  //    int e = 0;
  do {
    // Serial.println("do ec block");
    ecSerial.write('i');                                                 // Tell electrical conductivity board to reply with the board information by sending the 'i' character ...
    ecSerial.write('\r');                                                // ... Finish the command with the charage return character.
    received_from_ec = ecSerial.readBytesUntil('\r', EC_init_data, 30);  // Wait for the ec circut to send the data before moving on...
    //    received_from_ec = ecSerial.readBytes(EC_data, 30);
    Serial.print("received from ec: ");
    Serial.println(received_from_ec);
    Serial.print("EC data: ");
    Serial.println(EC_init_data);
    EC_init_data[received_from_ec] = 0;  // Null terminate the data by setting the value after the final character to 0.
    Serial.println();
    Serial.println();
    Serial.print("received from ec: ");
    Serial.println(received_from_ec);
    Serial.print("EC data: ");
    Serial.println(EC_init_data);
    Serial.println();
    Serial.println();
    Serial.println();

    //    e = 1;
  } while (EC_init_data[3] != 'E');  // Keep looping until the ecSerial has sent the board info string (also indicating it has booted up, I think...)
  //    } while (e != 1); // just making the block only run once for testing
  Serial.print("EC Board Info (Format: ?I,[board type],[Firmware Version]) -> ");
  Serial.println(EC_init_data);

  delay(10);
  ecSerial.write('C');   // Tell electrical conductivity board to continously ("C") transmit mesurements ...
  ecSerial.write(',');   //
  ecSerial.write('0');   // ... every x seconds (here x is the EC_SAMPLING_FREQUENCY variable)
  ecSerial.write('\r');  // Finish the command with the carrage return character.

  received_from_ec = ecSerial.readBytesUntil('\r', EC_init_data, 10);  // keep reading the reply until the return character is recived (or it gets to be 10 characters long, which shouldn't happen)
  EC_init_data[received_from_ec] = 0;                                  // Null terminate the data by setting the value after the final character to 0.
  ecSerial.end();
  delay(10);


  oSerial.begin(9600);
  //  //  int o = 0;
  do {
    // Serial.println("do o block");
    oSerial.write('i');                                                // Tell electrical conductivity board to reply with the board information by sending the 'i' character ...
    oSerial.write('\r');                                               // ... Finish the command with the charage return character.
    received_from_o = oSerial.readBytesUntil('\r', DO_init_data, 30);  // Wait for the ec circut to send the data before moving on...
    DO_init_data[received_from_o] = 0;                                 // Null terminate the data by setting the value after the final character to 0.
    //    //    o ++;
    //    //    o = 11;
    Serial.println();
    Serial.println();
    Serial.print("received from o: ");
    Serial.println(received_from_o);
    Serial.print("o data: ");
    Serial.println(DO_init_data);
    Serial.println();
    Serial.println();
    Serial.println();
  } while (DO_init_data[3] != 'D');  // Keep looping until the ecSerial has sent the board info string (also indicating it has booted up, I think...)
  //  //  } while (o < 10);
  Serial.print("DO Board Info (Format: ?I, [board type], [Firmware Version]) -> ");
  Serial.println(DO_init_data);
  //
  delay(10);
  //
  oSerial.write('C');
  oSerial.write(',');
  oSerial.write('0');
  oSerial.write('\r');
  received_from_o = oSerial.readBytesUntil('\r', DO_init_data, 10);
  DO_init_data[received_from_o] = 0;
  //
  delay(10);

  Serial.print("EC Frequency Set Sucessfully? -> ");
  Serial.println(EC_init_data);
  Serial.print("DO Frequency Set Successfully? -> ");
  Serial.println(DO_init_data);

  //  oSerial.begin(9600);
  //  oSerial.write('C');
  //  o_in_string.reserve(10);
  //  o_sen_string.reserve(30);

  //Initialize real-time clock
 //if (rtc.lostPower()) {

    //reset RTC with time when code was compiled if RTC loses power
    Serial.println("Lets set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //}
  DateTime now = rtc.now() + 21;
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
  delay(250);  // Wait a quarter second to continue.
  Serial.print("Writing to datalog: ");
  Serial.println(datalogFileName);

  Serial.println("--- Starting Datalogging ---");
  //  if (delayedStart_seconds > 0) {
  //    nextAlarm = DateTime(now.unixtime() + delayedStart_seconds);
  //    rtc.enableAlarm(nextAlarm);
  //    setBBSQW(); //enable battery-backed alarm
  //    //    serialSend("POWEROFF,1");
  //    delay(100); //ensure the alarm is set
  //    rtc.clearAlarm(); //turn off battery
  //    delay(delayedStart_seconds * 1000); //delay program if we have another power source
  //  }
  if (delayedStart_seconds != 0) {
    delay(500);
    for (int s = 0; s < delayedStart_seconds / 20; s++) {
      Watchdog.sleep(1000 * delayedStart_seconds);
      //     //    long timeUntilAlarm = nextAlarm.unixtime() - rtc.now().unixtime();
      //     //    if (timeUntilAlarm > 5) {
      //     //      delay(1000); //give the SD card enough time to close the file and reshuffle data.
      //     //      //    serialSend("POWEROFF,1");
      //     //      rtc.clearAlarm(); //turn off battery
      //     //      //mimic power off when provided USB power
      //     //      delay((sleepDuration_seconds - timeUntilAlarm) * 1000);
    }
  }
  // digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  if (sleepDuration_seconds != 0) {
    for (int L = 0; L < samples_per_wake; L++) {
      sample();
    }

    delay(100);
    for (int s = 0; s < sleepDuration_seconds / 20; s++) {
      Watchdog.sleep(1000 * sleepDuration_seconds);
    }
  } else {
    sample();
  }
}
//  EC = 0;


// Tip: For a slower overall logging frequency, set the EC_SAMPLING_FREQUENCY variable rather than adding a delay (this will avoid the possibility of garbled ec sensor readings)


void SDCardDateTimeCallback(uint16_t* date, uint16_t* time)  // This funny function allows the sd-library to set the correct file created & modified dates for all sd card files (As would show up in the file explorer on your computer)
{
  //  Serial.println("setting datetime");
  DateTime now = rtc.now() + 21;
  *date = FAT_DATE(now.year(), now.month(), now.day());
  *time = FAT_TIME(now.hour(), now.minute(), now.second());
}

//enable alarm on battery power. Normally disabled
// void setBBSQW() {
//   uint8_t ctReg = rtc.read_register(DS3231_CONTROL_REG);
//   ctReg |= 0b01000000;
//   rtc.writeRegister(DS3231_CONTROL_REG, ctReg);
//   Serial.println("poopy");
// }


//void SerialEvent() {
//  o_in_string = oSerial.readStringUntil(13);
//  o_in_string_complete = true;
//}

void newSettings() {
  bool set = false;  //checks whether new settings have been updated
  char* r_a;         //received, abridged
  while (!set) {
    String temp = Serial.readString();
    temp.toCharArray(r, 15);
    if (r[0] == 'c') {
      Serial.println("Connected");
      set = false;
    } else if (r[0] == 'p') {
      r_a = r + 1;  // p character before number indicates this is sleep duration, r_a doesn't have this character
      sleepDuration_seconds = atoi(r_a);
      Serial.print("Sleep duration set to ");
      Serial.print(sleepDuration_seconds / 60);
      Serial.print(" minutes and ");
      Serial.print(sleepDuration_seconds % 60);
      Serial.println(" seconds");
      set = false;
    } else if (r[0] == 'd') {  // d indicates delayed start time. unix time
      r_a = r + 1;
      delayedStart_seconds = atoi(r_a);
      // delayedStart_seconds = start_dt - int(rtc.now().unixtime());
      Serial.print("Starting in ");
      Serial.print(delayedStart_seconds);
      Serial.println(" seconds.");
    } else if (r[0] == 'e') {
      Serial.println("Settings confirmed. Sensor disconnected.");
      set = true;
    }
  }
}

void get_numbered_filename(char* outStr, char* filePrefix, char* fileExtension) {
  Serial.println("get numbered filename");
  // Make base filename
  sprintf(outStr, "%s000.%s", filePrefix, fileExtension);
  int namelength = strlen(outStr);
  if (namelength > 12) Serial.println("Error: filename too long. Shorten your filename to < 5 characters (12 chars total w number & file extension) !");

  // Keep incrementing the number part of filename until we reach an unused filename
  int i = 1;
  while (SD.exists(outStr)) {  // keep looping if filename already exists on card. [If the filename doesn't exist, the loop exits, so we found our first unused filename!]

    int hundreds = i / 100;
    outStr[namelength - 7] = '0' + hundreds;
    outStr[namelength - 6] = '0' + (i / 10) - (hundreds * 10);
    outStr[namelength - 5] = '0' + i % 10;
    i++;
  }
}

void get_date_time_string(char* outStr, DateTime date) {
  //  if (s < 10) {
  // Serial.println("date time string");
  //  }
  // outputs the date as a date time string,
  sprintf(outStr, "%02d/%02d/%02d,%02d:%02d:%02d", date.month(), date.day(), date.year(), date.hour(), date.minute(), date.second());
  // Note: If you would like the date & time to be seperate columns chabge the space in the formatting string to a comma - this works because the file type is CSV (Comma Seperated Values)
}


float get_temp_c_by_index(int sensor_index) {

  float value = t_sensors.getTempCByIndex(sensor_index);
  if (value == DEVICE_DISCONNECTED_C) {
    return NAN;  // Return Not a Number (NAN) to indicate temperature probe has error or is disconnected.
  } else {
    return value;  // otherwise return the measured value.
  }
}

void blink(int on, int off) {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(on);
  digitalWrite(LED_BUILTIN, LOW);
}

void sample() {
  // blink(500, 500);
  // EC_data[48] = {};
  // DO_data[48] = {};
  // EC[48] = { '*' };
  // if (ecSerial.available() > 0) {

  //   received_from_ec = ecSerial.readBytesUntil(13, EC_data, 48);
  //   EC_data[received_from_ec] = 0;  // Null terminate the data by setting the value after the final character to 0.

  //   if ((EC_data[0] >= 48) && (EC_data[0] <= 57)) {  // Parse data, if EC_data begins with a digit, not a letter (testing ASCII values).

  //     EC = strtok(EC_data, ",");
  //   }
  // }
  // // }
  ecSerial.begin(9600);
  ecSerial.write('R');
  ecSerial.write('\r');
  received_from_ec = ecSerial.readBytesUntil('\r', EC_data, 48);
  EC = strtok(EC_data, ",");
  ecSerial.end();
  delay(500);
  // //Read electrical conductivity sensor
  oSerial.begin(9600);
  // if (oSerial.available() > 0) {
  //  Serial.println("oSerial available > 0");
  oSerial.write('R');
  oSerial.write('\r');
  received_from_o = oSerial.readBytesUntil('\r', DO_data, 48);
  DO = strtok(DO_data, ",");
  // }
  oSerial.end();
  // Read pressure sensor
  p_sensor.readSensor();
  pressure_abs = p_sensor.pressure();

  // Read the temperature sensors.
  if (millis() - tempADelayStartTime > requiredMesurementDelay) {  // wait for conversion to happen before attempting to read temp probe A's value;
    tempA = get_temp_c_by_index(0);
    t_sensors.requestTemperaturesByIndex(0);  // request temp sensor A start mesuring so it can be read on the following loop (if enough time elapses).
    tempADelayStartTime = millis();           // mark when we made the request to make sure we wait long enough before reading it.
  }

  turb_am = vcnl.readAmbient();
  turb_pr = vcnl.readProximity();

  DateTime now = rtc.now() + 21;  //check RTC
  char dateTimeString[40];
  get_date_time_string(dateTimeString, now);

  //output readings to serial
  if (Serial) {
    Serial.print(dateTimeString);
    Serial.print(",");
    Serial.print(pressure_abs);
    Serial.print(",");
    Serial.print(tempA);
    Serial.print(",");
    // Serial.print(tempB);
    // Serial.print(",");
    // Serial.print(tempC);
    // Serial.print(",");
    Serial.print(EC_data);
    Serial.print(",");
    Serial.print(turb_am);
    Serial.print(",");
    Serial.print(turb_pr);
    Serial.print(",");
    Serial.println(DO_data);
    //    s ++;
  }
  //output readings to data file.
  // SD.begin(chipSelect);
  dataFile = SD.open(datalogFileName, FILE_WRITE);
  if (dataFile) {
    // digitalWrite(5, HIGH);
    dataFile.print(dateTimeString);
    dataFile.print(",");
    dataFile.print(pressure_abs);
    dataFile.print(",");
    dataFile.print(tempA);
    dataFile.print(",");
    // dataFile.print(tempB);
    // dataFile.print(",");
    // dataFile.print(tempC);
    // dataFile.print(",");
    dataFile.print(EC_data);
    dataFile.print(",");
    dataFile.print(turb_am);
    dataFile.print(",");
    dataFile.print(turb_pr);
    dataFile.print(",");
    dataFile.println(DO_data);
    dataFile.close();
  }
  // digitalWrite(5, LOW);
}
