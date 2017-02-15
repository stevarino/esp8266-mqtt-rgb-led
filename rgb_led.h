#include <ArduinoJson.h>
#include <Arduino.h>

#ifndef RGB_LED_H
#define RGB_LED_H

class RGB_LED {
    public:
        RGB_LED();
        void init (String, int, int, int, int);
        void loop();
        
        String get_topic();
        bool get_is_on();
        float get_brightness();
        float get_red();
        float get_green();
        float get_blue();
        
        void set_is_on(bool);
        void set_brightness(int);
        void set_color(int, int, int);
        void set_topic(String);

        void process_json(JsonObject&, float, float, float);

        static bool get_root_is_on();
        static float get_root_brightness();
        static float get_root_red();
        static float get_root_green();
        static float get_root_blue();
        static void update_root(RGB_LED[], int);

        static void set_default_transition(float);
        static void set_ramp_coefficient(float);
        static void set_millis_step(int);
        static void set_on_cmd(String);
        static void set_off_cmd(String);

    private:
        uint8_t index;            // the index of this set of leds (0, 1...)
        bool is_on;               // true if on
        uint8_t brightness[3];    // 0-255 brightness as ordered
        uint8_t pins[3];          // pin numbers

        uint8_t brightness_calc;  // average of brightness
        int color_calc;           // normalized color

        float target[3];          // physical command (is_on * brightness)
        float step[3];            // how much to adjust brightness by per loop
        float current[3];         // current state
        int tick[3];              // count of steps for change (debug)
        float scale[3];           // scale of the color - applied by aliases

        String topic_state;       // mqtt topic for LED state
        
        static float transition;
        static float ramp_coefficient;
        static String on_cmd;
        static String off_cmd;
        static int millis_step;

        static bool root_is_on;
        static float root_brightness;
        static float root_red;
        static float root_green;
        static float root_blue;
};

typedef struct RGB_LED_ALIAS {
    String topic_state;           // mqtt topic
    int index_start;              // start index of the targeted LED.
    int index_end;                // start index of the targeted LED, -1 to finish at the end.
    float scale[3];               // 0-1 scaling applied 
} RGB_LED_ALIAS;

#endif