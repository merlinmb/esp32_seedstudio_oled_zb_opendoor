#pragma once
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#endif

#ifdef ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#endif

#ifdef SECURE
#include <WiFiClientSecure.h>
#endif

//#include <WiFiUdp.h>
//#include <ArduinoOTA.h>

#include <PubSubClient.h>

#include <NTPClient.h>
#include <TimeLib.h>
#include <Time.h>

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

//#define MQTT_MAX_PACKET_SIZE 512

byte _networkConnection = 1;

#define MQTT_SERVER_IP "192.168.1.55"
#define MQTT_SERVER_PORT 1883
#define MQTT_FRIENDLYNAME "mcmddevices"

#define OTA_UPDATE_WEBPATH "/firmware"
#define HTTPRESPONSETIMEOUT 450

const char *_deviceClientName = "espTestDevice";

#ifdef SECURE
WiFiClientSecure _espWiFiClient;
#else
WiFiClient _espWiFiClient;
#endif

PubSubClient _mqttClient(_espWiFiClient);
String _mqttClientId = "";
String _lastReceivedMQTTMessage = "";
String _lastPublishedMQTTMessage = "";

int _mqttConnectionRetries = 0;

byte _ledBuiltIn = 12;



// NTP client: offset is set dynamically by checkAndApplyBST(), so initialise to 0.
// Update interval 60 s keeps the epoch counter accurate without hammering the pool.
WiFiUDP ntpUDP;
const char* _timeServer = "pool.ntp.org";
NTPClient _timeClient(ntpUDP, _timeServer, 0, 60000);
int _ntpTimeOffset = 0; // tracks the offset currently applied to _timeClient
unsigned long _unixTime;
String _ntpDate = "";
char timeHour[3];
char timeMin[3];
char timeSec[3];
char timeday[3];
char timemonth[10];
char timeyear[5];
char timeWeekDay[10];
int dayInWeek;


// Measure Signal Strength (RSSI) of Wi-Fi connection
long _rssi = 0;
// Measure Signal Strength (RSSI) of Wi-Fi connection
long _rssiQualityPercentage = 0;

const int RSSI_MAX = -50;  // define maximum strength of signal in dBm
const int RSSI_MIN = -100; // define minimum strength of signal in dBm

// void ICACHE_RAM_ATTR AS3935Irq();
int dBmtoPercentage(int dBm)
{
	int quality = 2 * (dBm + 100);

	if (dBm <= RSSI_MIN)
	{
		quality = 0;
	}
	else if (dBm >= RSSI_MAX)
	{
		quality = 100;
	}

	return quality;
} // dBmtoPercentage

String macToStr(const uint8_t *mac)
{
	String result;
	for (int i = 0; i < 6; ++i)
	{
		result += String(mac[i], 16);
	}
	return result;
}

void updateTimeString()
{
	//_ntpDate = String(year(_unixTime)) + "/" + String(month(_unixTime)) + "/" + String(day(_unixTime));
	_ntpDate = _timeClient.getFormattedDate();
	_ntpDate = _ntpDate.substring(0, 16);
	_ntpDate.trim();
	DEBUG_PRINTLN(_ntpDate);
}

String getTimefromEpoch(long int timeData)
{

	time_t t = timeData;
	// time_t Z = 1515675789; //linux epoch time
	/*
	printDigits(day(t));
	tft.print("/");
	printDigits(month(t));
	tft.print("/");
	tft.print(year(t));
	tft.print(" ");
	*/
	byte __hour = hour(t) % 24;
	return String(__hour) + ":" + (minute(t) < 10 ? "0" : "") + String(minute(t));
	;
}

unsigned long webUnixTime(Client &client)
{
	unsigned long time = 0;

	// Just choose any reasonably busy web server, the load is really low
	if (client.connect("g.cn", 80))
	{
		// Make an HTTP 1.1 request which is missing a Host: header
		// compliant servers are required to answer with an error that includes
		// a Date: header.
		client.print(F("GET / HTTP/1.1 \r\n\r\n"));

		char buf[5]; // temporary buffer for characters
		client.setTimeout(500);
		if (client.find((char *)"\r\nDate: ") // look for Date: header
			&& client.readBytes(buf, 5) == 5) // discard
		{
			unsigned day = client.parseInt(); // day
			client.readBytes(buf, 1);		  // discard
			client.readBytes(buf, 3);		  // month
			int year = client.parseInt();	  // year
			byte hour = client.parseInt();	  // hour
			byte minute = client.parseInt();  // minute
			byte second = client.parseInt();  // second

			int daysInPrevMonths;
			switch (buf[0])
			{
			case 'F':
				daysInPrevMonths = 31;
				break; // Feb
			case 'S':
				daysInPrevMonths = 243;
				break; // Sep
			case 'O':
				daysInPrevMonths = 273;
				break; // Oct
			case 'N':
				daysInPrevMonths = 304;
				break; // Nov
			case 'D':
				daysInPrevMonths = 334;
				break; // Dec
			default:
				if (buf[0] == 'J' && buf[1] == 'a')
					daysInPrevMonths = 0; // Jan
				else if (buf[0] == 'A' && buf[1] == 'p')
					daysInPrevMonths = 90; // Apr
				else
					switch (buf[2])
					{
					case 'r':
						daysInPrevMonths = 59;
						break; // Mar
					case 'y':
						daysInPrevMonths = 120;
						break; // May
					case 'n':
						daysInPrevMonths = 151;
						break; // Jun
					case 'l':
						daysInPrevMonths = 181;
						break; // Jul
					default:   // add a default label here to avoid compiler warning
					case 'g':
						daysInPrevMonths = 212;
						break; // Aug
					}
			}

			// This code will not work after February 2100
			// because it does not account for 2100 not being a leap year and because
			// we use the day variable as accumulator, which would overflow in 2149
			day += (year - 1970) * 365; // days from 1970 to the whole past year
			day += (year - 1969) >> 2;	// plus one day per leap year
			day += daysInPrevMonths;	// plus days for previous months this year
			if (daysInPrevMonths >= 59	// if we are past February
				&& ((year & 3) == 0))	// and this is a leap year
				day += 1;				// add one day
										// Remove today, add hours, minutes and seconds this month
			time = (((day - 1ul) * 24 + hour) * 60 + minute) * 60 + second;
		}
	}
	// delay(10);
	client.flush();
	client.stop();

	return time;
}

void getTimeUpdate()
{
	int _retryCount = 0;
	while (!_timeClient.forceUpdate() && _retryCount < 10)
	{
		_retryCount++;
		DEBUG_PRINTLN("NTP Time update failed. Retrying...");
		delay(300);
	}

	DEBUG_PRINTLN(_timeClient.getFormattedTime());

	_unixTime = webUnixTime(_espWiFiClient);

	setTime(_unixTime);
}

void mqttCallback(char *topic, byte *payload, unsigned int length);
void mqttTransmitCustomStat();

void mqttTransmitCustomSubscribe();

void RenderHeadingtoSprite();

String IpAddress2String(const IPAddress &ipAddress)
{
	return String(ipAddress[0]) + String(".") +
		   String(ipAddress[1]) + String(".") +
		   String(ipAddress[2]) + String(".") +
		   String(ipAddress[3]);
}

void mqttSubscribe(String _topic)
{
	DEBUG_PRINTLN("Subscribing to: " + _topic);
	_mqttClient.subscribe(_topic.c_str());
	delay(50);
}

void mqttCustomPublish(String topic, String topicPostfix, String value, bool retain)
{

	DEBUG_PRINTLN("mqttCustom: Sending ");
	String __ret = (topic + "/" + topicPostfix);
	String __retVal = value;
	DEBUG_PRINTLN(__ret + " " + __retVal);
	_mqttClient.publish(__ret.c_str(), __retVal.c_str(), retain);
	_lastPublishedMQTTMessage = __ret + " " + __retVal;
	DEBUG_PRINTLN("mqttCustom: Sent");
}

void mqttPublishStat(String deviceName, String topicPostFix, String value, bool retain)
{	
	String __ret = ("stat/" + deviceName + "/" + topicPostFix);
	String __retVal = value;
	
	_mqttClient.publish(__ret.c_str(), __retVal.c_str(), retain);
	_lastPublishedMQTTMessage = __ret + " " + __retVal;
	DEBUG_PRINT("MQTT Sent: ");
	DEBUG_PRINTLN(_lastPublishedMQTTMessage);
}

void mqttPublishStat(String name, String value, bool retain)
{
	mqttPublishStat(_deviceClientName, name, value, retain);
}

void mqttPublishStat(String deviceName, String name, String value)
{
	mqttPublishStat(deviceName, name, value, false);
}
void mqttPublishStat(String name, String value)
{
	mqttPublishStat(_deviceClientName, name, value);
}

void mqttHomePublish(String name, String value, bool retain)
{
	//DEBUG_PRINTLN("MQTT Sending ");
	String __ret = ("stat/home/" + name);
	DEBUG_PRINTLN(__ret + " " + value);
	_mqttClient.publish(__ret.c_str(), value.c_str(), retain);
	_lastPublishedMQTTMessage = __ret + " " + value;
	//DEBUG_PRINTLN("MQTT: Sent");
}

void mqttHomePublish(String name, String value)
{
	mqttHomePublish(name, value, false);
}

void mqttTransmitInitStat(String deviceName)
{
	mqttPublishStat("init", "{\"value1\":\"" + IpAddress2String(WiFi.localIP()) + "\",\"value2\":\"" + WiFi.macAddress() + "\",\"value3\":\"" + deviceName + "\"}");
}

void mqttTransmitInitStat()
{
	mqttTransmitInitStat(_deviceClientName);
}

void tickLED()
{
	// toggle state
	int state = digitalRead(_ledBuiltIn); // get the current state of GPIO1 pin
	if (WiFi.status() == WL_CONNECTED)	  // only switch LED if Wifi is connected
	{
		digitalWrite(_ledBuiltIn, !state); // set pin to the opposite state
	}
	else
	{
		// Turn the LED on (Note that LOW is the voltage level
		// but actually the LED is on; this is because
		// it is acive low on the ESP-01)
		digitalWrite(_ledBuiltIn, HIGH); // Turn the LED off by making the voltage HIGH
	}
}

void mqttHandleFriendlyCallback(String deviceName, String mqttIncomingTopic, String mqttIncomingPayload)
{
	if (mqttIncomingTopic == "cmnd/" + String(MQTT_FRIENDLYNAME) + "/sendstat")
	{
		mqttTransmitInitStat(deviceName);
		return;
	}
	if (mqttIncomingTopic == "cmnd/" + String(MQTT_FRIENDLYNAME) + "/reset")
	{
		mqttPublishStat(deviceName, "restart", mqttIncomingPayload);
		delay(1000);
#ifdef ESP8266
		ESP.reset();
#endif
#ifdef ESP32
		ESP.restart();
#endif
	}
}
void mqttHandleFriendlyCallback(String mqttIncomingTopic, String mqttIncomingPayload)
{
	mqttHandleFriendlyCallback(_deviceClientName, mqttIncomingTopic, mqttIncomingPayload);
}

bool mqttReconnect(String deviceName)
{
	//_mqttClient.setSocketTimeout(1500);
	//_mqttClient.setKeepAlive(45);

	// If already connected, nothing to do
	if (_mqttClient.connected()) return true;
	DEBUG_PRINTLN("Attempting MQTT reconnect...");
	
	_mqttClientId = deviceName+ String(micros());
	//_mqttClientId += "_" + String(random(0xffff), HEX);
	DEBUG_PRINTLN("MQTT reconnecting client id [" + _mqttClientId + "]");
	

	// Attempt a single connect and return immediately (non-blocking behavior)
	if (_mqttClient.connect(_mqttClientId.c_str())) // , MQTT_USERNAME, MQTT_PASSWORD))
	{
		DEBUG_PRINTLN("MQTT: Connected");

		DEBUG_PRINTLN("MQTT: Setting callback");
		_mqttClient.setCallback(mqttCallback);

		DEBUG_PRINTLN("MQTT: subscribing...");
		// Once connected, publish an announcement...
		String __outMessage = "tele/" + String(_deviceClientName) + "/alive";
		_mqttClient.publish(__outMessage.c_str(), "1");
		__outMessage = "tele/" + String(_deviceClientName) + "/ip";
		_mqttClient.publish(__outMessage.c_str(), IpAddress2String(WiFi.localIP()).c_str());

		mqttSubscribe("cmnd/" + deviceName + "/#");
		mqttSubscribe("cmnd/" + String(MQTT_FRIENDLYNAME) + "/#");

		mqttTransmitInitStat(deviceName);
		mqttTransmitCustomStat();
		mqttTransmitCustomSubscribe();
		return true;
	}
	else
	{
		DEBUG_PRINT("MQTT: Connect failed, rc=");
		DEBUG_PRINTLN(_mqttClient.state());
		// Caller should retry later; do not block here
		return false;
	}
}

bool mqttReconnect()
{
	return mqttReconnect(_deviceClientName);
}

void flipAPDetails()
{
	_networkConnection = !_networkConnection;
}

int isWiFiConnected(String deviceName, String apName, String apPassword)
{
	if (WiFi.status() == WL_CONNECTED)
	{
		DEBUG_PRINT("WiFi connected to '" + apName + "' with IP address: ");
		DEBUG_PRINTLN(WiFi.localIP());
		return true;
	}
	else
	{
		DEBUG_PRINTLN("WiFi not connected, attempting connection...");
	}

	DEBUG_PRINTLN("\r\nConnecting to: " + String(apName));
	WiFi.begin(apName.c_str(), apPassword.c_str());

	byte counter = 0;
	while (WiFi.status() != WL_CONNECTED)
	{
		DEBUG_PRINT(F("."));
		counter++;

		if (counter == 10)
		{
			// ESP.restart();
			DEBUG_PRINTLN();
			return 0;
		}
		delay(150);
	}

	DEBUG_PRINTLN();
	DEBUG_PRINTLN("WiFi connected\r\n");
	DEBUG_PRINT("IP address: ");
	DEBUG_PRINTLN(WiFi.localIP());

	_rssi = WiFi.RSSI();
	DEBUG_PRINT(_rssi); // Signal strength in dBm
	DEBUG_PRINT(" dBm (");

	_rssiQualityPercentage = dBmtoPercentage(_rssi);

	DEBUG_PRINT(_rssiQualityPercentage); // Signal strength in %
	DEBUG_PRINTLN("%)");

	return 1;
}

int isWiFiConnected(String deviceName)
{
	String __apName = _networkConnection ? WIFI_ACCESSPOINT : WIFI_ACCESSPOINT1;
	String __apPassword = _networkConnection ? WIFI_ACCESSPOINT_PASSWORD : WIFI_ACCESSPOINT_PASSWORD1;

	return isWiFiConnected(deviceName, __apName, __apPassword);
}

int isWiFiConnected()
{
	return isWiFiConnected(_deviceClientName);
}

String getWiFIAPName()
{
	return (_networkConnection ? WIFI_ACCESSPOINT : WIFI_ACCESSPOINT1);
}

bool isLeapYear(int year)
{
	return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int zellersCongruence(int year, int month, int day) {
  if (month <= 2) {
    month += 12;
    year--;
  }
  
  int k = year % 100;
  int j = year / 100;
  
  int dayOfWeek = (day + 13*(month + 1)/5 + k + k/4 + j/4 + 5*j ) % 7 ;
  
  return dayOfWeek ; // Convert to Arduino's date format (Sat = 1, Sunday = 1, ...)
}

#define LEAP_YEAR(Y)     ( (Y>0) && !(Y%4) && ( (Y%100) || !(Y%400) ))     // from time-lib

#define LEAP_YEAR(Y)     ( (Y>0) && !(Y%4) && ( (Y%100) || !(Y%400) ))     // from time-lib

int calcDayOfWeek(uint16_t year, uint8_t month, uint8_t day)
{
  uint16_t months[] = 
  {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 // days until 1st of month
  };   

  uint32_t days = year * 365;   // days until year 
  for (uint16_t i = 4; i < year; i+=4) 
  {
    if (LEAP_YEAR(i)) days++;  // adjust leap years
  }

  days += months[month-1] + day;
  if ((month > 2) && LEAP_YEAR(year)) days++;
  return days % 7;
}

int lastSunday(int month, int year)
{
	int lastDay = 31; // Initialize lastDay with a default value

	// Adjust lastDay for months with less than 31 days
	if (month == 4 || month == 6 || month == 9 || month == 11)
	{
		lastDay = 30;
	}
	else if (month == 2)
	{
		lastDay = (isLeapYear(year) ? 29 : 28);
	}
	
	
	for (int day = lastDay; day > 0; day--) {
    	//if (zellersCongruence(year, month, day) == 1) 
		if(calcDayOfWeek(year, month, day)==1)
		{
			return day;
		}
	}
	

	return lastDay;
}

bool isBST(struct tm *timeinfo)
{

	int year = 1900 + timeinfo->tm_year;
	int month = timeinfo->tm_mon + 1; // tm_mon is 0-indexed (0 for January, 11 for December)
	int dayOfMonth = timeinfo->tm_mday;

	  //if (month < 3 || month > 10) return false; // BST is not in effect
	/*
	DEBUG_PRINTLN("isBST()");
	DEBUG_PRINTLN("Year: "+String(year));
	DEBUG_PRINTLN("Month: "+String(month));
	DEBUG_PRINTLN("dayOfMonth: "+String(dayOfMonth));
	DEBUG_PRINTLN("  last sunday: 01-" + String(lastSunday(1,2024)));
	DEBUG_PRINTLN("  last sunday: 02-" + String(lastSunday(2,2024)));
	DEBUG_PRINTLN("  last sunday: 03-" + String(lastSunday(3,2024)));
	DEBUG_PRINTLN("  last sunday: 04-" + String(lastSunday(4,2024)));
	DEBUG_PRINTLN("  last sunday: 05-" + String(lastSunday(5,2024)));
	DEBUG_PRINTLN("  last sunday: 06-" + String(lastSunday(6,2024)));
	DEBUG_PRINTLN("  last sunday: 07-" + String(lastSunday(7,2024)));
	DEBUG_PRINTLN("  last sunday: 08-" + String(lastSunday(8,2024)));
	DEBUG_PRINTLN("  last sunday: 09-" + String(lastSunday(9,2024)));
	DEBUG_PRINTLN("  last sunday: 10-" + String(lastSunday(10,2024)));
	DEBUG_PRINTLN("  last sunday: 11-" + String(lastSunday(11,2024)));
	DEBUG_PRINTLN("  last sunday: 12-" + String(lastSunday(12,2024)));
	//DEBUG_PRINTLN("  last sunday: 2024 03-" + String(lastSunday(3,2024)));
	*/

	if (month > 3 && month < 10)
		return true; // BST is in effect
	// BST transition months (March and October)

	if (month == 3 )
	{
		return (dayOfMonth>= lastSunday(month, year));
	}
	if (month == 10)
	{
		return (dayOfMonth < lastSunday(month, year));
	}

	return false;
}

// Apply the correct UTC offset for BST/GMT and return whether BST is active.
// Call once at boot (setupTimeClient) and then periodically (e.g. hourly) from
// the main loop so the offset tracks daylight-saving changes automatically.
bool checkAndApplyBST()
{
	// Derive raw UTC epoch by subtracting the currently applied offset.
	unsigned long utcEpoch = _timeClient.getEpochTime() - (unsigned long)_ntpTimeOffset;
	time_t t = (time_t)utcEpoch;
	struct tm *utcTm = gmtime(&t);
	if (!utcTm)
	{
		DEBUG_PRINTLN("checkAndApplyBST: gmtime failed");
		return false;
	}

	bool __bst = isBST(utcTm);
	int newOffset = __bst ? 3600 : 0;
	if (_ntpTimeOffset != newOffset)
	{
		_ntpTimeOffset = newOffset;
		_timeClient.setTimeOffset(newOffset);
		DEBUG_PRINTLN("BST offset updated: " + String(newOffset) + "s (" + String(__bst ? "BST" : "GMT") + ")");
	}
	return __bst;
}

void setupTimeClient()
{
	DEBUG_PRINTLN("Fetching NTP time");
	_timeClient.begin();

	int _retries = 0;
	while (!_timeClient.forceUpdate() && _retries < 5)
	{
		_retries++;
		DEBUG_PRINTLN(F("NTP delay"));
		delay(350);
	}

	// Now that we have a valid UTC epoch, determine and apply the BST offset.
	bool __bst = checkAndApplyBST();
	DEBUG_PRINTLN("Daylight Saving is " + String(__bst ? "" : "not ") + "applied");
}

void setupMQTT(String deviceName, String mqttServerIP, uint16_t mqttServerPort)
{
	DEBUG_PRINT("MQTT client configured as (" + deviceName + ") & connecting to: ");
	DEBUG_PRINT(mqttServerIP.c_str());
	DEBUG_PRINT(":");
	DEBUG_PRINTLN(mqttServerPort);

	//_mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
	_mqttClient.setServer(mqttServerIP.c_str(), mqttServerPort);
	
}

void setupMQTT(String mqttServerIP, uint16_t mqttServerPort)
{
	setupMQTT(_deviceClientName, mqttServerIP, mqttServerPort);
}

void setupMQTT()
{

	setupMQTT( MQTT_SERVER_IP, MQTT_SERVER_PORT);
}

void mqttSendInitStat()
{
	mqttTransmitInitStat();
}
