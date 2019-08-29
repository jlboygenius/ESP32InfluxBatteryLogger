
#include <time.h> 
#include <WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h> 
#include <Adafruit_GFX.h>
#include <InfluxDb.h>

// battery measurements
#define VCC 3850
#define BATTERY_PIN A0
#define R1 213800 // Resistor R1 Ohms
#define R2 21770 // Resistor R2 Ohms
#define CONNECT_WAIT_MILLIS 10000
#define LOW_THRESHOLD 12000 // milliVolts
#define ADC_SAMPLES 20
#define ADC_DISPERSED 2
#define SLEEP_SECS 14400 // 4 hour
//#define SLEEP_SECS 15

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */


// influxDB settings
#define INFLUXDB_HOST "1.1.1.1" // LOCAL IP of INFLUXDB server
 #define INFLUXDB_PORT "1337"
 #define INFLUXDB_DATABASE "CarPowerMonitor" // INFLUXDB name
 //if used with authentication
 #define INFLUXDB_USER "influxuser" // INFLUXDB USER
 #define INFLUXDB_PASS "influxpass" // INFLUXDB PASSWORD




const char* ssid     = "WIFISSID";
const char* password = "WIFIPASS";
 
const float DIV_RATIO = (R1 + R2) / (float) R2;
const char VOLTAGE_EVENT[11] = "carVoltage";
const char LOW_EVENT[14] = "carLowVoltage";
const char FLASH_REQUEST[13] = "flashRequest";
const char FLASH_READY[13] = "flashReady";

// set to true when the photon can sleep
volatile bool canSleep = false;
// set to true when photon is waiting for OTA update
volatile bool flashing = false;


uint64_t SLEEPTIME = SLEEP_SECS * uS_TO_S_FACTOR;  // not used


long timezone = 1; 
byte daysavetime = 1;


#define LOGO16_GLCD_HEIGHT 16 
#define LOGO16_GLCD_WIDTH  16 

#define SSD1306_128_64
 

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);


Influxdb influx(INFLUXDB_HOST);
  

int uint16Compare (const void *pa, const void *pb) {
    uint16_t a = *(const uint16_t *) pa;
    uint16_t b = *(const uint16_t *) pb;

    if (a < b) {
        return -1;
    } else if (a > b) {
        return 1;
    } else {
        return 0;
    }
}
uint16_t readMilliVolts(int pin) {
  return map(analogRead(pin), 0, 4095, 0, VCC);
}

// Averaging of N-X ADC samples as per ST Application Note AN4073
uint16_t getCentralAvgBatteryMilliVolts() {
    uint32_t mV = 0;
    uint16_t readings[ADC_SAMPLES];

    for (int i = 0; i < ADC_SAMPLES; i++) {
        readings[i] = readMilliVolts(BATTERY_PIN);
        delay(3);
    }

    qsort(readings, ADC_SAMPLES, sizeof(uint16_t), uint16Compare);

    for (int i = ADC_DISPERSED; i < (ADC_SAMPLES - ADC_DISPERSED); i++) {
        mV += readings[i];
    }

    return round(mV * DIV_RATIO / (float) (ADC_SAMPLES - (2 * ADC_DISPERSED)));
}

// Averaging of N ADC samples as per ST Application Note AN4073
uint16_t getAvgBatteryMilliVolts() {
    uint32_t mV = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        mV += readMilliVolts(BATTERY_PIN);
        delay(3);
    }
    return round(mV * DIV_RATIO / (float) ADC_SAMPLES);
}

uint16_t getBatteryMilliVolts() {
  uint16_t mV = readMilliVolts(BATTERY_PIN);
  return round(mV * DIV_RATIO);
}



void ReportPower(uint16_t  batteryMillivolts){
  InfluxData measurement ("Voltage");
  measurement.addTag("device", "CARSensor"); 
  measurement.addTag("sensor", "Voltage");
  measurement.addValue("value", batteryMillivolts);
  // write it into db
  influx.write(measurement);
}



void setup() {
  delay(1000);
  uint16_t  batteryMillivolts = getCentralAvgBatteryMilliVolts();
 
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    int i = 0;
        while (WiFi.status() != WL_CONNECTED && i<60) {
            digitalWrite(2,HIGH);
            delay(100);
            digitalWrite(2,LOW);
            delay(100);
            Serial.print(".");
            Serial.print(i);
            i++;

            
        }
        if(i==60) {
        // wifi didn't connect. shutdown and try later.
            esp_sleep_enable_timer_wakeup(SLEEP_SECS * uS_TO_S_FACTOR);
            esp_deep_sleep_start();  
        
        }

    Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.println("Contacting Time Server");
    configTime(3600*timezone, daysavetime*3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
    struct tm tmstruct ;
        delay(200);
        tmstruct.tm_year = 0;
        getLocalTime(&tmstruct, 5000);
    Serial.printf("\nNow is : %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct.tm_year)+1900,( tmstruct.tm_mon)+1, tmstruct.tm_mday,tmstruct.tm_hour , tmstruct.tm_min, tmstruct.tm_sec);
        Serial.println("");
        
        // Measure voltage before connecting to avoid ADC noise due to network activity
        //uint16_t  batteryMillivolts = getCentralAvgBatteryMilliVolts();
        Serial.printf("%.1fV", batteryMillivolts / 1000.0);


 // port defaults to 8086
 // or to use a custom port
 //Influxdb influx(INFLUXDB_HOST, INFLUXDB_PORT);

 // set the target database
 //influx.setDb(INFLUXDB_DATABASE);
 // or use a db with auth
    influx.setDbAuth(INFLUXDB_DATABASE, INFLUXDB_USER, INFLUXDB_PASS); // with authentication

    
    ReportPower(batteryMillivolts);
    esp_sleep_enable_timer_wakeup(14400000000);
    //esp_sleep_enable_timer_wakeup(SLEEPTIME);
    Serial.println (SLEEP_SECS);
    Serial.println( uS_TO_S_FACTOR);
    
    esp_deep_sleep_start();
 
}


void loop() {

}
