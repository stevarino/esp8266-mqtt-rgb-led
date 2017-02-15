/*
   ESP8266 MQTT Multiple Channel Lights for Home Assistant

   Allows multiple LED strands to be controlled using an ESP8266
   and MQTT broker (Home Assistant). 
   
   Based on code from:
     https://github.com/corbanmailloux/esp-mqtt-rgb-led
*/

// https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>
// https://github.com/esp8266/Arduino
#include <ESP8266WiFi.h>

// http://pubsubclient.knolleary.net/
#include <PubSubClient.h>

String client_id;
const int BUFFER_SIZE = JSON_OBJECT_SIZE(12);

#include "rgb_led.h"
#include "settings.h"

// array management
const int pin_count = sizeof(rgb_pins) == 0 
        ? 0 
        : sizeof(rgb_pins) / sizeof(rgb_pins[0]);

const int alias_count = sizeof(aliases) == 0 
        ? 0 
        : sizeof(aliases) / sizeof(aliases[0]);

const int led_count = pin_count / 3;

RGB_LED rgb_leds[led_count];

WiFiClient espClient;
byte wifi_mac[6] = {0, 0, 0, 0, 0, 0};
PubSubClient client(espClient);

const int millis_step = 50;
unsigned long millis_prev = 0;

/**
 * Setup function - runs once.
 */
void setup()
{
    Serial.begin(115200);
    Serial.printf("Setting up %d pins.\n", pin_count);

    RGB_LED::set_default_transition(default_transition);
    RGB_LED::set_ramp_coefficient(ramp_coefficient);
    RGB_LED::set_millis_step(millis_step);
    RGB_LED::set_on_cmd(String(on_cmd));
    RGB_LED::set_off_cmd(String(off_cmd));
    Serial.printf("Setting up %d LEDs.\n", led_count);
    for (int i = 0; i < pin_count; i += 3)
    {
        rgb_leds[i].init(topic_base.c_str(), i/3, rgb_pins[i], 
                rgb_pins[i + 1], rgb_pins[i + 2]);

        Serial.printf("Set up LED %d (%d, %d, %d).\n", 
                i/3, rgb_pins[i], rgb_pins[i + 1], rgb_pins[i + 2]);
    }
    analogWriteRange(255);

    setup_wifi();
    WiFi.macAddress(wifi_mac);
    client_id = "ESP8266_" + bytes_to_string(wifi_mac, 6, "");
    Serial.println("MAC Address: " + bytes_to_string(wifi_mac, 6, ":"));
    client.setServer(ARB_SERVER, ARB_SERVERPORT);
    client.setCallback(mqtt_callback);
}

/**
 * Loop function - runs repeatedly.
 */
void loop()
{
    if (!client.connected())
    {
        mqtt_reconnect();
    }
    client.loop();

    led_loop();
}

/**
 * Esatblishes WiFi connection. Called in setup()
 */
void setup_wifi()
{
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WLAN_SSID);

    WiFi.begin(WLAN_SSID, WLAN_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

/*
  SAMPLE PAYLOAD:
  {
    "brightness": 120,
    "transition": 5,
    "state": "ON"
  }
*/

/**
 * Recveive a message from the MQTT library and parse it.
 */
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("\nMessage arrived [");
    Serial.print(topic);
    Serial.print("] ");

    char message[length + 1];
    for (uint8_t i = 0; i < length; i++)
    {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    Serial.println(message);

    RGB_LED_ALIAS alias = {"default", 0, -1, {1, 1, 1}};

    if (!isDigit(topic[topic_base.length()]) && topic[topic_base.length()] != '/')
    {
        Serial.println("ERROR: Topic does not include an index.");
        return;
    }

    String target = String(topic).substring(topic_base.length());
    if (isDigit(target[0]))
    {
        alias.index_start = String(topic).substring(topic_base.length()).toInt();
        alias.index_end = alias.index_start + 1;
    }
    else
    {
        for (int i=0; i<alias_count; i++)
        {
            String t = topic_base + "/" + aliases[i].topic_state + "/set";
            if (t.equals(topic))
            {
                alias = aliases[i];
                break;
            }
        }
    }
    Serial.println("Targeting index " + String(alias.topic_state));

    if (!mqtt_process(message, alias))
    {
        Serial.println("ERROR: Unable to parse json.");
        return;
    }
}

/**
 * Process individual settings from an MQTT message.
 */
bool mqtt_process(char *message, RGB_LED_ALIAS alias)
{
    StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

    JsonObject &json = jsonBuffer.parseObject(message);

    if (!json.success())
    {
        Serial.println("parseObject() failed");
        return false;
    }

    if ((!json.containsKey("brightness")) && (!json.containsKey("color")) && 
            json.containsKey("state"))
    {
        json["brightness"] = 255;
        JsonObject& color = json.createNestedObject("color");
        color["r"] = 255 * alias.scale[0];
        color["g"] = 255 * alias.scale[1];
        color["b"] = 255 * alias.scale[2];
    }

    int led_start = alias.index_start;
    int led_end = alias.index_end == -1 
            ? led_count 
            : alias.index_end;

    for (int j = led_start; j < led_end; j++)
    {
        rgb_leds[j].process_json(json, alias.scale[0], alias.scale[1], alias.scale[2]);
        mqtt_respond(&rgb_leds[j]);
    }
    RGB_LED::update_root(rgb_leds, led_count);
    mqtt_respond_base();
    for (int i = 0; i < alias_count; i++) 
    {
        mqtt_respond_alias(aliases[i], aliases[i].topic_state.equals(
                alias.topic_state));
    }

    return true;
}

/**
 * Send a single LED status back to the MQTT broker.
 */
void mqtt_respond(RGB_LED *led)
{
    StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

    JsonObject &json = jsonBuffer.createObject();

    json["state"] = led->get_is_on() ? on_cmd : off_cmd;

    json["brightness"] = led->get_brightness();
    
    JsonObject& color = json.createNestedObject("color");
    color["r"] = (int) led->get_red();
    color["g"] = (int) led->get_green();
    color["b"] = (int) led->get_blue();

    char buffer[json.measureLength() + 1];
    json.printTo(buffer, sizeof(buffer));

    Serial.print("Message sent [");
    Serial.print(led->get_topic());
    Serial.print("] {");
    Serial.print(buffer);
    Serial.println("}");
    client.publish(led->get_topic().c_str(), buffer, true);
}

/**
 * Send the combined LED status back to the MQTT broker.
 */
void mqtt_respond_base()
{
    RGB_LED r = RGB_LED();
    r.set_is_on(false);
    r.set_color(0,0,0);
    r.set_topic(topic_base);

    r.set_is_on(RGB_LED::get_root_is_on());
    r.set_brightness(RGB_LED::get_root_brightness());
    r.set_color(RGB_LED::get_root_red(), RGB_LED::get_root_green(), 
            RGB_LED::get_root_blue());

    mqtt_respond(&r);
}

/**
 * Send the status of the alias to the broker (on/off only)
 */
void mqtt_respond_alias(RGB_LED_ALIAS alias, bool is_set)
{
    String t = topic_base + "/" + alias.topic_state;
    StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

    JsonObject &json = jsonBuffer.createObject();

    json["state"] = (RGB_LED::get_root_is_on() * is_set) ? on_cmd : off_cmd;
    char buffer[json.measureLength() + 1];
    json.printTo(buffer, sizeof(buffer));

    Serial.print("Message sent [");
    Serial.print(t);
    Serial.printf("] (%d/%d) {", RGB_LED::get_root_is_on(), is_set);
    Serial.print(buffer);
    Serial.println("}");
    client.publish(t.c_str(), buffer, true);
}

/**
 * Connect to the MQTT broker.
 */
void mqtt_reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection as " + client_id + "... ");
        // Attempt to connect
        if (client.connect(client_id.c_str(), ARB_USERNAME, ARB_PASSWORD))
        {
            Serial.println("connected");
            client.subscribe((topic_base + "/set").c_str());
            Serial.println("Registered base topic");

            // leds
            for (int i = 0; i < led_count; i++)
            {
                String topic_set = rgb_leds[i].get_topic() + "/set";
                client.subscribe(topic_set.c_str());
                Serial.println("Registered topic "+topic_set+".");
            }

            // aliases
            for (int i = 0; i < alias_count; i++) 
            {
                String topic_set = topic_base + "/" + aliases[i].topic_state + "/set";
                client.subscribe(topic_set.c_str());
                Serial.println("Registered topic "+topic_set+".");
            }
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

/**
 * Determine if iteration needs to be performed on an LED.
 */
void led_loop()
{
    unsigned long mil = millis();
    // first run or rollover.
    if (millis_prev == 0 || millis_prev > mil)
    {
        millis_prev = mil;
        return;
    }

    if ((mil - millis_prev) > millis_step)
    {
        millis_prev = mil;
        for (int i = 0; i < led_count; i++)
        {
            rgb_leds[i].loop();
        }
    }
}

/**
 * General library function.
 * Converts an array of bytes ({253, 12, 4}) to a string ("FD0C04")
 */
String bytes_to_string(byte *b, int n, String sep)
{
    int sep_size = sep.length();

    char c[2 * n + (n - 1) * sep_size + 1];
    c[2 * n + (n - 1) * sep_size] = 0;

    for (int i = 0; i < n; i++)
    {
        if (i > 0)
        {
            for (int j = 0; j < sep_size; j++)
            {
                c[(i - 1) * (2 + sep_size) + 2 + j] = sep[j];
            }
        }
        c[i * (2 + sep_size)] = byte_to_char(b[i] >> 4);
        c[i * (2 + sep_size) + 1] = byte_to_char(b[i] & 15);
    }
    return String(c);
}

/**
 * General library function
 * Converts 4 bits to a char (0-9,A-F).
 */
char byte_to_char(byte b)
{
    char c = b & 15;
    if (c > 9)
    {
        return c + 55; // A-F
    }
    return c += 48; // 0-9
}
