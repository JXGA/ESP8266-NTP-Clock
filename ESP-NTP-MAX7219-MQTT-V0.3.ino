/*
 * Based on code from https://github.com/G6EJD/ESP8266-MAX7219-LED-4x8x8-Matrix-Clock
  >> ESP-NTP-MQTT V0.1 > This is designed to connect to WiFi and collect NTP time. 27-MAR-2020. AKA COVID-19 year.
  >> ESP-NTP-MQTT V0.2 > MQTT Callback added. Send a message to the address below.
  >> ESP-NTP-MQTT V0.3 > Start to add Led Matrix code for MAX7219.
*/

#include <NTPtimeESP.h>   // NTP Client
#include <PubSubClient.h> // MQTT Client
#include <SPI.h>          // SPI for MAX display
#include <Adafruit_GFX.h> // Graphics Generator for MAX display
#include <Max72xxPanel.h> // Panel Module for MAX display

#define DEBUG_ON

// Consts for NTP
NTPtime NTPch("ie.pool.ntp.org");   // Choose server pool as required
strDateTime dateTime;

// Consts for WiFi
char *ssid      = "SSID_GOES_HERE";               // Set you WiFi SSID
char *password  = "PASSWD_GOES_HERE";             // Set you WiFi password
int wifiAttempt = 0;
WiFiClient espClient;

// Consts for MQTT Rec.
const char* mqtt_server = "MQTT_SERVER_GOES_HERE";
const char* mqttUser = "MQTT_USER_GOES_HERE";
const char* mqttPassword = "MQTT_PASSWD_GOES_HERE";
PubSubClient client(espClient);

// Consts for Loop
int lastMinute = 0;
int lastSecond = 0;

// Consts for MAX
int pinCS = D2; // Attach CS to this pin, DIN to MOSI and CLK to SCK (cf http://arduino.cc/en/Reference/SPI )
int numberOfHorizontalDisplays = 4;
int numberOfVerticalDisplays   = 1;
char time_value[20];  // This var will contain the digits for the time
Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);
int wait = 70; // In milliseconds, at the end of the scrolling MQTT message before returning to time

int spacer = 1;
int width  = 5 + spacer; // The font width is 5 pixels

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "NTPNode-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("nodeNTP/avail", "online");
      client.subscribe("nodeNTP/msg");
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

  String payloadData;             // we start, assuming open

  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  Serial.print("Message:");
  payload[length] = '\0';
  payloadData = String((char*)payload);
  Serial.print(payloadData);

  display_message(payloadData);
  
  Serial.println();
  Serial.println("-----------------------");

  delay(100);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booted");

  delay(1000);

  Serial.println("Init Matrix");

  matrix.fillScreen(0);
  matrix.setIntensity(0); // Use a value between 0 and 15 for brightness
  matrix.setRotation(0, 1);    // The first display is position upside down
  matrix.setRotation(1, 1);    // The first display is position upside down
  matrix.setRotation(2, 1);    // The first display is position upside down
  matrix.setRotation(3, 1);    // The first display is position upside down
  display_message("Ready");

  delay(1000);

  Serial.println("Connecting to Wi-Fi");

  WiFi.mode(WIFI_STA);
  WiFi.begin (ssid, password);

  wifiAttempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    wifiAttempt = wifiAttempt + 1;
    if (wifiAttempt > 40) {
      ESP.restart();
    }
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  display_message("Connected");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

}

void display_message(String message) {
  for ( int i = 0 ; i < width * message.length() + matrix.width() - spacer; i++ ) {
    //matrix.fillScreen(LOW);
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

void loop() {

  matrix.fillScreen(LOW);

  //Ensures MQTT client is connected
  {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
  }


  String myTime;


  // first parameter: Time zone in floating point (0 for UTC?); second parameter: 1 for European summer time; 2 for US daylight saving time; 0 for no DST adjustment; (contributed by viewwer, not tested by me)

  dateTime = NTPch.getNTPtime(0.0, 1);


  // check dateTime.valid before using the returned time
  // Use "setSendInterval" or "setRecvTimeout" if required

  if (dateTime.valid) {

    // NTPch.printDateTime(dateTime);

    byte actualHour = dateTime.hour;
    byte actualMinute = dateTime.minute;
    byte actualSecond = dateTime.second;
    int actualyear = dateTime.year;
    byte actualMonth = dateTime.month;
    byte actualday = dateTime.day;
    byte actualdayofWeek = dateTime.dayofWeek;

    if (lastSecond != actualSecond) {


      // Pretty display for future use:
      if (actualHour < 10) {
        myTime = myTime + "0" + actualHour;
      }
      else {
        myTime = actualHour;
      }

      if (actualMinute < 10) {
        myTime = myTime + ":0" + actualMinute;
      }
      else {
        myTime = myTime + ":" + actualMinute;
      }

      if (actualSecond < 10) {
        myTime = myTime + ":0" + actualSecond;
      }
      else {
        myTime = myTime + ":" + actualSecond;
      }

      myTime.toCharArray(time_value, 10);
      matrix.drawChar(1, 0, time_value[0], HIGH, LOW, 1); // H
      matrix.drawChar(9, 0, time_value[1], HIGH, LOW, 1); // HH
      // matrix.drawChar(14,0, time_value[2], HIGH,LOW,1); // HH: // Static ':' Symbol
      if (actualSecond % 2 == 0) {
        matrix.drawChar(14, 0, ':', HIGH, LOW, 1);
      } else {
        matrix.drawChar(14, 0, ' ', HIGH, LOW, 1);
      }
      matrix.drawChar(19, 0, time_value[3], HIGH, LOW, 1); // HH:M
      matrix.drawChar(26, 0, time_value[4], HIGH, LOW, 1); // HH:MM
      matrix.write(); // Send bitmap to display

      Serial.println(myTime); // Display on Serial

      lastSecond = actualSecond; // Loop for MQTT Update (Below)

    }

    delay(50);

    if (actualSecond == 1) {
      if (lastMinute != actualMinute) {
        Serial.println();
        Serial.println("60 Second MQTT Update running");
        client.publish("nodeNTP/avail", "online");
        Serial.println("...complete");
        lastMinute = actualMinute;
      }
    }

  }

}
