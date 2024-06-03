#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "pistol.h"

enum AnimationState {ANIM_CONNECTING, ANIM_IDLE, ANIM_PLAYING, ANIM_DEAD, ANIM_SHOOTING, ANIM_HIT};

class animations
{
  private:
    AnimationState state = ANIM_CONNECTING;
    AnimationState nextState = ANIM_IDLE;
    pistol_state_t *pState;
    bool repeat = false;
    Adafruit_NeoPixel *leds_gun;
    Adafruit_NeoPixel *leds_sensors;
    // Animation functions
    void _anim_connecting();
    void _anim_idle();
    void _anim_playing();
    void _anim_dead();
    void _anim_shooting();
    void _anim_hit();
    unsigned long startMillis = 0;
  public:
    animations(Adafruit_NeoPixel *leds_gun, Adafruit_NeoPixel *leds_sensors, pistol_state_t *state);
    ~animations() = default;
    void draw();
    void setAnimation(AnimationState state, AnimationState nextState);
    void setAnimation(AnimationState state);
    void nextAnimation();
};

#endif