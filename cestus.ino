//
// Cestus
//
// Copyright (c) 2016-2017 Tapani J. Otala
//
// See README.md for details on the source code.
// See LICENSE for details on license.
//
#include <Adafruit_LSM9DS0.h>
#include <Adafruit_Simple_AHRS.h>
#include <Adafruit_NeoPixel.h>
#include <NeoPatterns.h>

// common colors
#define COLOR(r, g, b)              Adafruit_NeoPixel::Color(r, g, b)
#define COLOR_BLACK                 COLOR(0, 0, 0)
#define COLOR_WHITE                 COLOR(255, 255, 255)
#define COLOR_RED                   COLOR(255, 0, 0)
#define COLOR_GREEN                 COLOR(0, 255, 0)
#define COLOR_BLUE                  COLOR(0, 0, 255)

// brightness setting 0..255 from percentage 0..100%
#define BRIGHTNESS(b)               uint8_t((b) != 0 ? 255 * 100 / (b) : 0) 

// configuration settings
#define CONF_USE_DEBUG_OUTPUT       false         // true: enable, false: disable
#define CONF_DEBUG_OUTPUT_SPEED     115200        // bps

#define CONF_BLINK_ONBOARD_LED      50            // ms; set to 0 to not blink at all

#define CONF_TURN_TOLERANCE         30            // ± degrees; tolerance from 90deg roll (for turn signal)
#define CONF_TURN_INTERVAL          10            // ms
#define CONF_TURN_COLOR             COLOR_RED
#define CONF_TURN_BRIGHTNESS        BRIGHTNESS(50)
#define CONF_TURN_SPLIT             true          // true: split the pattern into two segments, false: unidirectional pattern

#define CONF_STOP_TOLERANCE         30            // ± degrees; tolerance from 90deg pitch (for stop signal)
#define CONF_STOP_INTERVAL          50            // ms
#define CONF_STOP_COLOR1            COLOR_RED
#define CONF_STOP_COLOR2            COLOR_BLACK
#define CONF_STOP_BRIGHTNESS        BRIGHTNESS(100)

#define CONF_GO_TOLERANCE           30            // ± degrees; tolerance from 90deg pitch (for ride signal)
#define CONF_GO_INTERVAL            100           // ms
#define CONF_GO_COLOR1              COLOR_WHITE
#define CONF_GO_COLOR2              COLOR_BLACK
#define CONF_GO_BRIGHTNESS          BRIGHTNESS(100)

// hardware constants for Adafruit Flora on-board resources
#define CONF_ONBOARD_LED_PIN        7             // hardwired
#define CONF_ONBOARD_NEOPIXEL_PIN   8             // hardwired

// hardware constants for the NeoPixel ring
#define CONF_NEOPIXEL_PIN           6             // connected to pin D6
#define CONF_NEOPIXEL_LEDS          16            // number of LEDs

#if CONF_USE_DEBUG_OUTPUT
#define DEBUG_OUT(msg)              Serial.print(msg)
#define DEBUG_OUTLN(msg)            Serial.println(msg)
#else
#define DEBUG_OUT(msg)              /* nothing */
#define DEBUG_OUTLN(msg)            /* nothing */
#endif

//
// Derived Classes
//
class TurnPattern : public Scanner
{
public:
    TurnPattern(NeoPatterns& pixels) : Scanner(pixels, CONF_TURN_INTERVAL, CONF_TURN_COLOR, CONF_TURN_SPLIT) { }
};

class StopPattern : public TheaterChase
{
public:
    StopPattern(NeoPatterns& pixels) : TheaterChase(pixels, CONF_STOP_INTERVAL, CONF_STOP_COLOR1, CONF_STOP_COLOR2) { }
};

class GoPattern : public Pulsar
{
public:
    GoPattern(NeoPatterns& pixels) : Pulsar(pixels, CONF_GO_INTERVAL, CONF_GO_COLOR1, CONF_GO_COLOR2) { }
};

//
// Global Variables
//
NeoPatterns ring(CONF_NEOPIXEL_LEDS, CONF_NEOPIXEL_PIN);
Adafruit_LSM9DS0 sensor;
Adafruit_Simple_AHRS ahrs(&sensor.getAccel(), &sensor.getMag());

//
// Setup function
//
// This executes exactly once, at startup
//
void setup()
{
#if CONF_USE_DEBUG_OUTPUT
    Serial.begin(CONF_DEBUG_OUTPUT_SPEED);
    DEBUG_OUTLN(F("Cestus v1.0"));
#endif

#if CONF_BLINK_ONBOARD_LED
    // initialize the on-board LED
    pinMode(CONF_ONBOARD_LED_PIN, OUTPUT);
#endif

    // initialize the sensor(s)
    // we really only need acceleration
    // however, the AHRS library expects magnetometer as well
    // we do not need the gyroscope or temperature
    sensor.setupAccel(sensor.LSM9DS0_ACCELRANGE_2G);
    sensor.setupMag(sensor.LSM9DS0_MAGGAIN_2GAUSS);
    if (!sensor.begin())
    {
#if CONF_USE_DEBUG_OUTPUT
        while (true)
        {
            DEBUG_OUTLN(F("Oops ... unable to initialize the LSM9DS0. Check your I2C wiring!"));
        }
#endif
    }

    // initialize the NeoPixel Ring
    ring.begin();
}

template<typename T> bool InRange(T val, T min, T max)
{
    return min <= val && val <= max;
}

//
// Loop
//
// This code runs continuously until powered off
//
void loop()
{
#if CONF_BLINK_ONBOARD_LED
    //
    // Blink the on-board LED
    //
    digitalWrite(CONF_ONBOARD_LED_PIN, HIGH);
    delay(CONF_BLINK_ONBOARD_LED);
    digitalWrite(CONF_ONBOARD_LED_PIN, LOW);
#endif
  
    //
    // Sample the current orientation
    //
    sensors_vec_t orientation;
    ahrs.getOrientation(&orientation);

    DEBUG_OUT(F("roll: ")); DEBUG_OUT(int(orientation.roll));
    DEBUG_OUT(F(", pitch: ")); DEBUG_OUT(int(orientation.pitch));
    DEBUG_OUT(F(", heading: ")); DEBUG_OUT(int(orientation.heading));
    DEBUG_OUTLN(F(""));

    //
    // Animate a stop signal if we're oriented at 90deg pitch angle, with ±CONF_STOP_TOLERANCE
    //
    if (InRange(int(orientation.pitch), 90 - CONF_STOP_TOLERANCE, 90 + CONF_STOP_TOLERANCE))
    {
        static StopPattern stop(ring);
        if (!ring.IsActive(&stop))
        {
            DEBUG_OUTLN(F("starting stop animation"));
            ring.setBrightness(CONF_STOP_BRIGHTNESS);
            ring.Start(&stop);
        }
    }
    //
    // Animate a go signal if we're oriented at -90deg pitch angle, with ±CONF_GO_TOLERANCE
    //
    else if (InRange(int(orientation.pitch), -90 - CONF_GO_TOLERANCE, -90 + CONF_GO_TOLERANCE))
    {
        static GoPattern go(ring);
        if (!ring.IsActive(&go))
        {
            DEBUG_OUTLN(F("starting go animation"));
            ring.setBrightness(CONF_GO_BRIGHTNESS);
            ring.Start(&go);
        }
    }
    //
    // Animate a turning signal if we're oriented at 90deg or -90deg roll angles, with ±CONF_TURN_TOLERANCE
    //
    else if (InRange(int(orientation.roll), 90 - CONF_TURN_TOLERANCE, 90 + CONF_TURN_TOLERANCE) ||
             InRange(int(orientation.roll), -90 - CONF_TURN_TOLERANCE, -90 + CONF_TURN_TOLERANCE))
    {
        static TurnPattern turn(ring);
        if (!ring.IsActive(&turn))
        {
            DEBUG_OUTLN(F("starting turn animation"));
            ring.setBrightness(CONF_TURN_BRIGHTNESS);
            ring.Start(&turn);
        }
    }
    else
    {
        ring.Stop();
    }
    ring.Update();
}

