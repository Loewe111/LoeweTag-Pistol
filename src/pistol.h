#include <Arduino.h>

#ifndef PISTOL_H
#define PISTOL_H

#pragma pack(push, 1)

enum gamestate_t : int8_t {
    GAMESTATE_OFFLINE = -1,
    GAMESTATE_IDLE = 0,
    GAMESTATE_PLAYING = 1
};

struct pistol_color_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct pistol_weapon_t {
    enum : uint8_t {automatic, manual} reload_type;
    uint16_t reload_time;
    uint16_t power;
    bool active;
    enum : uint8_t {narrow, wide} beam_type;
};

struct pistol_state_t {
    gamestate_t gamestate;
    pistol_color_t color;
    pistol_weapon_t weapon;
    uint16_t health;
    uint16_t max_health;
    uint16_t cooldown;
};

// Messages

enum device_type_t : uint8_t {
    DEVICE_TYPE_MASTER = 0,
    DEVICE_TYPE_PISTOL = 1,
};

enum message_type_t : uint8_t {
    MESSAGE_PING = 0x0,
    MESSAGE_DEVICE_INFORMATION = 0x1,
    MESSAGE_SET_GAMESTATE = 0x2,
    MESSAGE_LOCATE = 0x4,
    MESSAGE_SET_COLOR = 0x10,
    MESSAGE_SET_WEAPON = 0x11,
    MESSAGE_SET_HEALTH = 0x12,
    MESSAGE_HIT = 0x20,
    MESSAGE_REQUEST_INFO = 0x22,
};

struct message_base_t {
    uint16_t sender;
    uint16_t target;
    message_type_t type;
    uint8_t length;
    uint8_t data[0];
};

struct message_device_information_t {
    uint16_t device_id;
    device_type_t device_type;
};

struct message_hit_t {
    uint16_t shooter_id;
};

struct message_ping_t {
    uint8_t is_master = 0;
};

struct message_set_gamestate_t {
    gamestate_t gamestate;
};

struct message_set_color_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct message_set_weapon_t {
    pistol_weapon_t weapon;
};

struct message_set_health_t {
    uint8_t health;
    uint8_t max_health;
};

#pragma pack(pop)

#endif