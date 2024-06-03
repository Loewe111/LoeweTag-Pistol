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
    this->pState = state;
}

void animations::_anim_connecting() {
    unsigned long m = millis() / 2 * -1; 
    
    for (int i = 0; i < leds_gun->numPixels(); i++) {
        leds_gun->setPixelColor(i, intSin(m + i * 90), 0, 0);
    }
    leds_sensors->fill(0);

    leds_gun->show();
    leds_sensors->show();
}

void animations::_anim_idle() {
    unsigned long m = millis() / 10; 
    
    for (int i = 0; i < leds_gun->numPixels(); i++) {
        float sinVal = floatSin(m);
        leds_gun->setPixelColor(
            i, 
            sinVal * pState->color.r, 
            sinVal * pState->color.g,
            sinVal * pState->color.b
        );
    }
    for (int i = 0; i < leds_sensors->numPixels(); i++) {
        float sinVal = floatSin(m);
        leds_sensors->setPixelColor(
            i, 
            sinVal * pState->color.r, 
            sinVal * pState->color.g,
            sinVal * pState->color.b
        );
    }

    leds_gun->show();
    leds_sensors->show();
}

void animations::_anim_playing() {
    unsigned long m = millis() / 10; 
    
    // calculate amount of health to display
    int pixels = pState->health * leds_gun->numPixels() / pState->max_health;
    for (int i = 0; i < leds_gun->numPixels(); i++) {
        float sinVal = floatSin(m + i * 5) / 2 + 0.5;
        if (i < pixels) {
            leds_gun->setPixelColor(i, sinVal * pState->color.r, sinVal * pState->color.g, sinVal * pState->color.b);
        } else {
            leds_gun->setPixelColor(i, 0, 0, 0);
        }
    }
    for (int i = 0; i < leds_sensors->numPixels(); i++) {
        float sinVal = floatSin(m + i * 180);
        leds_sensors->setPixelColor(
            i, 
            sinVal * pState->color.r, 
            sinVal * pState->color.g,
            sinVal * pState->color.b
        );
    }

    leds_gun->show();
    leds_sensors->show();
}

void animations::_anim_dead() {
    unsigned long m = millis() / 10; 
    
    for (int i = 0; i < leds_gun->numPixels(); i++) {
        float sinVal = floatSin(i * 180 + m) * 0.2;
        leds_gun->setPixelColor(
            i, 
            sinVal * pState->color.r, 
            sinVal * pState->color.g,
            sinVal * pState->color.b
        );
    }
    leds_sensors->fill(0);

    leds_gun->show();
    leds_sensors->show();
}

void animations::_anim_shooting() {
    unsigned long m = millis() / 10; 
    unsigned long d = (millis() - startMillis);
    
    for (int i = 0; i < leds_gun->numPixels(); i++) {
        float sinVal = floatSin(d * -1 + i * 12);
        leds_gun->setPixelColor(
            i, 
            sinVal * pState->color.r, 
            sinVal * pState->color.g,
            sinVal * pState->color.b
        );
    }
    
    for (int i = 0; i < leds_sensors->numPixels(); i++) {
        float sinVal = floatSin(m + i * 180);
        leds_sensors->setPixelColor(
            i, 
            sinVal * pState->color.r, 
            sinVal * pState->color.g,
            sinVal * pState->color.b
        );
    }

    leds_gun->show();
    leds_sensors->show();

    if (d >= 180) {
        nextAnimation();
    }
}

void animations::_anim_hit() {
    unsigned long d = (millis() - startMillis);
    
    int pixels = pState->health * leds_gun->numPixels() / pState->max_health;
    for (int i = 0; i < leds_gun->numPixels(); i++) {
        if (i < pixels) {
            leds_gun->setPixelColor(i, 0.1 * pState->color.r, 0.1 * pState->color.g, 0.1 * pState->color.b);
        } else {
            leds_gun->setPixelColor(i, 0, 0, 0);
        }
    }
    
    for (int i = 0; i < leds_sensors->numPixels(); i++) {
        float sinVal = 1 - (float)d / 200;
        leds_sensors->setPixelColor(
            i, 
            sinVal * pState->color.r, 
            sinVal * pState->color.g,
            sinVal * pState->color.b
        );
    }

    leds_gun->show();
    leds_sensors->show();

    if (d >= 200) {
        nextAnimation();
    }
}

void animations::draw() {
  switch (state) {
    case ANIM_CONNECTING:
        _anim_connecting();
        break;
    case ANIM_IDLE:
        _anim_idle();
        break;
    case ANIM_PLAYING:
        _anim_playing();
        break;
    case ANIM_DEAD:
        _anim_dead();
        break;
    case ANIM_SHOOTING:
        _anim_shooting();
        break;
    case ANIM_HIT:
        _anim_hit();
        break;
  }
}

void animations::setAnimation(AnimationState state, AnimationState nextState) {
    this->state = state;
    this->nextState = nextState;
    startMillis = millis();
}

void animations::setAnimation(AnimationState state) {
    startMillis = millis();
    if (this->state == state) return;
    this->nextState = this->state;
    this->state = state;
}

void animations::nextAnimation() {
    this->state = nextState;
    startMillis = millis();
}