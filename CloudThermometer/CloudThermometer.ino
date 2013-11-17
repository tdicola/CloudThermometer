/***************************************************
  Cloud Data-Logging Thermometer
 
  Log a thermomistor value to an Amazon DynamoDB table every minute.
  
  Copyright 2013 Tony DiCola (tony@tonydicola.com).
  Released under an MIT license: 
    http://opensource.org/licenses/MIT

  Dependencies:
  - Adafruit CC3000 Library 
    https://github.com/adafruit/Adafruit_CC3000_Library
  - RTClib Library
    https://github.com/adafruit/RTClib
  
  Parts of this code were adapted from Adafruit CC3000 library example 
  code which has the following license:
  
  Designed specifically to work with the Adafruit WiFi products:
  ----> https://www.adafruit.com/products/1469

  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried & Kevin Townsend for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
  
  SHA256 hash and signing code adapted from Peter Knight's library
  available at https://github.com/Cathedrow/Cryptosuite
 ****************************************************/

#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include "sha256.h"

// CC3000 configuration
#define     ADAFRUIT_CC3000_IRQ    3    // MUST be an interrupt pin!
#define     ADAFRUIT_CC3000_VBAT   5    // VBAT and CS can be any two pins
#define     ADAFRUIT_CC3000_CS     10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, 
                                         ADAFRUIT_CC3000_IRQ, 
                                         ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIV2);

// Wireless network configuration
#define     WLAN_SSID              "myNetwork"      // cannot be longer than 32 characters!
#define     WLAN_PASS              "myPassword"
#define     WLAN_SECURITY          WLAN_SEC_WPA2  // Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2

// Thermistor configuration
#define     THERMISTOR_PIN         3       // Analog pin to read thermistor values.
#define     SERIES_RESISTOR        10000   // Resistor value (in Ohms) in series with the thermistor.
#define     ADC_SAMPLES            5       // Number of ADC samples to average for a reading.

// Change the thermistor coefficient values below to match what you measured
// after running through the calibration sketch.
#define     A_COEFFICIENT          0
#define     B_COEFFICIENT          0
#define     C_COEFFICIENT          0

// DynamoDB table configuration
#define     TABLE_NAME             "Temperatures"  // The name of the table to write results.
#define     ID_VALUE               "Test"          // The value for the ID/primary key of the table.  Change this to a 
                                                   // different value each time you start a new measurement.

// Amazon AWS configuration
#define     AWS_ACCESS_KEY         "your_AWS_access_key"                         // Put your AWS access key here.  
                                                                                 // Don't put the read-only user credentials here, instead use your AWS account credentials
                                                                                 // or the credentials of an account with write access to your DynamoDB table here.
                                                                                 
#define     AWS_SECRET_ACCESS_KEY  "your_AWS_secret_access_key"                  // Put your AWS secret access key here.
                                                                                 // Don't put the read-only user credentials here, instead use your AWS account credentials
                                                                                 // or the credentials of an account with write access to your DynamoDB table here.

#define     AWS_REGION             "us-east-1"                                   // The region where your dynamo DB table lives.
                                                                                 // Copy the _exact_ region value from this table: http://docs.aws.amazon.com/general/latest/gr/rande.html#ddb_region 

#define     AWS_HOST               "dynamodb.us-east-1.amazonaws.com"            // The endpoint host for where your dynamo DB table lives.
                                                                                 // Copy the _exact_ endpoint host from this table: http://docs.aws.amazon.com/general/latest/gr/rande.html#ddb_region 

// Other sketch configuration
#define     READING_DELAY_MINS     1      // Number of minutes to wait between readings.
#define     TIMEOUT_MS             15000  // How long to wait (in milliseconds) for a server connection to respond (for both AWS and NTP calls).

// Don't modify the below constants unless you want to play with calling other DynamoDB APIs
#define     AWS_TARGET             "DynamoDB_20120810.PutItem"
#define     AWS_SERVICE            "dynamodb"
#define     AWS_SIG_START          "AWS4" AWS_SECRET_ACCESS_KEY
#define     SHA256_HASH_LENGTH     32

// State used to keep track of the current time and time since last temp reading.
unsigned long lastPolledTime = 0;   // Last value retrieved from time server
unsigned long sketchTime = 0;       // CPU milliseconds since last server query
unsigned long lastReading = 0;      // Time of last temperature reading.

void setup(void) {
  Serial.begin(115200);
  
  // Initialize and connect to the wireless network
  // This code is adapted from CC3000 example code.
  if (!cc3000.begin()) {
    Serial.println(F("Unable to initialise the CC3000!"));
    while(1);
  }
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed to connect to AP!"));
    while(1);
  }
  // Wait for DHCP to complete
  while (!cc3000.checkDHCP()) {
    delay(100);
  }
  
  // Get an initial time value by querying an NTP server.
  unsigned long t = getTime();
  while (t == 0) {
    // Failed to get time, try again in a minute.
    delay(60*1000);
    t = getTime();
  }
  lastPolledTime = t;
  sketchTime = millis();
  
  Serial.println(F("Running..."));
}

void loop(void) {
  // Update the current time.
  // Note: If the sketch will run for more than ~24 hours, you probably want to query the time
  // server again to keep the current time from getting too skewed.
  unsigned long currentTime = lastPolledTime + (millis() - sketchTime) / 1000;
  
  if ((currentTime - lastReading) >= (READING_DELAY_MINS*60)) {
    lastReading = currentTime;

    // Get a temp reading
    float currentTemp = readTemp();
  
    // Write the result to the database.
    dynamoDBWrite(TABLE_NAME, ID_VALUE, currentTime, currentTemp);
  }
}

// Take a reading of the thermistor and return the current temp in kelvin.
float readTemp() {
  float R = 0;
  for (int i = 0; i < ADC_SAMPLES; ++i) {
    R += analogRead(THERMISTOR_PIN);
  }
  R /= (float)ADC_SAMPLES;
  R = (1023 / R) - 1;
  R = SERIES_RESISTOR / R;
  return 1/(A_COEFFICIENT + B_COEFFICIENT*log(R) + C_COEFFICIENT*pow(log(R), 3));
}

// Write a temperature reading to the DynamoDB table.
void dynamoDBWrite(char* table, char* id, unsigned long currentTime, float currentTemp) {
  // Generate time and date strings
  DateTime dt(currentTime);
  // Set dateTime to the ISO8601 simple date format string.
  char dateTime[17];
  memset(dateTime, 0, 17);
  dateTime8601(dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second(), dateTime);
  // Set date to just the year month and day of the ISO8601 simple date string.
  char date[9];
  memset(date, 0, 9);
  memcpy(date, dateTime, 8);
  // Set currentTimeStr to the string value of the current unix time (seconds since epoch).
  char currentTimeStr[8*sizeof(unsigned long)+1];
  memset(currentTimeStr, 0, 8*sizeof(unsigned long)+1);
  ultoa(currentTime, currentTimeStr, 10);

  // Generate string for the temperature reading.
  char temp[8*sizeof(unsigned long)+5];
  memset(temp, 0, 8*sizeof(unsigned long)+5);
  // Convert to fixed point string.  Using a proper float to string function
  // like dtostrf takes too much program memory (~1.5kb) to use in this sketch.
  ultoa((unsigned long)currentTemp, temp, 10);
  int n = strlen(temp);
  temp[n] = '.';
  temp[n+1] = '0' + ((unsigned long)(currentTemp*10)) % 10;
  temp[n+2] = '0' + ((unsigned long)(currentTemp*100)) % 10;
  temp[n+3] = '0' + ((unsigned long)(currentTemp*1000)) % 10;

  // Generate string with payload length for use in the signing and request sending.  
  char payloadlen[8*sizeof(unsigned long)+1];
  memset(payloadlen, 0, 8*sizeof(unsigned long)+1);
  ultoa(71+strlen(table)+strlen(id)+strlen(currentTimeStr)+strlen(temp), payloadlen, 10);

  // Generate the signature for the request.
  // For details on the AWS signature process, see: 
  //   http://docs.aws.amazon.com/general/latest/gr/signature-version-4.html

  // First, generate signing key to use in later signature generation.
  // Note: This could be optimized to generate just once per day (when the date value changes),
  // but since calls are only made every few minutes it's simpler to regenerate each time.
  char signingkey[SHA256_HASH_LENGTH];
  Sha256.initHmac((uint8_t*)AWS_SIG_START, strlen(AWS_SIG_START));
  Sha256.print(date);
  memcpy(signingkey, Sha256.resultHmac(), SHA256_HASH_LENGTH);
  Sha256.initHmac((uint8_t*)signingkey, SHA256_HASH_LENGTH);
  Sha256.print(AWS_REGION);
  memcpy(signingkey, Sha256.resultHmac(), SHA256_HASH_LENGTH);
  Sha256.initHmac((uint8_t*)signingkey, SHA256_HASH_LENGTH);
  Sha256.print(AWS_SERVICE);
  memcpy(signingkey, Sha256.resultHmac(), SHA256_HASH_LENGTH);
  Sha256.initHmac((uint8_t*)signingkey, SHA256_HASH_LENGTH);
  Sha256.print(F("aws4_request"));
  memcpy(signingkey, Sha256.resultHmac(), SHA256_HASH_LENGTH);
  
  // Second, generate hash of the payload data.
  Sha256.init();
  Sha256.print(F("{\"TableName\":\""));
  Sha256.print(table);
  Sha256.print(F("\",\"Item\":{\"Id\":{\"S\":\""));
  Sha256.print(id);
  Sha256.print(F("\"},\"Date\":{\"N\":\""));
  Sha256.print(currentTimeStr);
  Sha256.print(F("\"},\"Temp\":{\"N\":\""));
  Sha256.print(temp);
  Sha256.print(F("\"}}}"));
  char payloadhash[2*SHA256_HASH_LENGTH+1];
  memset(payloadhash, 0, 2*SHA256_HASH_LENGTH+1);
  hexString(Sha256.result(), SHA256_HASH_LENGTH, payloadhash);

  // Third, generate hash of the canonical request.
  Sha256.init();
  Sha256.print(F("POST\n/\n\ncontent-length:"));
  Sha256.print(payloadlen);
  Sha256.print(F("\ncontent-type:application/x-amz-json-1.0\nhost:"));
  Sha256.print(AWS_HOST);
  Sha256.print(F(";\nx-amz-date:"));
  Sha256.print(dateTime);
  Sha256.print(F("\nx-amz-target:"));
  Sha256.print(AWS_TARGET);
  Sha256.print(F("\n\ncontent-length;content-type;host;x-amz-date;x-amz-target\n"));
  Sha256.print(payloadhash);  
  char canonicalhash[2*SHA256_HASH_LENGTH+1];
  memset(canonicalhash, 0, 2*SHA256_HASH_LENGTH+1);
  hexString(Sha256.result(), SHA256_HASH_LENGTH, canonicalhash);
  
  // Finally, generate request signature from the string to sign and signing key.
  Sha256.initHmac((uint8_t*)signingkey, SHA256_HASH_LENGTH);
  Sha256.print(F("AWS4-HMAC-SHA256\n"));
  Sha256.print(dateTime);
  Sha256.print(F("\n"));
  Sha256.print(date);
  Sha256.print(F("/"));
  Sha256.print(AWS_REGION);
  Sha256.print(F("/"));
  Sha256.print(AWS_SERVICE);
  Sha256.print(F("/aws4_request\n"));
  Sha256.print(canonicalhash);
  char signature[2*SHA256_HASH_LENGTH+1];
  memset(signature, 0, 2*SHA256_HASH_LENGTH+1);
  hexString(Sha256.resultHmac(), SHA256_HASH_LENGTH, signature);
  
  // Make request to DynamoDB API.
  uint32_t ip = 0;
  while (ip == 0) {
    if (!cc3000.getHostByName(AWS_HOST, &ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
  }
  Adafruit_CC3000_Client www = cc3000.connectTCP(ip, 80);
  if (www.connected()) {
    www.fastrprint(F("POST / HTTP/1.1\r\nhost: "));
    www.fastrprint(AWS_HOST);
    www.fastrprint(F(";\r\nx-amz-date: "));
    www.fastrprint(dateTime);
    www.fastrprint(F("\r\nAuthorization: AWS4-HMAC-SHA256 Credential="));
    www.fastrprint(AWS_ACCESS_KEY);
    www.fastrprint(F("/"));
    www.fastrprint(date);
    www.fastrprint(F("/"));
    www.fastrprint(AWS_REGION);
    www.fastrprint(F("/"));
    www.fastrprint(AWS_SERVICE);
    www.fastrprint(F("/aws4_request, SignedHeaders=content-length;content-type;host;x-amz-date;x-amz-target, Signature="));
    www.fastrprint(signature);
    www.fastrprint(F("\r\ncontent-type: application/x-amz-json-1.0\r\ncontent-length: "));
    www.fastrprint(payloadlen);
    www.fastrprint(F("\r\nx-amz-target: "));
    www.fastrprint(AWS_TARGET);
    www.fastrprint(F("\r\n\r\n{\"TableName\":\""));
    www.fastrprint(table);
    www.fastrprint(F("\",\"Item\":{\"Id\":{\"S\":\""));
    www.fastrprint(id);
    www.fastrprint(F("\"},\"Date\":{\"N\":\""));
    www.fastrprint(currentTimeStr);
    www.fastrprint(F("\"},\"Temp\":{\"N\":\""));
    www.fastrprint(temp);
    www.fastrprint(F("\"}}}"));
  } 
  else {
    Serial.println(F("Connection failed"));    
    www.close();
    return;
  }
  
  // Read data until either the connection is closed, or the idle timeout is reached.
  Serial.println(F("AWS response:"));
  unsigned long lastRead = millis();
  while (www.connected() && (millis() - lastRead < TIMEOUT_MS)) {
    while (www.available()) {
      char c = www.read();
      Serial.print(c);
      lastRead = millis();
    }
  }
  www.close();
}

// Convert an array of bytes into a lower case hex string.
// Buffer MUST be two times the length of the input bytes array!
void hexString(uint8_t* bytes, size_t len, char* buffer) {
  for (int i = 0; i < len; ++i) {
    btoa2Padded(bytes[i], &buffer[i*2], 16);
  }
}

// Fill a 16 character buffer with the date in ISO8601 simple format, like '20130101T010101Z'.  
// Buffer MUST be at least 16 characters long!
void dateTime8601(int year, byte month, byte day, byte hour, byte minute, byte seconds, char* buffer) {
  ultoa(year, buffer, 10);
  btoa2Padded(month, buffer+4, 10);
  btoa2Padded(day, buffer+6, 10);
  buffer[8] = 'T';
  btoa2Padded(hour, buffer+9, 10);
  btoa2Padded(minute, buffer+11, 10);
  btoa2Padded(seconds, buffer+13, 10);
  buffer[15] = 'Z';
}

// Print a value from 0-99 to a 2 character 0 padded character buffer.
// Buffer MUST be at least 2 characters long!
void btoa2Padded(uint8_t value, char* buffer, int base) {
  if (value < base) {
    *buffer = '0';
    ultoa(value, buffer+1, base);
  }
  else {
    ultoa(value, buffer, base); 
  }
}

// getTime function adapted from CC3000 ntpTest sketch.
// Minimalist time server query; adapted from Adafruit Gutenbird sketch,
// which in turn has roots in Arduino UdpNTPClient tutorial.
unsigned long getTime(void) {
  Adafruit_CC3000_Client client;
  uint8_t       buf[48];
  unsigned long ip, startTime, t = 0L;

  // Hostname to IP lookup; use NTP pool (rotates through servers)
  if(cc3000.getHostByName("pool.ntp.org", &ip)) {
    static const char PROGMEM
      timeReqA[] = { 227,  0,  6, 236 },
      timeReqB[] = {  49, 78, 49,  52 };

    startTime = millis();
    do {
      client = cc3000.connectUDP(ip, 123);
    } while((!client.connected()) &&
            ((millis() - startTime) < TIMEOUT_MS));

    if(client.connected()) {
      // Assemble and issue request packet
      memset(buf, 0, sizeof(buf));
      memcpy_P( buf    , timeReqA, sizeof(timeReqA));
      memcpy_P(&buf[12], timeReqB, sizeof(timeReqB));
      client.write(buf, sizeof(buf));

      memset(buf, 0, sizeof(buf));
      startTime = millis();
      while((!client.available()) &&
            ((millis() - startTime) < TIMEOUT_MS));
      if(client.available()) {
        client.read(buf, sizeof(buf));
        t = (((unsigned long)buf[40] << 24) |
             ((unsigned long)buf[41] << 16) |
             ((unsigned long)buf[42] <<  8) |
              (unsigned long)buf[43]) - 2208988800UL;
      }
      client.close();
    }
  }
  return t;
}


