#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "pistol.h"

enum AnimationState {ANIM_NONE, ANIM_SHOOT, ANIM_HIT, ANIM_LOCATE};

class animations
{
  private:
    AnimationState state = ANIM_NONE;
    pistol_state_t *pistol_state;
    bool repeat = false;
    Adafruit_NeoPixel *leds_gun;
    Adafruit_NeoPixel *leds_sensors;
    
    uint64_t runTime = 0;
    uint64_t startTime = 0;
    // Animation functions
    void _anim_connecting();
    void _anim_idle();
    void _anim_playing();
    void _anim_dead();
    void (animations::*currentAnimation)();
    void (animations::*lastAnimation)();
    bool _anim_shoot();
    bool _anim_hit();
    bool _anim_locate();
    bool (animations::*currentOverlay)();
  public:
    animations(Adafruit_NeoPixel *leds_gun, Adafruit_NeoPixel *leds_sensors, pistol_state_t *state);
    ~animations() = default;
    void draw();
    void play(AnimationState state);
};

#endif