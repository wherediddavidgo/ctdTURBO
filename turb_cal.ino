#include <RTClib.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_VCNL4010.h>

const DateTime uploadDT = DateTime((__DATE__), (__TIME__));
RTC_DS3231 rtc;
DateTime startDT = uploadDT;

const int chipSelect = 4;
char datalogFileName[12];

File dataFile;

Adafruit_VCNL4010 vcnl;
int turb_am;
int turb_pr;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Initializing SD card");

  while (!SD.begin(chipSelect)) {
    Serial.println("Card failed or not present");
    delay(1000);
  }

  SdFile::dateTimeCallback(SDCardDateTimeCallback);

  Serial.println("SD card initialized");
  delay(1000);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }

  get_numbered_filename(datalogFileName, (char*)"CAL", (char*)"CSV");

  dataFile = SD.open(datalogFileName, FILE_WRITE);

  if (dataFile) {
    Serial.println("===============================================");
    Serial.println("Date,Time,Ambient,Proximity");
    dataFile.println("Date,Time,Ambient,Proximity");
    dataFile.close();
  } else {
    Serial.println("Error: cannot open datalog");
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power. Setting time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime now = rtc.now();
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
  delay(250);

  vcnl.begin();

  Serial.print("Writing to datalog: ");
  Serial.println(datalogFileName);

  Serial.println("--- Starting Datalogging ---");
}

void loop() {
  // put your main code here, to run repeatedly:
  turb_am = vcnl.readAmbient();
  turb_pr = vcnl.readProximity();

  Serial.print(dateTimeString);
  Serial.print(",");
  Serial.print(turb_am);
  Serial.print(",");
  Serial.print(turb_pr);

  dataFile = SD.open(datalogFileName, FILE_WRITE);
  if (dataFile) {
    dataFile.print(dateTimeString);
    dataFile.print(",");
    dataFile.print(turb_am);
    dataFile.print(",");
    dataFile.print(turb_pr);
    dataFile.close();
  }
}

void SDCardDateTimeCallback(uint16_t* date, uint16_t* time) {
  Serial.println("Setting date/time");
  DateTime now = rtc.now();
  *date = FAT_DATE(now.year(), now.month(), now.day());
  *time = FAT_TIME(now.hour(), now.minute(), now.second());
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
  Serial.println("date time string");
  //  }
  // outputs the date as a date time string,
  sprintf(outStr, "%02d/%02d/%02d,%02d:%02d:%02d", date.month(), date.day(), date.year(), date.hour(), date.minute(), date.second());
  // Note: If you would like the date & time to be seperate columns chabge the space in the formatting string to a comma - this works because the file type is CSV (Comma Seperated Values)
}