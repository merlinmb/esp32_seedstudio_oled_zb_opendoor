#include <FS.h>
#include <SPIFFS.h>

//#include <Arduino.h>

#include "connectionDetails.h"
#include "merlinNetwork.h"
#include "merlinUpdateWebServer.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SH110x.h>

#include <vector>
#include <ArduinoJson.h>

#define DEBUG 1
#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTDEC(x, DEC) Serial.print(x, DEC)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTLNDEC(x, DEC) Serial.println(x, DEC)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x, DEC)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTLNDEC(x, DEC)
#endif

#define MCMDVERSION 1.8

// Enable/disable TOUCH sensor functionality
#define ENABLE_TOUCH_SENSOR 1

#define MAXBRIGHTNESS 255
#define MINBRIGHTNESS 0

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

/* Uncomment the initialize the I2C address , uncomment only one, If you get a totally blank screen try the other*/
#define i2c_Address 0x3c // initialize with the I2C addr 0x3C Typically eBay OLED's

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SH1106G _display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, OLED_RESET);

//Adafruit_SSD1306 _display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, OLED_RESET);
// Adafruit_SSD1309 display90(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, OLED_RESET);

#include <FluxGarage_RoboEyes.h>
RoboEyes<Adafruit_SH1106G> _roboEyes(_display); // create RoboEyes instance

#define UPDATE_READINGSDATA_INTERVAL_MILLISECS 1800000 // 60000=10*60*1000, Update every 10min
// #define UPDATE_READINGSDATA_INTERVAL_MILLISECS 5000    // every 10 seconds
#define UPDATE_WIFICHECK_INTERVAL_MILLISECS 60000 // Update every 1 min
#define UPDATE_DISPLAY_INTERVAL_MILLISECS 3000	  // Update every 3s
unsigned long _runWifiReconnect;
unsigned long _runCurrent;

// Non-blocking reconnect intervals (do not attempt reconnection on every loop)
#define WIFI_RECONNECT_INTERVAL_MILLISECS 10000
#define MQTT_RECONNECT_INTERVAL_MILLISECS 5000

unsigned long _lastMqttAttempt = 0;
unsigned long _mqttReconnectInterval = MQTT_RECONNECT_INTERVAL_MILLISECS;
int _mqttFailCount = 0;
const int MQTT_FAIL_LIMIT = 8;

String _lastMQTTMessage = "";

int _brightnesses[5] = {0, 51, 115, 192, MAXBRIGHTNESS};
int _selectedBrightness = 4;

int _brightness = 100;
bool _isDisplayOn = false;
bool _forceUpdate = false;
byte _displayscreen = 0;
int _configFlipSreen = 999;

int _totalOpenContacts = 0;


String _style = F(
	"<style>#file-input,input{width:80%;height:40px;border-radius:4px;margin:5px auto;font-size:15px}"
	"input{background:#f1f1f1;border:0;padding:0 5px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777;}"
	"select{background:#f1f1f1;border:0;padding:0 5px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
	"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:center;display:block;cursor:pointer}"
	"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
	"div{background:#fff;max-width:1024px;margin:5px auto;padding:5px;border-radius:5px;text-align:left;font-family:sans-serif;font-size:14px;}"
	"form{background:#fff;margin:5px auto;padding:5px;border-radius:5px;text-align:center;font-family:sans-serif;font-size:14px;}"
	".btn{background:#3498db;color:#fff;cursor:pointer}"
	"svg[data-lastpass-icon=true]{visibility:none;display: none !important;}"
	"div[data-lastpass-icon-root=true]{visibility:none;display: none !important;}"
	"div[data-lastpass-icon-root]{visibility:none;display: none !important;}"
	"div[data-lastpass-root]{visibility:none;display: none !important;}"
	"div[data-lastpass-infield]{visibility:none;display: none !important;}"
	"div[data-lastpass-infield='true']{visibility:none;display: none !important;}"
	"hr.new5 { border: 1px solid #3498db;  }"
	".dot{height:12px;width:12px;border-radius:50%;display:inline-block;margin-right:6px;vertical-align:middle}.dot-closed{background:#2ecc71}.dot-open{background:#e74c3c}.legend{margin-top:6px;margin-bottom:6px;}"
	"</style>");

// MQTT subscription topics managed via web UI and persisted to SPIFFS
struct MQTTSubscription
{
	String topic;
	String friendly;
	bool contact = true;	  // last known 'contact' boolean
	String lastSeen;		  // 'last_seen' field from payload or empty
	bool lastPayload = false; // raw last payload received
};

std::vector<MQTTSubscription> _subscriptions;

void addMQTTTopic(String topic, String friendly);
void removeMQTTTopic(int idx);

// When true, OLED shows list of open contacts instead of roboEyes
bool _showOpenContacts = false;

// Scrolling state for open contacts list
int _openScrollOffset = 0; // which open item index to start showing
unsigned long _runOpenScroll = 0;
unsigned long _openScrollInterval = 3000; // milliseconds, adjustable via web UI

void showOpenContactsOnOLED();

#ifdef ENABLE_TOUCH_SENSOR
// Touch sensor configuration (TTP223 on D2 / GPIO2). If your board maps D2 differently, adjust TOUCH_PIN accordingly.
#define TOUCH_PIN D2 // D2 => GPIO2
#define TOUCH_DEBOUNCE_MS 50
#define TOUCH_DOUBLE_TAP_MS 400
#define TOUCH_LONG_PRESS_MS 5000
volatile bool _interruptFlag = false; // Flag to signal the loop

// Touch state variables
bool _touchState = false;       // logical pressed/released state
bool _lastTouchRaw = false;     // last raw digitalRead value
unsigned long _touchLastChange = 0;
unsigned long _touchPressedAt = 0;
unsigned long _lastTapTime = 0;
int _tapCount = 0;
bool _longPressHandled = false;

// Touch handling functions
void handleTouch();
void toggleDisplay();
void clearOpenContacts();
#endif // ENABLE_TOUCH_SENSOR

// Helper to match subscription topic to incoming topic (supports '/#' suffix for prefix matches)
bool topicMatches(const String &sub, const String &topic);

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) \
	{                       \
		int16_t t = a;      \
		a = b;              \
		b = t;              \
	}
#endif

void setOLEDContrast(uint8_t value)
{
	_display.oled_command(0x81);
	_display.oled_command(value); // Value 0-255
	// 0x81 is the command for contrast, followed by the value
	//uint8_t contrast_cmd[] = {0x81, value};
	//_display.oled_commandList(contrast_cmd, 2);
}

void setOLEDDimMode(bool enable)
{
	DEBUG_PRINTLN("Setting Dim Mode: " + String(enable ? "ENABLED" : "DISABLED"));
	_display.oled_command(enable ? 0xAB : 0xAC); // Dim Mode
}

void setOLEDDisplayOn(bool on)
{
	if (_isDisplayOn == on)
	{
		return;
	}
	DEBUG_PRINTLN("Turning display " + String(on ? "ON" : "OFF"));
	_display.oled_command(on ? 0xAF : 0xAE);
	_isDisplayOn = on;
}

void setContrast(uint8_t contrast)
{

	if(_brightness == contrast){		return;	}

	if (contrast > MAXBRIGHTNESS)
	{
		contrast = MAXBRIGHTNESS;
	}
	if (contrast < MINBRIGHTNESS)
	{
		contrast = MINBRIGHTNESS;
	}

	DEBUG_PRINTLN("Setting contrast to: " + String(contrast));
	setOLEDContrast(contrast);
	setOLEDDisplayOn(contrast > 0);

	_brightness = contrast;
}

void setupSPIFFS()
{
	if (SPIFFS.begin(true))
	{
		DEBUG_PRINTLN("SPIFFS: Mounted file system.");
	}
	else
	{
		DEBUG_PRINTLN("SPIFFS: FAILED to mount file system!");
	}
}

/***************************************************
  SPIFFS functions
****************************************************/
bool parseConfigValue(String key, String value)
{
	DEBUG_PRINTLN("Parsing Config Value, " + key + ": " + value);

	key.toLowerCase();
	value.trim();

	if (key == "flipscreen")
	{
		int __intValue = (value == "true" ? 3 : 1);
		if (_configFlipSreen != __intValue)
		{
			_configFlipSreen = __intValue;

			_display.setRotation(_configFlipSreen ? 0 : 2);
		}
	}

	if (key == "brightness")
	{
		int __newVal = value.toInt();
		if (_brightness != __newVal)
		{
			_brightness = value.toInt();

			setContrast(_brightness);
		}
	}

	if (key == "open_scroll_interval_ms")
	{

		// Use the std:: namespace
		unsigned long __newVal = std::max((unsigned long)500, (unsigned long)value.toInt());

		if (_openScrollInterval != __newVal)
		{
			_openScrollInterval = __newVal;
			DEBUG_PRINTLN("Open scroll interval set to " + String(_openScrollInterval) + " ms");
		}
	}

	if (key == "subscribetopics")
	{
		// topics are stored as a '|' delimited list of entries formatted as: topic###friendly
		_subscriptions.clear();
		value.trim();
		if (value.length() > 0)
		{
			int start = 0;
			while (start < value.length())
			{
				int sep = value.indexOf('|', start);
				if (sep == -1)
					sep = value.length();
				String entry = value.substring(start, sep);
				entry.trim();
				if (entry.length() > 0)
				{
					int mid = entry.indexOf("###");
					String topic;
					String friendly;
					if (mid == -1)
					{
						topic = entry;
						friendly = "";
					}
					else
					{
						topic = entry.substring(0, mid);
						friendly = entry.substring(mid + 3);
					}
					topic.trim();
					friendly.trim();
					if (topic.length() > 0)
					{
						MQTTSubscription s;
						s.topic = topic;
						s.friendly = friendly;
						_subscriptions.push_back(s);
					}
				}
				start = sep + 1;
			}
		}
	}

	DEBUG_PRINTLN("parseConfigValue() - completed...");

	return true;
}

void loadCustomParamsSPIFFS()
{
	// read configuration from FS json
	DEBUG_PRINTLN("loadCustomParamsSPIFFS() - Open config file...");

	File __configFile = SPIFFS.open("/config.ini", FILE_READ);
	if (__configFile)
	{
		DEBUG_PRINTLN("Reading config file [" + String(__configFile.size()) + " bytes]");
		while (__configFile.available())
		{
			String __inString = __configFile.readStringUntil('\n');
			DEBUG_PRINTLN("Read line: " + __inString);
			int __equalsLoc = __inString.indexOf('=');

			String __key = __inString.substring(0, __equalsLoc);
			String __value = __inString.substring(__equalsLoc + 1, __inString.length());

			parseConfigValue(__key, __value);
		}

		DEBUG_PRINTLN("loadCustomParamsSPIFFS() - close config file...");
		__configFile.close();
		DEBUG_PRINTLN("... Done");
	}
}

void writeStrtoFile(File &file, const String &key, const String &value)
{
	DEBUG_PRINTLN("    " + key + ": " + value);
	file.println(key + "=" + value);
}

void saveConfigValuesSPIFFS()
{
	DEBUG_PRINTLN("saveConfigValuesSPIFFS()");
	if (SPIFFS.remove("/config.ini"))
	{
		DEBUG_PRINTLN("Deleted old file");
	}

	DEBUG_PRINTLN("Open File in Write Mode");
	// open the file in write mode
	File __configFile = SPIFFS.open("/config.ini", FILE_WRITE);
	DEBUG_PRINTLN("Saving config to FS");

	writeStrtoFile(__configFile, "flipscreen", String(_configFlipSreen == 3));
	writeStrtoFile(__configFile, "brightness", String(_brightness));

	// Save subscribed topics as a '|' delimited list of entries formatted as: topic###friendly
	String __subTopics = "";
	for (size_t i = 0; i < _subscriptions.size(); i++)
	{
		if (i > 0)
			__subTopics += "|";
		__subTopics += _subscriptions[i].topic + "###" + _subscriptions[i].friendly;
	}
	writeStrtoFile(__configFile, "subscribetopics", __subTopics);

	// Save OLED scroll interval
	writeStrtoFile(__configFile, "open_scroll_interval_ms", String(_openScrollInterval));

	__configFile.close();
	DEBUG_PRINTLN("... Done");
	//delay(250); // give SPIFFS chance to settle
}

// Add a MQTT topic to list (no duplicates). subscribes immediately if connected
void addMQTTTopic(String topic, String friendly)
{
	topic.trim();
	friendly.trim();
	if (topic.length() == 0)
		return;

	// If topic exists, update friendly name
	for (auto &s : _subscriptions)
	{
		if (s.topic == topic)
		{
			s.friendly = friendly;
			DEBUG_PRINTLN("Updated MQTT topic friendly: " + topic + " -> " + friendly);
			return;
		}
	}

	MQTTSubscription s;
	s.topic = topic;
	s.friendly = friendly;
	s.contact = true;
	s.lastSeen = "";
	s.lastPayload = false;
	_subscriptions.push_back(s);
	DEBUG_PRINTLN("Added MQTT topic: " + topic + " (" + friendly + ")");
	if (_mqttClient.connected())
	{
		mqttSubscribe(topic);
	}
}

// Remove a MQTT topic by index (unsubscribe if connected)
void removeMQTTTopic(int idx)
{
	if (idx < 0 || idx >= (int)_subscriptions.size())
		return;

	String topic = _subscriptions[idx].topic;
	if (_mqttClient.connected())
	{
		DEBUG_PRINTLN("Unsubscribing: " + topic);
		_mqttClient.unsubscribe(topic.c_str());
	}
	_subscriptions.erase(_subscriptions.begin() + idx);
	DEBUG_PRINTLN("Removed MQTT topic: " + topic);
}
void mqttTransmitCustomStat() {}
void mqttTransmitSensorStat() {}

void mqttTransmitCustomSubscribe()
{
	// Subscribe to user configured topics after MQTT connects
	if (_subscriptions.size() == 0)
		return;
	DEBUG_PRINTLN("Subscribing to user configured MQTT topics...");
	for (size_t i = 0; i < _subscriptions.size(); i++)
	{
		String t = _subscriptions[i].topic;
		if (t.length() > 0)
		{
			mqttSubscribe(t);
		}
	}
}

bool topicMatches(const String &sub, const String &topic)
{
	// support simple prefix wildcard using '/#'
	if (sub.endsWith("/#"))
	{
		String prefix = sub.substring(0, sub.length() - 2);
		return topic.startsWith(prefix);
	}

	// exact match
	return sub == topic;
}

void mqttSendSensorStat()
{
	if (_mqttClient.connected())
	{
		mqttTransmitSensorStat();
	}
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
	DEBUG_PRINT("Message arrived [");
	DEBUG_PRINT(topic);
	DEBUG_PRINT("] ");

	char message_buff[MQTT_MAX_PACKET_SIZE];
	int i = 0;
	for (i = 0; i < length; i++)
	{
		message_buff[i] = payload[i];
	}
	message_buff[i] = '\0';
	String __payloadString = String(message_buff);

	__payloadString.trim();

	DEBUG_PRINTLN(__payloadString);

	String __incoming = String(topic);
	__incoming.trim();

	_lastMQTTMessage = __incoming + " " + __payloadString;

	// Update subscription status if this topic matches any configured subscription
	for (size_t __s = 0; __s < _subscriptions.size(); __s++)
	{
		if (topicMatches(_subscriptions[__s].topic, topic))
		{
			DEBUG_PRINTLN("Updating subscription for topic match: " + _subscriptions[__s].topic);
			// try parse JSON payload for 'contact' and 'last_seen'
			JsonDocument __doc;
			DeserializationError __err = deserializeJson(__doc, __payloadString);
			if (!__err)
			{
				if (__doc["contact"].is<bool>())
				{
					_subscriptions[__s].contact = (bool)__doc["contact"];
				}
				if (__doc["last_seen"].is<const char *>())
				{
					// store as string
					const char *ls = __doc["last_seen"];
					if (ls)
						_subscriptions[__s].lastSeen = String(ls);
					else
						_subscriptions[__s].lastSeen = _timeClient.getFormattedDate();
				}
				_subscriptions[__s].lastPayload = true;
				DEBUG_PRINTLN("Updated subscription status for: " + _subscriptions[__s].topic + " contact=" + String(_subscriptions[__s].contact ? "true" : "false"));
			}
			else
			{
				DEBUG_PRINTLN("JSON parse failed for topic " + __incoming + ": " + String(__err.c_str()));
			}
			__doc.clear();
		}
	}
	if (_subscriptions.size() > 0)
	{
		// Check if any subscription has 'contact' == false (open)
		bool hasOpen = false;
		for (size_t __s = 0; __s < _subscriptions.size(); __s++)
		{
			if (!_subscriptions[__s].contact)
			{
				hasOpen = true;
				break;
			}
		}
		// reset scroll offset when entering open-display mode
		if (hasOpen && !_showOpenContacts)
		{
			_openScrollOffset = 0;
			_runOpenScroll = millis();
		}
		_showOpenContacts = hasOpen;
		if(hasOpen){
			DEBUG_PRINTLN("There are open contacts");
			setContrast(MAXBRIGHTNESS); //ensure display is on if there are open doors
		} else {
			DEBUG_PRINTLN("All contacts closed");
		}

	}

	if (__incoming == "cmnd/" + String(_deviceClientName) + "/reset")
	{
		ESP.restart();
		return;
	}

	if (__incoming == "cmnd/" + String(_deviceClientName) + "/sendstat")
	{
		mqttSendInitStat();
		mqttSendSensorStat();
		return;
	}

	if (__incoming == "cmnd/" + String(_deviceClientName) + "/mood")
	{
		// For mood type switch
		// #define DEFAULT 0
		// #define TIRED 1
		// #define ANGRY 2
		// #define HAPPY 3
		_roboEyes.setMood(__payloadString.toInt());

		return;
	}

	if (__incoming == "cmnd/" + String(_deviceClientName) + "/curiosity")
	{
		bool __curiosity = (__payloadString == "true" ? true : false);
		_roboEyes.setCuriosity(__curiosity);
		return;
	}

	if (__incoming == "cmnd/" + String(_deviceClientName) + "/laugh")
	{
		_roboEyes.anim_laugh();
		return;
	}

	if (__incoming == "cmnd/" + String(_deviceClientName) + "/confused")
	{
		_roboEyes.anim_confused();
		return;
	}
	if (__incoming == "cmnd/" + String(_deviceClientName) + "/eyewidth")
	{
		int __newVal = __payloadString.toInt();
		_roboEyes.setWidth(__newVal, __newVal);
		return;
	}

	if (__incoming == "cmnd/" + String(_deviceClientName) + "/eyeheight")
	{
		int __newVal = __payloadString.toInt();
		_roboEyes.setHeight(__newVal, __newVal);
		return;
	}

	if (__incoming == "cmnd/" + String(_deviceClientName) + "/eyeborderradius")
	{
		int __newVal = __payloadString.toInt();
		_roboEyes.setBorderradius(__newVal, __newVal);
		return;
	}

	if (__incoming == "cmnd/" + String(_deviceClientName) + "/cyclops")
	{
		bool __cyclops = (__payloadString == "true" ? true : false);
		_roboEyes.setCyclops(__cyclops);
		return;
	}

	if (__incoming == "cmnd/mcmddevices/airquality2.5ppm")
	{

		_roboEyes.setCuriosity(false);

		int __newVal = __payloadString.toInt();
		if (__newVal < 0)
		{
			__newVal = 0;
		}

		if (__newVal >= 0 && __newVal <= 10)
		{
			_roboEyes.setMood(HAPPY);
		}
		else if (__newVal > 10 && __newVal <= 15)
		{
			_roboEyes.setMood(DEFAULT);
		}
		else if (__newVal > 15 && __newVal <= 18)
		{
			_roboEyes.setMood(TIRED);
		}
		else if (__newVal > 18)
		{
			_roboEyes.setMood(ANGRY);
		}
		else
		{
			DEBUG_PRINTLN("Invalid Air Quality Value: " + __payloadString);
		}
		return;
	}

	if (__incoming == "cmnd/mcmddevices/brightness") // 0...100
	{
		int __newBrightness = __payloadString.toInt();

		if (__newBrightness < MINBRIGHTNESS)
			__newBrightness = MINBRIGHTNESS;
		if (__newBrightness > MAXBRIGHTNESS)
			__newBrightness = MAXBRIGHTNESS;

		// float __contrastVal = MAXBRIGHTNESS * __newBrightness / 100;

		if (_totalOpenContacts<=0) { //dont turn the display off if there are open doors
			setContrast(__newBrightness);
		}
		

		return;
	}
	if (__incoming == "cmnd/mcmddevices/brightnesspercentage") // 0...100
	{
		int __newBrightness = (__payloadString.toInt()/100*MAXBRIGHTNESS);

		if (__newBrightness < MINBRIGHTNESS)
			__newBrightness = MINBRIGHTNESS;
		if (__newBrightness > MAXBRIGHTNESS)
			__newBrightness = MAXBRIGHTNESS;

		// float __contrastVal = MAXBRIGHTNESS * __newBrightness / 100;

		setContrast(__newBrightness);

		return;
	}	
}

/***************************************************
 * Draw Progress bar during data update
 ****************************************************/
void drawProgress(byte percentage, String label)
{
	// if (!_isDisplayOn) { return; }
	// _display.clear();
	// _display.setTextAlignment(TEXT_ALIGN_CENTER);
	// _display.setFont(ArialMT_Plain_10);
	// _display.drawString(64, 10, label);
	// _display.drawProgressBar(2, 28, 124, 10, percentage);
	// _display.display();
}

void DisplayOut(String outStr)
{
	if (!_isDisplayOn)
	{
		DEBUG_PRINTLN("Display is off, skipping display output");
		return;
	}
	_display.clearDisplay();
	_display.setTextSize(1);
	_display.setTextColor(SH110X_WHITE);
	_display.setCursor(DISPLAY_WIDTH / 2 - (outStr.length() * 6) / 2, DISPLAY_HEIGHT / 2 - 4);

	DEBUG_PRINTLN(outStr);
	_display.print(outStr);
	_display.display();
}

void showWiFiandMQTTStatusOnScreen()
{
	if (WiFi.status() == WL_CONNECTED) // draw WiFi Connected icon
	{
		_display.drawFastHLine(DISPLAY_WIDTH - 1, 0, 1, SH110X_WHITE);
	}
	if (_mqttClient.connected()) // draw MQTT Connected icon
	{
		_display.drawFastHLine(DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1, 1, SH110X_WHITE);
	}
}

// Display list of open contacts (friendly names or topic if no friendly name)
void showOpenContactsOnOLED()
{
	// if (!_isDisplayOn)
	// 	return;

	if (_totalOpenContacts > 0)
		setContrast(MAXBRIGHTNESS); // ensure display is on at full brightness when showing open contacts

	_display.clearDisplay();
	_display.setTextSize(1);
	_display.setTextColor(SH110X_WHITE);
	
	_totalOpenContacts =0;
	// Count open contacts
	for (size_t i = 0; i < _subscriptions.size(); i++)
	{
		if (!_subscriptions[i].contact)
			_totalOpenContacts++;
	}

	// Advance scroll offset on interval
	if (_totalOpenContacts > 0 && (millis() - _runOpenScroll >= _openScrollInterval))
	{
		_runOpenScroll = millis();
		_openScrollOffset = (_totalOpenContacts > 0) ? (_openScrollOffset + 1) % _totalOpenContacts : 0;
	}

	int y = 0;
	_display.setCursor(0, y);
	_display.print(String(_totalOpenContacts) + " Open:");
	// Current time HH:MM in top-right corner
	String _currentTime = _timeClient.getFormattedTime().substring(0, 5);
	_display.setCursor(DISPLAY_WIDTH - 30, y); // 5 chars * 6px = 30px
	_display.print(_currentTime);
	_display.drawFastHLine(0, 9, DISPLAY_WIDTH, SH110X_WHITE);

	y += 11;

	int shown = 0;
	int maxRows = (DISPLAY_HEIGHT - y) / 10; // how many rows we can show

	// If there are open items, display starting at _openScrollOffset
	if (_totalOpenContacts > 0)
	{
		// ensure scroll offset is within range
		if (_openScrollOffset >= _totalOpenContacts)
			_openScrollOffset = _openScrollOffset % _totalOpenContacts;
		int visibleRows = min(maxRows, _totalOpenContacts);
		for (int r = 0; r < visibleRows; r++)
		{
			int want = (_openScrollOffset + r) % _totalOpenContacts;
			// find want-th open subscription
			int count = 0;
			int idx = -1;
			for (size_t j = 0; j < _subscriptions.size(); j++)
			{
				if (!_subscriptions[j].contact)
				{
					if (count == want)
					{
						idx = j;
						break;
					}
					count++;
				}
			}
			if (idx == -1)
				break;

			String name = _subscriptions[idx].friendly.length() ? _subscriptions[idx].friendly : _subscriptions[idx].topic;
			if (name.length() > 34)
				name = name.substring(0, 32) + "..";

			// Extract time-only from lastSeen if possible and reduce to HH:MM
			String timeStr = "N/A";
			if (_subscriptions[idx].lastSeen.length() > 0)
			{
				int tPos = _subscriptions[idx].lastSeen.indexOf('T');
				if (tPos >= 0)
				{
					int endPos = _subscriptions[idx].lastSeen.indexOf('.', tPos);
					if (endPos == -1)
						endPos = _subscriptions[idx].lastSeen.indexOf('Z', tPos);
					if (endPos == -1)
						endPos = _subscriptions[idx].lastSeen.length();
					if (endPos > tPos + 1)
					{
						timeStr = _subscriptions[idx].lastSeen.substring(tPos + 1, endPos);
						int dot = timeStr.indexOf('.');
						if (dot != -1)
							timeStr = timeStr.substring(0, dot);
					}
				}
				else
				{
					if (_subscriptions[idx].lastSeen.length() >= 8)
						timeStr = _subscriptions[idx].lastSeen.substring(_subscriptions[idx].lastSeen.length() - 8);
				}
				// Reduce to HH:MM if possible
				int c1 = timeStr.indexOf(':');
				if (c1 >= 0)
				{
					int c2 = timeStr.indexOf(':', c1 + 1);
					if (c2 > 0)
						timeStr = timeStr.substring(0, c2); // keep HH:MM
					else if (timeStr.length() > 5)
						timeStr = timeStr.substring(0, 5);
				}
			}

			_display.setCursor(0, y);
			_display.print(name);
			_display.setCursor(DISPLAY_WIDTH - 30, y);
			_display.print(timeStr);
			y += 10;
			shown++;
		}
	}

	if (shown == 0)
	{
		_display.setCursor(0, y);
		_display.print("None");
	}
	else if (_totalOpenContacts > shown)
	{
		_display.setCursor(0, y);
		_display.print("+" + String(_totalOpenContacts - shown) + " more");
	}

	showWiFiandMQTTStatusOnScreen();

	_display.display();
}

// Draw a triangle
void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
	// _display.drawLine(x0, y0, x1, y1);
	// _display.drawLine(x1, y1, x2, y2);
	// _display.drawLine(x2, y2, x0, y0);
}

// Fill a triangle
void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{

	int16_t a, b, y, last;
	// Sort coordinates by Y order (y2 >= y1 >= y0)
	if (y0 > y1)
	{
		_swap_int16_t(y0, y1);
		_swap_int16_t(x0, x1);
	}
	if (y1 > y2)
	{
		_swap_int16_t(y2, y1);
		_swap_int16_t(x2, x1);
	}
	if (y0 > y1)
	{
		_swap_int16_t(y0, y1);
		_swap_int16_t(x0, x1);
	}

	if (y0 == y2)
	{ // Handle awkward all-on-same-line case as its own thing
		a = b = x0;
		if (x1 < a)
			a = x1;
		else if (x1 > b)
			b = x1;
		if (x2 < a)
			a = x2;
		else if (x2 > b)
			b = x2;
		_display.drawFastHLine(a, y0, b - a + 1, color);
		// writeFastHLine(a, y0, b - a + 1);
		return;
	}

	int16_t
		dx01 = x1 - x0,
		dy01 = y1 - y0,
		dx02 = x2 - x0,
		dy02 = y2 - y0,
		dx12 = x2 - x1,
		dy12 = y2 - y1;
	int32_t
		sa = 0,
		sb = 0;

	// For upper part of triangle, find scanline crossings for segments
	// 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
	// is included here (and second loop will be skipped, avoiding a /0
	// error there), otherwise scanline y1 is skipped here and handled
	// in the second loop...which also avoids a /0 error here if y0=y1
	// (flat-topped triangle).
	if (y1 == y2)
		last = y1; // Include y1 scanline
	else
		last = y1 - 1; // Skip it
	for (y = y0; y <= last; y++)
	{
		a = x0 + sa / dy01;
		b = x0 + sb / dy02;
		sa += dx01;
		sb += dx02;
		/* longhand:
		a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
		b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
		*/
		if (a > b)
			_swap_int16_t(a, b);
		_display.drawFastHLine(a, y, b - a + 1, color);
	}

	// For lower part of triangle, find scanline crossings for segments
	// 0-2 and 1-2.  This loop is skipped if y1=y2.
	sa = dx12 * (y - y1);
	sb = dx02 * (y - y0);
	for (; y <= y2; y++)
	{
		a = x1 + sa / dy12;
		b = x0 + sb / dy02;
		sa += dx12;
		sb += dx02;
		/* longhand:
		a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
		b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
		*/
		if (a > b)
			_swap_int16_t(a, b);
		_display.drawFastHLine(a, y, b - a + 1, color);
	}
}

void setupWebServer()
{
	DEBUG_PRINTLN("Setting up Web Server");

	_httpServer.on("/", []()
				   {
			String __infoStr;
		__infoStr.reserve(2048);
		__infoStr += "<html><head>"+_style;
             __infoStr += "<script>  ";
             __infoStr += "function checkFlipped() { document.getElementById('flipscreen').value=document.getElementById('flipscreenHidden').checked; }";
             __infoStr += "function submitForm() { checkFlipped(); document.getElementById('myForm').submit(); }";
             __infoStr += "function editFriendly(idx) { var cell = document.getElementById('friendly_' + idx); var current = cell.textContent || cell.innerText; cell.innerHTML = ''; var input = document.createElement('input'); input.id = 'fname_' + idx; input.type = 'text'; input.value = current; input.size = 20; cell.appendChild(input); var saveBtn = document.createElement('button'); saveBtn.textContent = 'Save'; saveBtn.onclick = function(){ saveFriendly(idx); }; cell.appendChild(saveBtn); var cancelBtn = document.createElement('button'); cancelBtn.textContent = 'Cancel'; cancelBtn.onclick = function(){ cancelEdit(idx, current); }; cell.appendChild(cancelBtn); }";
             __infoStr += "function saveFriendly(idx) { var val = document.getElementById('fname_' + idx).value; window.location = '/edittopic?idx=' + idx + '&name=' + encodeURIComponent(val); }";
             __infoStr += "function cancelEdit(idx, val) { var cell = document.getElementById('friendly_' + idx); cell.textContent = val; }";
             __infoStr +="</script>";
             __infoStr += "</head>";
					   __infoStr += "<div align=left><H1>" + String(MQTT_CLIENTNAME) + "<i> v" + String(MCMDVERSION,1)+"</i></H1>";
             __infoStr += loginIndex+loginIndex2;

					   __infoStr += "<hr class='new5'>";
             __infoStr += "<form action='/set' id='myForm'>";
					   __infoStr += "Vertically Flip Screen:&nbsp;&nbsp;<input id='flipscreenHidden' onclick='checkFlipped()' data-lpignore='true' name='flipscreenHidden' type='checkbox' value='true' width=20% ";
             __infoStr +=  String(_configFlipSreen==3?"checked":"")+"><input type='hidden' name='flipscreen' id='flipscreen' value='false' /><br>";

            __infoStr += "Screen brightness:&nbsp;&nbsp;";
            __infoStr += "<select id='brightness' name='brightness'>";
            for (int i = 0; i < 5; i++)
            {
                __infoStr += "<option value='"+String(_brightnesses[i])+"'"+ (_selectedBrightness==i?"selected='selected'":"") +">"+String(map(_brightnesses[i], 0, MAXBRIGHTNESS, 0, 100))+"%</option>";
            }
            
            __infoStr += "</select><br>";


             __infoStr += "<input type='submit' class='btn' value='Save setting(s)'>";
             __infoStr += "</form>";

            // Section to add MQTT subscription topics
            __infoStr += "<hr class='new5'><h3>MQTT Subscribe Topics</h3>";
            __infoStr += "<form action='/addtopic' id='addtopicform'>";
            __infoStr += "<input type='text' name='topic' placeholder='e.g. zigbee2mqtt/mcmdhome/0x00158d00053e7610' value='' size='48'>&nbsp;";
            __infoStr += "<input type='text' name='name' placeholder='Friendly name (e.g. Back door)' value='' size='20'>&nbsp;";
            __infoStr += "<input type='submit' class='btn' value='Add Topic'>";
            __infoStr += "</form>";

            // Legend for state colors
            __infoStr += "<div class='legend'><span class='dot dot-closed'></span>Closed (contact present)&nbsp;&nbsp;<span class='dot dot-open'></span>Open (no contact)</div>";

            // Control for OLED scroll interval
            __infoStr += "<div style='margin-top:8px;'>OLED scroll interval:&nbsp;";
            __infoStr += "<input id='scrollInterval' type='range' min='500' max='10000' step='250' value='" + String(_openScrollInterval) + "' oninput='document.getElementById(\'scrollIntervalVal\').innerText=this.value' />";
            __infoStr += "<span id='scrollIntervalVal'>" + String(_openScrollInterval) + "</span> ms &nbsp;";
            __infoStr += "<button class='btn' onclick='setScrollInterval()'>Set</button>";
            __infoStr += "</div>";

            // Add JS function for setting interval
            __infoStr += "<script>function setScrollInterval(){ var v=document.getElementById('scrollInterval').value; fetch('/setscrollinterval?ms='+v).then(function(){ location.reload(); }); }</script>"; 

            // Table of current topics (friendly name editable inline)
            __infoStr += "<table border=1 cellpadding=4 style='border-collapse:collapse; margin-top:8px;'><tr><th>Friendly name</th><th>Topic</th><th>State</th><th>Last Seen</th><th>Status</th><th>Action</th></tr>";
            for (size_t __i = 0; __i < _subscriptions.size(); __i++)
            {
                String __state = "N/A";
                String __dotClass = "";
                if (_subscriptions[__i].lastPayload) {
                    __state = (_subscriptions[__i].contact ? ("closed") : ("open"));
                    __dotClass = (_subscriptions[__i].contact ? ("dot-closed") : ("dot-open"));
                }
                String __lastSeen = "N/A";
                if (_subscriptions[__i].lastSeen.length() > 0) __lastSeen = _subscriptions[__i].lastSeen;
                String __status = "N/A";
                if (_subscriptions[__i].lastPayload) {
                    __status = (_subscriptions[__i].contact ? ("true") : ("false"));
                }
                __infoStr += "<tr><td id='friendly_" + String(__i) + "'>" + _subscriptions[__i].friendly + "</td>";
                __infoStr += "<td>" + _subscriptions[__i].topic + "</td>";
                __infoStr += "<td><span class='dot " + __dotClass + "'></span>" + __state + "</td>";
                __infoStr += "<td>" + __lastSeen + "</td>";
                __infoStr += "<td>" + __status + "</td>";
                __infoStr += "<td><a href='javascript:void(0)' onclick='editFriendly(" + String(__i) + ")'>Edit</a>&nbsp;|&nbsp;<a href='/togglecontact?idx=" + String(__i) + "'>Toggle</a>&nbsp;|&nbsp;<a href='/removetopic?idx=" + String(__i) + "'>Remove</a></td></tr>";
            }
            __infoStr += "</table>";
			__infoStr += "<hr class='new5'>";
			__infoStr += "Last Message Received:  <i>" + _lastMQTTMessage;
			__infoStr += "</i><br>Last Message Published: <i>" + _lastPublishedMQTTMessage;

			__infoStr += "</i><br><hr  class='new5'>";
			__infoStr += "Connected to: " + String(_networkConnection ? WIFI_ACCESSPOINT : WIFI_ACCESSPOINT1) + " (" + _rssiQualityPercentage + "%)<br>";
			__infoStr += _mqttClient.connected() ? "MQTT Connected" : "MQTT Disconnected <a href='/mqttreconnect'>(Reconnect)</a>";
			__infoStr += "<br>";
			__infoStr += "  IP Address: " + IpAddress2String(WiFi.localIP());
			__infoStr += "<br>  MAC Address: " + WiFi.macAddress();
			__infoStr += "</b></div>";

			String __retStr = __infoStr+"</html>";

			_httpServer.sendHeader("Connection", "close");
			_httpServer.send(200, "text/html", __retStr); });

	_httpServer.on("/serverIndex", HTTP_GET, []()
				   {
					   _httpServer.sendHeader("Connection", "close");
					   _httpServer.send(200, "text/html", serverIndex); });

	_httpServer.on("/reset", []()
				   {
					   String _webClientReturnString = "Resetting device";
					   _httpServer.send(200, "text/plain", _webClientReturnString);
					   ESP.restart();
					   delay(1000); });
	_httpServer.on("/resetSettings", []()
				   {
                   String _webClientReturnString = "Resetting Settings";
                   _httpServer.send(200, "text/plain", _webClientReturnString);

                   if (SPIFFS.exists("/config.ini"))
                   {
                     DEBUG_PRINTLN("Removing Configuration files from SPIFFS");
                     SPIFFS.remove("/config.ini");
                   } });

	_httpServer.on("/defaults", []()
				   {
                    String _webClientReturnString = "Resetting device to defaults";
                    _httpServer.send(200, "text/plain", _webClientReturnString); });

	// Set OLED scroll interval (ms)
	_httpServer.on("/setscrollinterval", []()
				   {
			String ms = _httpServer.arg("ms");
			if (ms.length() == 0) {
				_httpServer.send(400, "text/plain", "Missing ms parameter");
				return;
			}
			int msi = ms.toInt();
			if (msi < 100) msi = 100; // enforce a reasonable floor
			_openScrollInterval = (unsigned long) msi;
			_openScrollOffset = 0;
			_runOpenScroll = millis();
			saveConfigValuesSPIFFS();
			_httpServer.send(200, "text/plain", String(_openScrollInterval)); });

	// Heap diagnostics endpoint
	_httpServer.on("/heap", []()
				   {
			String _h = "freeHeap: " + String(ESP.getFreeHeap());
			size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
			_h += ", largest_free_block: " + String((unsigned long)largest);
			_httpServer.send(200, "text/plain", _h); });
	/*handling uploading firmware file */
	_httpServer.on(
		"/update", HTTP_POST, []()
		{
			_httpServer.sendHeader("Connection", "close");
			_httpServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
			ESP.restart(); },
		[]()
		{
			HTTPUpload &upload = _httpServer.upload();
			if (upload.status == UPLOAD_FILE_START)
			{
				DisplayOut("Updating Firmware");
				DEBUG_PRINT("Update: ");
				DEBUG_PRINTLN(upload.filename.c_str());
				if (!Update.begin(UPDATE_SIZE_UNKNOWN))
				{ // start with max available size
					Update.printError(Serial);
				}
			}
			else if (upload.status == UPLOAD_FILE_WRITE)
			{
				//_updatingFirmware = true;
				/* flashing firmware to ESP*/
				if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
				{
					Update.printError(Serial);
				}
			}
			else if (upload.status == UPLOAD_FILE_END)
			{
				if (Update.end(true))
				{ // true to set the size to the current progress
					DEBUG_PRINTLN("Update Success:" + String(upload.totalSize) + "\nRebooting...\n");
					//_updatingFirmware = false;
				}
				else
				{
					Update.printError(Serial);
				}
			}
		});

	_httpServer.on("/set", HTTP_GET, []()
				   {
			String __retMessage = "";
			String __val = "";
			bool __update = false;

			for (uint8_t i = 0; i < _httpServer.args(); i++) {
				__val = _httpServer.arg(i);
				String __key = _httpServer.argName(i);
				__update = parseConfigValue(__key, __val);
				__retMessage += " " + _httpServer.argName(i) + ": " + _httpServer.arg(i) + (__update ? " set." : " not set.") + "\n";
			}
			_httpServer.send(200, "text/plain", __retMessage);

			if (__update) {
				saveConfigValuesSPIFFS();
			} });

	// Handler to add a topic via GET /addtopic?topic=...&name=... (redirects back to /)
	_httpServer.on("/addtopic", HTTP_GET, []()
				   {
			String topic = _httpServer.arg("topic");
			String name = _httpServer.arg("name");
			if (topic.length() > 0) {
				addMQTTTopic(topic, name);
				saveConfigValuesSPIFFS();
			}
			_httpServer.sendHeader("Location", "/");
			_httpServer.send(302, "text/plain", ""); });

	// Handler to remove a topic via GET /removetopic?idx=N (redirects back to /)
	_httpServer.on("/removetopic", HTTP_GET, []()
				   {
			String __idx = _httpServer.arg("idx");
			int idx = __idx.toInt();
			removeMQTTTopic(idx);
			saveConfigValuesSPIFFS();
			_httpServer.sendHeader("Location", "/");
			_httpServer.send(302, "text/plain", ""); });

	// Handler to toggle a contact state for testing via GET /togglecontact?idx=N (redirects back to /)
	_httpServer.on("/togglecontact", HTTP_GET, []()
				   {
			String __idx = _httpServer.arg("idx");
			int idx = __idx.toInt();
			if (idx >= 0 && idx < (int)_subscriptions.size()) {
				// flip contact state
				_subscriptions[idx].contact = !_subscriptions[idx].contact;
				// update lastPayload to reflect change
				_subscriptions[idx].lastPayload = String("{\"contact\":") + (_subscriptions[idx].contact ? "true" : "false") + String("}");
				// set lastSeen to current NTP date/time if available
				_subscriptions[idx].lastSeen = _timeClient.getFormattedDate();
				DEBUG_PRINTLN("Toggled contact for: " + _subscriptions[idx].topic + " -> " + (_subscriptions[idx].contact ? "true" : "false"));
				// update display mode
				bool hasOpen = false;
				for (size_t __s = 0; __s < _subscriptions.size(); __s++) {
					if (!_subscriptions[__s].contact) { hasOpen = true; break; }
				}
				if (hasOpen) { _showOpenContacts = true; showOpenContactsOnOLED(); }
				else { _showOpenContacts = false; _display.clearDisplay(); _display.display(); }
			}
			_httpServer.sendHeader("Location", "/");
			//saveConfigValuesSPIFFS();
			_httpServer.send(302, "text/plain", ""); });

	_httpServer.on("/mqttreconnect", []()
				   {
			String _webClientReturnString = "Reconnecting MQTT...";
			_httpServer.send(200, "text/plain", _webClientReturnString);
			mqttReconnect(); });
}

void initWiFiServices()
{

	DEBUG_PRINTLN(WiFi.localIP());
	DEBUG_PRINTLN(WiFi.macAddress());

	DisplayOut("Starting DNS");
	MDNS.begin(_deviceClientName);

	DisplayOut("Getting NTP Time");
	setupTimeClient();
	getTimeUpdate();

	DEBUG_PRINT(F("********** Free Heap: "));
	DEBUG_PRINTLN(ESP.getFreeHeap());
	DEBUG_PRINTLN("ESP WiFi -> OTA Setup");
	DisplayOut("Setting up OTA updates");
	setupOTA();

	MDNS.addService("http", "tcp", 80);	

	DEBUG_PRINT(F("********** Free Heap: "));
	DEBUG_PRINTLN(ESP.getFreeHeap());
	DisplayOut("Web Server starting");
	setupWebServer();
	_httpServer.begin();
}

void keepWiFiAlive(void *parameters)
{
	for (;;)
	{
		if (WiFi.status() == WL_CONNECTED)
		{
			DEBUG_PRINTLN("keepWiFiAlive: WiFi connected");
			vTaskDelay(10000 / portTICK_PERIOD_MS);
			continue;
		}

		while (!isWiFiConnected())
		{
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			DEBUG_PRINTLN("keepWiFiAlive: WiFi not connected, attempting reconnect...");
		}
	}
}

void keepMQTTAlive(void *parameters)
{
	for (;;)
	{
		if (_mqttClient.connected())
		{
			_mqttClient.loop();
			DEBUG_PRINTLN("keepMQTTAlive: MQTT connected");
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}

		while (!_mqttClient.connected())
		{
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			DEBUG_PRINTLN("keepMQTTAlive: MQTT not connected, attempting reconnect...");
			mqttReconnect();
		}
	}
}

void toggleDisplay()
{
    if (_isDisplayOn)
    {
        setContrast(0);
        _isDisplayOn = false;
        DEBUG_PRINTLN("Touch: Display OFF");
    }
    else
    {
        DEBUG_PRINTLN("Touch: Display ON");
        setContrast(MAXBRIGHTNESS);
        _forceUpdate = true;
    }
}

void clearOpenContacts()
{
    for (size_t i = 0; i < _subscriptions.size(); i++)
    {
        _subscriptions[i].contact = true;
    }
    _showOpenContacts = false;
    _openScrollOffset = 0;
    _forceUpdate = true;
    DEBUG_PRINTLN("Touch: Cleared open contacts");
}

#ifdef ENABLE_TOUCH_SENSOR
// ---------- Touch handling functions ----------
// The Interrupt Service Routine (ISR)
void IRAM_ATTR handleInterrupt() {
  _interruptFlag = true; // Set the flag when interrupt occurs
  // Keep ISR short — set a flag and return
}
#endif // ENABLE_TOUCH_SENSOR

#ifdef ENABLE_TOUCH_SENSOR
void handleTouch()
{
    unsigned long now = millis();

    // If an interrupt signalled a rising edge, sample the pin and start press
    if (_interruptFlag) {
        _interruptFlag = false;
        bool raw = digitalRead(TOUCH_PIN);
        // Debounce and detect real state changes
        if (raw != _lastTouchRaw && (now - _touchLastChange) >= TOUCH_DEBOUNCE_MS) {
            _lastTouchRaw = raw;
            _touchLastChange = now;
            if (raw) {
                // Press began
                if (!_touchState) {
                    _touchState = true;
                    _touchPressedAt = now;
                    _longPressHandled = false;
                    DEBUG_PRINTLN("Touch: press detected");
                }
            }
        }
    }

    // If currently pressed, look for release or long-press
    if (_touchState) {
        bool raw = digitalRead(TOUCH_PIN);
        // Release detected (stable low)
        if (!raw && (now - _touchLastChange) >= TOUCH_DEBOUNCE_MS) {
            unsigned long duration = now - _touchPressedAt;
            _touchState = false;
            _lastTouchRaw = raw;
            _touchLastChange = now;

            if (!_longPressHandled) {
                if (duration >= TOUCH_LONG_PRESS_MS) {
                    DEBUG_PRINTLN("Touch: long press detected (on release) -> rebooting");
                    delay(50);
                    ESP.restart();
                } else {
                    // short press => count taps
                    _tapCount++;
                    _lastTapTime = now;
                    DEBUG_PRINTLN("Touch: tap registered (count=" + String(_tapCount) + ")");
                }
            }
        } else {
            // while still pressed, detect long press
            if (!_longPressHandled && (now - _touchPressedAt) >= TOUCH_LONG_PRESS_MS) {
                _longPressHandled = true;
                DEBUG_PRINTLN("Touch: long press detected (while pressed) -> rebooting");
                delay(50);
                ESP.restart();
            }
        }
    }

    // Handle single vs double tap after timeout
    if (_tapCount > 0 && (now - _lastTapTime) > TOUCH_DOUBLE_TAP_MS) {
        if (_tapCount == 1) {
            DEBUG_PRINTLN("Touch: single tap -> toggle display");
            toggleDisplay();
        } else if (_tapCount == 2) {
            DEBUG_PRINTLN("Touch: double tap -> clear open contacts");
            clearOpenContacts();
        } else {
            DEBUG_PRINTLN("Touch: " + String(_tapCount) + " taps - no action");
        }
        _tapCount = 0;
    }
}
#endif // ENABLE_TOUCH_SENSOR

void setup()
{

#ifdef DEBUG
	Serial.begin(115200);
	delay(2000); // Give serial time to start
#endif

	// Initialize touch input (TTP223 on D2/GPIO2). Output is typically HIGH on touch.
#ifdef ENABLE_TOUCH_SENSOR
	pinMode(TOUCH_PIN, INPUT_PULLDOWN); // change to INPUT_PULLUP if your module is active-LOW
	_lastTouchRaw = digitalRead(TOUCH_PIN);
	DEBUG_PRINTLN("Initial touch state: " + String(_lastTouchRaw == HIGH ? "NOT TOUCHED" : "TOUCHED"));
	attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), handleInterrupt, RISING);

	_touchState = false;

	_touchPressedAt = 0;
	_tapCount = 0;
	_longPressHandled = false;
#endif // ENABLE_TOUCH_SENSOR


	_deviceClientName = MQTT_CLIENTNAME;

	// IPAddress ip(192, 168, 1, 13);
	// IPAddress gateway(192, 168, 1, 1);
	// IPAddress subnet(255, 255, 255, 0);
	// IPAddress dns(192, 168, 1, 52);
	// WiFi.config(ip, gateway, subnet, dns);

	int __wifiRetryCount = 0;


	WiFi.begin(WIFI_ACCESSPOINT1, WIFI_ACCESSPOINT_PASSWORD);
	WiFi.setTxPower(WIFI_POWER_8_5dBm); // The "Magic Fix" for C3

    int retry_count = 0;
    while (WiFi.status() != WL_CONNECTED && retry_count < 20) {
        delay(500);
        Serial.print(".");
        retry_count++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
    } else {
        Serial.printf("\nFailed. Status code: %d\n", WiFi.status());
    }
	
	_mqttClient.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
	delay(5000);
	
	_mqttClient.setSocketTimeout(60);
	_mqttClient.setKeepAlive(60);
	_mqttClient.setBufferSize(512);
	
	_mqttClientId = String(MQTT_CLIENTNAME) + String(micros());
	//_mqttClientId += "_" + String(random(0xffff), HEX);
	DEBUG_PRINTLN("MQTT reconnecting client id [" + _mqttClientId + "]");

	delay(5000);
	

	// Attempt a single connect and return immediately (non-blocking behavior)
	if (_mqttClient.connect(_mqttClientId.c_str())) // , MQTT_USERNAME, MQTT_PASSWORD))
	{
		DEBUG_PRINTLN("MQTT initial connect successful");
	}
	else
	{
		DEBUG_PRINTLN("MQTT initial connect failed");
	}




	DEBUG_PRINTLN("Starting WiFi Services");
	initWiFiServices();

	DEBUG_PRINTLN("Loading configuration");
	setupSPIFFS();
	loadCustomParamsSPIFFS();

	// SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
	if (!_display.begin(0x3C))
	{
		DEBUG_PRINTLN(F("SSD1306 allocation failed"));
		delay(1000);
		ESP.restart();
	}

	
	_isDisplayOn = true;
	DisplayOut("Starting...");

	DisplayOut("Booting the Robot");
	_roboEyes.begin(DISPLAY_WIDTH, DISPLAY_HEIGHT, 100);

	// Define some automated eyes behaviour
	_roboEyes.setAutoblinker(ON, 6, 2); // Start auto blinker animation cycle -> bool active, int interval, int variation -> turn on/off, set interval between each blink in full seconds, set range for random interval variation in full seconds
	_roboEyes.setIdleMode(ON, 10, 2);	// Start idle animation cycle (eyes looking in random directions) -> turn on/off, set interval between each eye repositioning in full seconds, set range for random time interval variation in full seconds
	_roboEyes.setWidth(14, 14);
	//_roboEyes.setHeight(14, 14);
	_roboEyes.setHeight(17, 17);
	_roboEyes.setBorderradius(2, 2);
	_roboEyes.setSpacebetween(3);
	// Define mood, curiosity and position
	// roboEyes.setMood(DEFAULT); // mood expressions, can be TIRED, ANGRY, HAPPY, DEFAULT
	// roboEyes.setPosition(DEFAULT); // cardinal directions, can be N, NE, E, SE, S, SW, W, NW, DEFAULT (default = horizontally and vertically centered)
	// roboEyes.setCuriosity(ON); // bool on/off -> when turned on, height of the outer eyes increases when moving to the very left or very right
	// Play prebuilt oneshot animations
	//_roboEyes.anim_confused(); // confused - eyes shaking left and right
	_roboEyes.setCuriosity(true); // waiting for first PPM air quality reading
	// roboEyes.anim_laugh(); // laughing - eyes shaking up and down
	// roboEyes.anim_blink(); // blink - eyes blinking
	// roboEyes.anim_blink(ON); // blink - eyes blinking, bool on/off -> when turned on, eyes will blink in a loop
	// roboEyes.anim_blink(ON, 2); // blink - eyes blinking, bool on/off -> when turned on, eyes will blink in a loop, int interval -> set interval between each blink in full seconds
	// Set horizontal or vertical flickering
	// roboEyes.setHFlicker(ON, 2); // bool on/off, byte amplitude -> horizontal flicker: alternately displacing the eyes in the defined amplitude in pixels
	// roboEyes.setVFlicker(ON, 2); // bool on/off, byte amplitude -> vertical flicker: alternately displacing the eyes in the defined amplitude in pixels


	xTaskCreatePinnedToCore(keepMQTTAlive, "MQTTConnect", 4096, NULL, 1, NULL, CONFIG_ARDUINO_RUNNING_CORE);
	xTaskCreatePinnedToCore(keepWiFiAlive, "WiFiConnect", 4096, NULL, 1, NULL, CONFIG_ARDUINO_RUNNING_CORE);

	DisplayOut("Init Complete");
}


void loop()
{	
	_mqttClient.loop();
	ArduinoOTA.handle();		// this function will handle incomming chunk of SW, flash and respond sender */
	_httpServer.handleClient(); // Check if a client has connected

#ifdef ENABLE_TOUCH_SENSOR
	handleTouch();
#endif // ENABLE_TOUCH_SENSOR

	// re-runs program according to the value set for timebetweenruns
	_runCurrent = millis(); // sets the counter
	if (_runCurrent - _runWifiReconnect >= UPDATE_WIFICHECK_INTERVAL_MILLISECS)
	{
		_runWifiReconnect = millis();
		mqttReconnect();
	}

	if (_isDisplayOn)
	{
		if (_showOpenContacts)
		{
			showOpenContactsOnOLED();
		}
		else
		{
			_roboEyes.setCyclops(!(WiFi.status() == WL_CONNECTED && _mqttClient.connected()));
			_roboEyes.update(); // update eyes drawings
		}
	}

}
