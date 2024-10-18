#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "animations.h"
#include "pistol.h"

uint8_t intSin(uint16_t x) { // input 0-360, output 0-255
    return (sin(x * PI / 180) + 1) * 127;
}

float floatSin(uint16_t x) {
    return (sin(x * PI / 180) + 1) / 2;
}

// constructor
animations::animations(Adafruit_NeoPixel *leds_gun, Adafruit_NeoPixel *leds_sensors, pistol_state_t *state) {
    this->leds_gun = leds_gun;
    this->leds_sensors = leds_sensors;
    this->pistol_state = state;
}

void animations::_anim_connecting() {
    unsigned long m = runTime / 2 * -1; 
    
    for (int i = 0; i < leds_gun->numPixels(); i++) {
        leds_gun->setPixelColor(i, intSin(m + i * 90), 0, 0);
    }
    leds_sensors->fill(0);

    leds_gun->show();
    leds_sensors->show();
}

void animations::_anim_idle() {
    unsigned long m = runTime / 10; 
    
    for (int i = 0; i < leds_gun->numPixels(); i++) {
        float sinVal = floatSin(m);
        leds_gun->setPixelColor(
            i, 
            sinVal * pistol_state->color.r, 
            sinVal * pistol_state->color.g,
            sinVal * pistol_state->color.b
        );
    }
    for (int i = 0; i < leds_sensors->numPixels(); i++) {
        float sinVal = floatSin(m);
        leds_sensors->setPixelColor(
            i, 
            sinVal * pistol_state->color.r, 
            sinVal * pistol_state->color.g,
            sinVal * pistol_state->color.b
        );
    }

    leds_gun->show();
    leds_sensors->show();
}

void animations::_anim_playing() {
    unsigned long m = runTime / 10; 
    
    // calculate amount of health to display
    int pixels = pistol_state->health * leds_gun->numPixels() / pistol_state->max_health;
    for (int i = 0; i < leds_gun->numPixels(); i++) {
        float sinVal = floatSin(m + i * 5) / 2 + 0.5;
        if (i < pixels) {
            leds_gun->setPixelColor(i, sinVal * pistol_state->color.r, sinVal * pistol_state->color.g, sinVal * pistol_state->color.b);
        } else {
            leds_gun->setPixelColor(i, 0, 0, 0);
        }
    }
    for (int i = 0; i < leds_sensors->numPixels(); i++) {
        float sinVal = floatSin(m + i * 180);
        leds_sensors->setPixelColor(
            i, 
            sinVal * pistol_state->color.r, 
            sinVal * pistol_state->color.g,
            sinVal * pistol_state->color.b
        );
    }

    leds_gun->show();
    leds_sensors->show();
}

void animations::_anim_dead() {
    unsigned long m = runTime / 10; 
    
    for (int i = 0; i < leds_gun->numPixels(); i++) {
        float sinVal = floatSin(i * 180 + m) * 0.2;
        leds_gun->setPixelColor(
            i, 
            sinVal * pistol_state->color.r, 
            sinVal * pistol_state->color.g,
            sinVal * pistol_state->color.b
        );
    }
    leds_sensors->fill(0);

    leds_gun->show();
    leds_sensors->show();
}

bool animations::_anim_shoot() {
    unsigned long m = runTime / 2;
    
    for (int i = 0; i < leds_gun->numPixels(); i++) {
        float sinVal = floatSin(runTime * -1 + i * 12);
        leds_gun->setPixelColor(
            i, 
            sinVal * pistol_state->color.r, 
            sinVal * pistol_state->color.g,
            sinVal * pistol_state->color.b
        );
    }
    
    for (int i = 0; i < leds_sensors->numPixels(); i++) {
        float sinVal = floatSin(m + i * 180);
        leds_sensors->setPixelColor(
            i, 
            sinVal * pistol_state->color.r, 
            sinVal * pistol_state->color.g,
            sinVal * pistol_state->color.b
        );
    }

    leds_gun->show();
    leds_sensors->show();

    return runTime >= 180;
}

bool animations::_anim_hit() {
    
    int pixels = pistol_state->health * leds_gun->numPixels() / pistol_state->max_health;
    for (int i = 0; i < leds_gun->numPixels(); i++) {
        if (i < pixels) {
            leds_gun->setPixelColor(i, 0.1 * pistol_state->color.r, 0.1 * pistol_state->color.g, 0.1 * pistol_state->color.b);
        } else {
            leds_gun->setPixelColor(i, 0, 0, 0);
        }
    }
    
    for (int i = 0; i < leds_sensors->numPixels(); i++) {
        float sinVal = 1 - (float)runTime / 200;
        leds_sensors->setPixelColor(
            i, 
            sinVal * pistol_state->color.r, 
            sinVal * pistol_state->color.g,
            sinVal * pistol_state->color.b
        );
    }

    leds_gun->show();
    leds_sensors->show();

    return runTime >= 200;
}

void animations::draw() {
    // Overlay animations
    if (currentOverlay != nullptr) {
        bool finished = (this->*currentOverlay)();
        if (finished) currentOverlay = nullptr;
    }

    // Main animations
    switch (this->pistol_state->gamestate) {
        case GAMESTATE_OFFLINE:
            currentAnimation = &animations::_anim_connecting;
            break;
        case GAMESTATE_IDLE:
            currentAnimation = &animations::_anim_idle;
            break;
        case GAMESTATE_PLAYING:
            if (pistol_state->health > 0) {
                currentAnimation = &animations::_anim_playing;
            } else {
                currentAnimation = &animations::_anim_dead;
            }
            break;
    }

    if (lastAnimation != currentAnimation) {
        startTime = millis();
        lastAnimation = currentAnimation;
    }
    runTime = millis() - startTime;

    if (currentAnimation != nullptr && currentOverlay == nullptr) {
        (this->*currentAnimation)();
    }
}

void animations::play(AnimationState state) {
    switch (state) {
        case ANIM_SHOOT:
            currentOverlay = &animations::_anim_shoot;
            break;
        case ANIM_HIT:
            currentOverlay = &animations::_anim_hit;
            break;
        default:
            currentOverlay = nullptr;
            break;
    }
    startTime = millis();
    runTime = 0;
    draw();
}