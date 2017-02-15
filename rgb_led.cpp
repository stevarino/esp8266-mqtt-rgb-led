#include "rgb_led.h"

float RGB_LED::transition = 3000.0;
float RGB_LED::ramp_coefficient = 2.0;
String RGB_LED::on_cmd = "ON";
String RGB_LED::off_cmd = "OFF";
int RGB_LED::millis_step = 20;


bool RGB_LED::root_is_on = 0;
float RGB_LED::root_brightness = 0;
float RGB_LED::root_red = 0;
float RGB_LED::root_green = 0;
float RGB_LED::root_blue = 0;

/**
 * Default Constructor
 * Sets everything to 0.
 */
RGB_LED::RGB_LED()
{
    for (int j = 0; j < 3; j++)
    {
        is_on = false;
        brightness[j] = 255;
        current[j] = 0;
        target[j] = 0;
        step[j] = 0;
        tick[j] = 0;
        scale[j] = 1;
    }
}

/**
 * Initializes the object.
 */
void RGB_LED::init(String base, int i, int r, int g, int b)
{
    index = i;
    pins[0] = r;
    pins[1] = g;
    pins[2] = b;
    topic_state = base + String(i);

    pinMode(r, OUTPUT);
    pinMode(g, OUTPUT);
    pinMode(b, OUTPUT);
}

/**
 * Return the LED MQTT topic.
 */
String RGB_LED::get_topic()
{
    return this->topic_state;
}

/**
 * Iterate the LED by one cycle.
 */
void RGB_LED::loop()
{
    for (int i = 0; i < 3; i++)
    {
        if (this->step[i] != 0)
        {
            if ((this->target[i] > this->current[i] && this->step[i] < 0) ||
                (this->target[i] < this->current[i] && this->step[i] > 0))
            {
                this->step[i] = -1 * this->step[i];
            }
            if (abs(this->current[i] - this->target[i]) <= abs(this->step[i]))
            {
                this->step[i] = 0;
                this->current[i] = this->target[i];
                this->tick[i] = 0;
            }
            else 
            {
                this->current[i] = this->current[i] + this->step[i];
                this->tick[i] += 1;
            }
        }
        analogWrite(this->pins[i], 255 * pow(this->current[i] / 255.0, RGB_LED::ramp_coefficient));
    }
}

/**
 * Returns the apparent brightness.
 */
float RGB_LED::get_brightness()
{
    return _max(this->brightness[0] * this->scale[0], _max(
            this->brightness[1] * this->scale[1], 
            this->brightness[2] * this->scale[2]));
}

/**
 * Returns the red component of the LED.
 */
float RGB_LED::get_red()
{
    return this->brightness[0] * this->scale[0];
}

/**
 * Returns the green component of the LED.
 */
float RGB_LED::get_green()
{
    return this->brightness[1] * this->scale[1];
}

/**
 * Returns the blue component of the LED.
 */
float RGB_LED::get_blue()
{
    return this->brightness[2] * this->scale[2];
}

/**
 * Returns if the LED is on or not.
 */
bool RGB_LED::get_is_on()
{
    return this->is_on;
}

/**
 * Sets whether the LED is on or not.
 */
void RGB_LED::set_is_on(bool on)
{
    this->is_on = on;
}

/**
 * Sets the apparent brightness and adjusts the colors appropriately.
 */
void RGB_LED::set_brightness(int b)
{
    float current = this->get_brightness();
    float s = 1.0;
    if (current != 0) 
    {
        s = b / current;
    }
    
    for (int i = 0; i < 3; i++)
    {
        this->brightness[i] = constrain(s * this->brightness[i], 0, 255);
    }
}

/**
 * Sets the LED color by primary components.
 */
void RGB_LED::set_color(int r, int g, int b)
{
    this->brightness[0] = r;
    this->brightness[1] = g;
    this->brightness[2] = b;
}

/**
 * Sets the LED topic.
 */
void RGB_LED::set_topic(String topic) 
{
    this->topic_state = topic;
}

/**
 * Process the JSON passed.
 */
void RGB_LED::process_json(JsonObject& json, float sc_r, float sc_g, float sc_b)
{
    if (json.containsKey("state"))
    {
        this->is_on = RGB_LED::on_cmd.equals(json["state"].asString());
    }

    if (json.containsKey("color"))
    {
        this->set_color(json["color"]["r"], 
                        json["color"]["g"], 
                        json["color"]["b"]);
    }
    if (json.containsKey("brightness"))
    {
        this->set_brightness((int)json["brightness"]);
    }
    
    this->scale[0] = sc_r;
    this->scale[1] = sc_g;
    this->scale[2] = sc_b;

    Serial.printf("Using scale (%d, %d, %d)\n", (int)(this->scale[0]*1000), 
            (int)(this->scale[1]*1000), (int)(this->scale[2]*1000));

    for (int k = 0; k < 3; k++)
    {
        this->target[k] = this->brightness[k] * this->is_on * this->scale[k];

        float trans = json.containsKey("transition") 
                            ? ((int)json["transition"]) * 1000.0
                            : RGB_LED::transition;

        if (trans > 0)
        {
            this->step[k] = (this->target[k] - this->current[k]) / 
                    (trans / millis_step);
        }
        else
        {
            this->step[k] = 255;
        }

    }
    Serial.printf("LED: %d; Is On: %d; Target: (%d, %d, %d); Current: (%d, %d, %d); Step (k): (%d, %d, %d)\n",
                    this->index, this->is_on, 
                    (int)this->target[0], (int)this->target[1], (int)this->target[2], 
                    (int)this->current[0], (int)this->current[1], (int)this->current[2], 
                    (int)(1000*this->step[0]), (int)(1000*this->step[1]), (int)(1000*this->step[2]));
}

/**
 * Sets the default transition time in ms.
 */
void RGB_LED::set_default_transition(float t)
{
    RGB_LED::transition = t;
    Serial.printf("Default Transition set to %d\n", (int)RGB_LED::transition);
}

/**
 * Sets the default step time in ms.
 */
void RGB_LED::set_millis_step(int s)
{
    RGB_LED::millis_step = s;
    Serial.printf("LED step rate set to %d\n", RGB_LED::millis_step);
}

/**
 * Sets the default on command (ON)
 */
void RGB_LED::set_on_cmd(String s)
{
    RGB_LED::on_cmd = s;
}

/**
 * Sets the default off command (OFF)
 */
void RGB_LED::set_off_cmd(String s)
{
    RGB_LED::off_cmd = s;
}

/**
 * Sets the non-linear ramp ramp_coefficient for LEDs.
 */
void RGB_LED::set_ramp_coefficient(float ramp)
{
    ramp_coefficient = ramp;
}

/**
 * Recalculates all the base settings.
 */
void RGB_LED::update_root(RGB_LED rgb_leds[], int led_count) 
{
    RGB_LED::root_is_on = false;
    RGB_LED::root_brightness = 0;

    RGB_LED::root_red = 0;
    RGB_LED::root_green = 0;
    RGB_LED::root_blue = 0;

    for (int i = 0; i < led_count; i++)
    {
        RGB_LED::root_is_on = RGB_LED::root_is_on || rgb_leds[i].get_is_on();
        
        RGB_LED::root_brightness = (_max(RGB_LED::root_brightness,
                rgb_leds[i].get_is_on() * rgb_leds[i].get_brightness()));
        RGB_LED::root_red = _max(RGB_LED::root_red, 
                rgb_leds[i].get_is_on() * rgb_leds[i].get_red());
        RGB_LED::root_green = _max(RGB_LED::root_green,
                rgb_leds[i].get_is_on() * rgb_leds[i].get_green());
        RGB_LED::root_blue = _max(RGB_LED::root_blue, 
                rgb_leds[i].get_is_on() * rgb_leds[i].get_blue());
    }
}

/**
 * True if any LED is on.
 */ 
bool RGB_LED::get_root_is_on()
{
    return RGB_LED::root_is_on;
}

/**
 * Returns the maximum LED brightness of any component on any strand.
 */
float RGB_LED::get_root_brightness()
{
    return RGB_LED::root_brightness;
}

/**
 * Returns the maximum Red component from any strand.
 */
float RGB_LED::get_root_red()
{
    return RGB_LED::root_red;
}

/**
 * Returns the maximum Green component from any strand.
 */
float RGB_LED::get_root_green()
{
    return RGB_LED::root_green;
}

/**
 * Returns the maximum Blue component from any strand.
 */
float RGB_LED::get_root_blue()
{
    return RGB_LED::root_blue;
}