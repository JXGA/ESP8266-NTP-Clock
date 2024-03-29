/*
 * Based on code from https://github.com/G6EJD/ESP8266-MAX7219-LED-4x8x8-Matrix-Clock
  >> ESP-NTP-MQTT V0.1 > This is designed to connect to WiFi and collect NTP time. 27-MAR-2020. AKA COVID-19 year.
  >> ESP-NTP-MQTT V0.2 > MQTT Callback added. Send a message to the address below.
  >> ESP-NTP-MQTT V0.3 > Start to add Led Matrix code for MAX7219.
  >> ESP-NTP-MQTT V0.4 > Base ESP8266 code has NTP functionality in it now, using that as it only queries NTP once an hour
  >>                   > Also added MQTT brightness topic to alter brightness.
  >> ESP-NTP-MQTT V0.5 > Add 'shutdown' MQTT to switch off MAX displays
  >>                   > Add defaultBrightness
  >>                   > Add debugMode
  >> ESP-NTP-MQTT V0.6 > Fixed string handling code
  >>				           > Removed 'shutdown' MQTT option, now just set brightness to '0' to turn off
  >>				           > Fixed a memory leak issue that was causing reboots						
  >> ESP-NTP-MQTT V0.7 > Added LWT (Last Will and Testament) code to track online status 
  >> 
*/

////////////////////////////////////////////////////////
// Includes:

#include <PubSubClient.h>   // MQTT Client
#include <SPI.h>            // SPI for MAX display
#include <Adafruit_GFX.h>   // Graphics Generator for MAX display
#include <Max72xxPanel.h>   // Panel Module for MAX display
#include <ESP8266WiFi.h>    // WiFi
#include <coredecls.h>      // settimeofday_cb()
#include <PolledTimeout.h>  // PolledTimeout
#include <time.h>           // time() ctime()
#include <sys/time.h>       // struct timeval
#include <TZ.h>             // TimeZone Database (includes DST etc.)

////////////////////////////////////////////////////////

#define STASSID "<Your SSID>"        // Enter your WiFi SSID here
#define STAPSK  "<Your password>"    // Enter your Wifi password here

#define MYTZ TZ_Europe_London        // Set your timezone here

#define timeServer "uk.pool.ntp.org"  // Set your timeserver here

const char* mqtt_server = "<Your MQTT server>";  // Set your MQTT server address here
const char* mqttUser = "";                       // Set your MQTT username here (if used)
const char* mqttPassword = "";                   // Set your MQTT password here (if used)

const char* willTopic = "nodeNTP_1/status/LWT";  // Modify to set LWT topic or message
const int willQoS = 0;
const bool willRetain = true;
const char* willMessage = "disconnected";		     // Messages
const char* willMessageConnect = "connected";											   

int pinCS = D2;                             // Attach CS to this pin, DIN to MOSI and CLK to SCK (cf http://arduino.cc/en/Reference/SPI )
int numberOfHorizontalDisplays = 4;         // Display number (horiz)
int numberOfVerticalDisplays   = 1;         // Display number (vert)
int wait = 70;                              // In milliseconds, at the end of the scrolling MQTT message before returning to time
int spacer = 1;                             // Font Spacer
int width  = 5 + spacer;                    // The font width is 5 pixels
int defaultBrightness = 5;                  // Default brightness (1-15) or (0 - Off)
int debugMode = 0;                          // Enable (1) or Disable (0) serial outputs of NTP time

////////////////////////////////////////////////////////

// for testing purpose:
extern "C" int clock_gettime(clockid_t unused, struct timespec *tp);

char time_value[20];      // This var will contain the digits for the time
Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);
static time_t now;  // An object which can store a time
byte actualSecond;

static esp8266::polledTimeout::periodicMs showTimeNow(1000); // this uses the PolledTimeout library to allow an action to be performed every 1000 milli seconds

WiFiClient espClient;
PubSubClient client(espClient);
int wifiAttempt = 0;

////////////////////////////////////////////////////////

// This is a shortcut to print a bunch of stuff to the serial port.  It's very confusing, but shows what values are available
#define PTM(w) \
  Serial.print(" " #w "="); \
  Serial.print(tm->tm_##w);

void printTm(const char* what, const tm* tm) {
  Serial.print(what);
  PTM(isdst); PTM(yday); PTM(wday);
  PTM(year);  PTM(mon);  PTM(mday);
  PTM(hour);  PTM(min);  PTM(sec);
}
//

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "NTPNode-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqttUser, mqttPassword, willTopic, willQoS, willRetain, willMessage)) {
      Serial.print("Connected: ");
      Serial.println(clientId);
      // Once connected, publish an announcement...
      client.publish(willTopic, willMessageConnect,1);
      client.subscribe("nodeNTP_1/inmsg");             // Subscribe to mqtt messages in this topic (eg nodeNTP_1/inmsg "Hello World" - will display text message)
      client.subscribe("nodeNTP_1/bright");            // Subscribe to messages in this topic (eg nodeNTP_1/bright "5" - sets the display brightness 1-15 or 0 to switch off)
      } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Handles receiving mqtt messages
  
  //char* payloadData;             // we start, assuming open
  
  //Terminate the payload with 'nul' \0
  payload[length] = '\0';
  
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  
  if (strcmp(topic,"nodeNTP_1/inmsg")==0) {
    Serial.print("Message:");

    char *payloadData = (char *) payload;
    Serial.print(payloadData);

    matrix.fillScreen(LOW);
    display_message(payloadData);
  
    Serial.println();
    Serial.println("-----------------------");

    delay(1000);
  }

  if (strcmp(topic,"nodeNTP_1/bright")==0) {
    int aNumber = atoi((char *)payload);
    Serial.print("Brightness set to: ");
    Serial.println(aNumber);
    if (aNumber >= 0 || aNumber <= 15) {
        matrix.shutdown(false);
        matrix.setIntensity(aNumber);
    } 
    if (aNumber == 0) {
        matrix.shutdown(true);
    } 
   }
}


void display_message(String message) {
    // Displays a message on the matrix
  for ( int i = 0 ; i < width * message.length() + matrix.width() - spacer; i++ ) {
    int letter = i / width;
    int x = (matrix.width() - 1) - i % width;
    int y = (matrix.height() - 8) / 2; // center the text vertically
    while ( x + width - spacer >= 0 && letter >= 0 ) {
      if ( letter < message.length() ) {
        matrix.drawChar(x, y, message[letter], HIGH, LOW, 1); // HIGH LOW means foreground ON, background off, reverse to invert the image
      }
      letter--;
      x -= width;
    }
    matrix.write(); // Send bitmap to display
    delay(wait / 2);
  }
}

void showTime() {           // This function gets the current time
  now = time(nullptr);      // Updates the 'now' variable to the current time value

  byte actualHour = localtime(&now)->tm_hour;
  byte actualMinute = localtime(&now)->tm_min;
  actualSecond = localtime(&now)->tm_sec;

  sprintf(time_value, "%02d:%02d:%02d",actualHour,actualMinute,actualSecond);
  
}

void time_is_set_scheduled() {    // This function is set as the callback when time data is retrieved
  // In this case we will print the new time to serial port, so the user can see it change (from 1970)
  showTime();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("Booted");
  Serial.println("Init Matrix");

  matrix.fillScreen(0);
  matrix.setIntensity(defaultBrightness); // Use a value between 0 and 15 for brightness
  matrix.setRotation(0, 1);    // The first display is position upside down
  matrix.setRotation(1, 1);    // The first display is position upside down
  matrix.setRotation(2, 1);    // The first display is position upside down
  matrix.setRotation(3, 1);    // The first display is position upside down
  display_message("Ready");

  delay(1000);

  Serial.println("Connecting to Wi-Fi");

  // start network
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);

  wifiAttempt = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    wifiAttempt = wifiAttempt + 1;
    if (wifiAttempt > 40) {
      ESP.restart();
    }
    Serial.print(".");
  }

  String ip = WiFi.localIP().toString().c_str();
  Serial.println("WiFi connected:" + ip);
  display_message("Connected - " + ip);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  // install callback - called when settimeofday is called (by SNTP or us)
  // once enabled (by DHCP), SNTP is updated every hour
  settimeofday_cb(time_is_set_scheduled);

  // This is where your time zone is set
  configTime(MYTZ, timeServer);
  
  // On boot up the time value will be 0 UTC, which is 1970.  
  // This is just showing you that, so you can see it change when the current data is received
  Serial.printf("Time is currently set by a constant:\n");
  showTime();
}

void loop() {

 //matrix.fillScreen(LOW);

  //Ensures MQTT client is connected
  {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
  }

if (showTimeNow) {
    showTime();

      if (debugMode) {
         Serial.print("time_value var is: ");
         Serial.println(time_value);
      }

      matrix.drawChar(1, 0, time_value[0], HIGH, LOW, 1); // H
      matrix.drawChar(9, 0, time_value[1], HIGH, LOW, 1); // HH
      //matrix.drawChar(14,0, time_value[2], HIGH,LOW,1); // HH: // Static ':' Symbol
      if (actualSecond % 2 == 0) {
        matrix.drawChar(14, 0, ':', HIGH, LOW, 1);
      } else {
       matrix.drawChar(14, 0, ' ', HIGH, LOW, 1);
      }
      matrix.drawChar(19, 0, time_value[3], HIGH, LOW, 1); // HH:M
      matrix.drawChar(26, 0, time_value[4], HIGH, LOW, 1); // HH:MM
      matrix.write(); // Send bitmap to display

      if (debugMode) {
        Serial.println();
        // human readable serial debug. Switch on debugMode at the vars to enable.
        Serial.print("ctime:     ");
        Serial.print(ctime(&now));
      }
      
     }
    //delay(50);
      
}
