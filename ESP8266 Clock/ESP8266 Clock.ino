/*
 Name:		ESP8266 Clock.ino
 Created:	4/23/2018 9:41:34 AM
 Author:	Terence
*/

#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <Wire.h>  // Include Wire if you're using I2C
#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library
#include <NTPClient.h>
#include <WiFiUdp.h>

//#define hard_reset

WiFiClient wifiClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "sg.pool.ntp.org", 8*60*60, 15*60*1000);  // offset +8 hours, update every 15 min.

#ifdef mDns
MDNSResponder mdns;
#endif

char EC_ID[16];
//flag for saving data
bool shouldSaveConfig = false;

//////////////////////////
// MicroOLED Definition //
//////////////////////////
#define PIN_RESET 0  // RST to pin 0 (SPI & I2C)
#define DC_JUMPER 0  // DC jumper setting(I2C only)

//////////////////////////////////
// MicroOLED Object Declaration //
//////////////////////////////////
MicroOLED oled(PIN_RESET, DC_JUMPER);  // I2C

// Use these variables to set the initial time
int hours = 12;
int minutes = 0;
int seconds = 0;

// How fast do you want the clock to spin? Set this to 1 for fun.
// Set this to 1000 to get _about_ 1 second timing.
const int CLOCK_SPEED = 1000;

// Global variables to help draw the clock face:
const int MIDDLE_Y = oled.getLCDHeight() / 2;
const int MIDDLE_X = oled.getLCDWidth() / 2;

int CLOCK_RADIUS;
int POS_12_X, POS_12_Y;
int POS_3_X, POS_3_Y;
int POS_6_X, POS_6_Y;
int POS_9_X, POS_9_Y;
int S_LENGTH;
int M_LENGTH;
int H_LENGTH;

unsigned long lastDraw = 0;

void initClockVariables()
{
	// Calculate constants for clock face component positions:
	oled.setFontType(0);
	CLOCK_RADIUS = min(MIDDLE_X, MIDDLE_Y) - 1;
	POS_12_X = MIDDLE_X - oled.getFontWidth();
	POS_12_Y = MIDDLE_Y - CLOCK_RADIUS + 2;
	POS_3_X = MIDDLE_X + CLOCK_RADIUS - oled.getFontWidth() - 1;
	POS_3_Y = MIDDLE_Y - oled.getFontHeight() / 2;
	POS_6_X = MIDDLE_X - oled.getFontWidth() / 2;
	POS_6_Y = MIDDLE_Y + CLOCK_RADIUS - oled.getFontHeight() - 1;
	POS_9_X = MIDDLE_X - CLOCK_RADIUS + oled.getFontWidth() - 2;
	POS_9_Y = MIDDLE_Y - oled.getFontHeight() / 2;

	// Calculate clock arm lengths
	S_LENGTH = CLOCK_RADIUS - 2;
	M_LENGTH = S_LENGTH * 0.7;
	H_LENGTH = S_LENGTH * 0.5;
}

// Simple function to increment seconds and then increment minutes
// and hours if necessary.
void updateTime()
{
	if (timeClient.update()) {
		hours = timeClient.getHours();
		minutes = timeClient.getMinutes();
		seconds = timeClient.getSeconds();
		//Serial.println(timeClient.getFormattedTime());
	}

	//seconds++;  // Increment seconds
	//if (seconds >= 60)  // If seconds overflows (>=60)
	//{
	//	seconds = 0;  // Set seconds back to 0
	//	minutes++;    // Increment minutes
	//	if (minutes >= 60)  // If minutes overflows (>=60)
	//	{
	//		minutes = 0;  // Set minutes back to 0
	//		hours++;      // Increment hours
	//		if (hours >= 12)  // If hours overflows (>=12)
	//		{
	//			hours = 0;  // Set hours back to 0
	//		}
	//	}
	//}
}

// Draw the clock's three arms: seconds, minutes, hours.
void drawArms(int h, int m, int s)
{
	double midHours;  // this will be used to slightly adjust the hour hand
	static int hx, hy, mx, my, sx, sy;

	// Adjust time to shift display 90 degrees ccw
	// this will turn the clock the same direction as text:
	h -= 3;
	m -= 15;
	s -= 15;
	if (h <= 0)
		h += 12;
	if (m < 0)
		m += 60;
	if (s < 0)
		s += 60;

	// Calculate and draw new lines:
	s = map(s, 0, 60, 0, 360);  // map the 0-60, to "360 degrees"
	sx = S_LENGTH * cos(PI * ((float)s) / 180);  // woo trig!
	sy = S_LENGTH * sin(PI * ((float)s) / 180);  // woo trig!
												 // draw the second hand:
	oled.line(MIDDLE_X, MIDDLE_Y, MIDDLE_X + sx, MIDDLE_Y + sy);

	m = map(m, 0, 60, 0, 360);  // map the 0-60, to "360 degrees"
	mx = M_LENGTH * cos(PI * ((float)m) / 180);  // woo trig!
	my = M_LENGTH * sin(PI * ((float)m) / 180);  // woo trig!
												 // draw the minute hand
	oled.line(MIDDLE_X, MIDDLE_Y, MIDDLE_X + mx, MIDDLE_Y + my);

	midHours = minutes / 12;  // midHours is used to set the hours hand to middling levels between whole hours
	h *= 5;  // Get hours and midhours to the same scale
	h += midHours;  // add hours and midhours
	h = map(h, 0, 60, 0, 360);  // map the 0-60, to "360 degrees"
	hx = H_LENGTH * cos(PI * ((float)h) / 180);  // woo trig!
	hy = H_LENGTH * sin(PI * ((float)h) / 180);  // woo trig!
												 // draw the hour hand:
	oled.line(MIDDLE_X, MIDDLE_Y, MIDDLE_X + hx, MIDDLE_Y + hy);
}

// Draw an analog clock face
void drawFace()
{
	// Draw the clock border
	oled.circle(MIDDLE_X, MIDDLE_Y, CLOCK_RADIUS);

	// Draw the clock numbers
	oled.setFontType(0); // set font type 0, please see declaration in SFE_MicroOLED.cpp
	oled.setCursor(POS_12_X, POS_12_Y); // points cursor to x=27 y=0
	oled.print(12);
	oled.setCursor(POS_6_X, POS_6_Y);
	oled.print(6);
	oled.setCursor(POS_9_X, POS_9_Y);
	oled.print(9);
	oled.setCursor(POS_3_X, POS_3_Y);
	oled.print(3);
}

//callback notifying us of the need to save config
void saveConfigCallback() {
	Serial.println("Should save config");
	shouldSaveConfig = true;
}

// the setup function runs once when you press reset or power the board
void setup() {

	oled.begin();     // Initialize the OLED
	oled.clear(PAGE); // Clear the display's internal memory
	oled.clear(ALL);  // Clear the library's display buffer
	oled.display();   // Display what's in the buffer (splashscreen)

	timeClient.begin();

	initClockVariables();

	Serial.begin(115200);
	Serial.println("OK");

	sprintf(EC_ID, "ESPCLK-%x", ESP.getChipId());
	Serial.print("ESP8266 Clock ID : ");
	Serial.println(EC_ID);

#ifdef hard_reset
	Serial.println("Reseting System");
	Serial.println("Format SPIFFS");
	Serial.println("This will take serval minutes");
	SPIFFS.format();
	Serial.println("SPIFFS formatted");
#endif

	//read configuration from FS json
	Serial.println("mounting FS...");

	if (SPIFFS.begin()) {
		Serial.println("mounted file system");

		if (SPIFFS.exists("/config.json")) {
			//file exists, reading and loading

			Serial.println("reading config file");
			File configFile = SPIFFS.open("/config.json", "r");

			if (configFile) {
				Serial.println("opened config file\n");
				size_t size = configFile.size();
				// Allocate a buffer to store contents of the file.
				std::unique_ptr<char[]> buf(new char[size]);

				configFile.readBytes(buf.get(), size);

				DynamicJsonBuffer jsonBuffer;

				JsonObject& json = jsonBuffer.parseObject(buf.get());

				json.printTo(Serial);

				Serial.println("\n");

				if (json.success()) {
					Serial.println("Conifg Ok\n");
				}
				else {
					Serial.println("failed to load json config");
				}
			}
		}
	}
	else {
		Serial.println("failed to mount FS");
	}

	//end read
	Serial.println("OK");
	// The extra parameters to be configured (can be either global or just in the setup)
	// After connecting, parameter.getValue() will get you the configured value
	// id/name placeholder/prompt default length
	wifi_station_set_hostname(EC_ID);

	//Local intialization. Once its business is done, there is no need to keep it around
	WiFiManager wifiManager;

	//set config save notify callback
	wifiManager.setSaveConfigCallback(saveConfigCallback);
	wifiManager.setTimeout(180);
	//set minimu quality of signal so it ignores AP's under that quality
	//defaults to 8%
	wifiManager.setMinimumSignalQuality();

#ifdef hard_reset
		wifiManager.resetSettings();
#endif

		//tries to connect to last known settings
		//if it does not connect it starts an access point with the specified name
		//and goes into a blocking loop awaiting configuration
		if (!wifiManager.autoConnect("ESP8266 Clock", "12345678")) {
			Serial.println("failed to connect, we should reset as see if it connects");
			delay(3000);
			ESP.reset();
			delay(5000);
		}

		//save the custom parameters to FS
		if (shouldSaveConfig) {
			Serial.println("saving config");
			DynamicJsonBuffer jsonBuffer;
			JsonObject& json = jsonBuffer.createObject();
			File configFile = SPIFFS.open("/config.json", "w");
			if (!configFile) {
				Serial.println("failed to open config file for writing");
			}

			json.printTo(Serial);
			json.printTo(configFile);
			configFile.close();
			//end save
		}

#ifdef mDns
		if (!MDNS.begin(EC_ID)) {
			Serial.println("Error setting up MDNS responder!");
			while (1) {
				delay(1000);
			}
		}
		Serial.println("mDNS responder started");
#endif
}

// the loop function runs over and over again until power down or reset
void loop()
{

	// Check if we need to update seconds, minutes, hours:
	if (lastDraw + CLOCK_SPEED < millis())
	{
		lastDraw = millis();
		// Add a second, update minutes/hours if necessary:
		updateTime();

		// Draw the clock:
		oled.clear(PAGE);  // Clear the buffer
		drawFace();  // Draw the face to the buffer
		drawArms(hours, minutes, seconds);  // Draw arms to the buffer
		oled.display(); // Draw the memory buffer
	}
}
