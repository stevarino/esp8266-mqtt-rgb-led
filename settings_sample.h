/*
 settings_sample.h - ESP8266 MQTT N-Channel LED Settings File
 Change the settings below and save as settings.h to include 
 in the project.
*/

/*****************************************
 * WiFi Settings
 ****************************************/
#define WLAN_SSID       "ssid"
#define WLAN_PASSWORD   "password"

/*****************************************
 * MQTT Broker Settings
 ****************************************/
#define ARB_SERVER      "192.168.1.2"
#define ARB_SERVERPORT  1883
#define ARB_USERNAME    "homeassistant"
#define ARB_PASSWORD    "api_password"

/*****************************************
 * LED settings.
 ****************************************/

// pins that LEDs are connected to
const byte rgb_pins[] = {5, 4, 2};

// base mqtt topic, first led will be home/topic_name0 and
// home/topic_name0/set
String topic_base = "home/lantern";

// color aliases - useful to set a particular color across a range of LEDs. 
// syntax is subtopic, led start index, led end index, and color (0-1).
// topic will be appended to base topic. end can be set to -1 for end of array.
RGB_LED_ALIAS aliases[] = {
    {"red_light", 0, -1, {1, 0, 0}}
};

// default transition time in ms.
const float default_transition = 3000.0;

// curve applied to led scaling, 2.0 is quadratic, 0.5 is 
// square-rooted, 1.0 is linear
const float ramp_coefficient = 2.0;

// language settings
const char* on_cmd = "ON";
const char* off_cmd = "OFF";
