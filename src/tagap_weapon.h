#ifndef TAGAP_WEAPON_H
#define TAGAP_WEAPON_H

struct tagap_entity_info;

#define WEAPON_SLOT_COUNT 32
#define WEAPON_DISPLAY_NAME_MAX 32

struct tagap_weapon
{
    char display_name[WEAPON_DISPLAY_NAME_MAX];
    struct tagap_entity_info *primary;
    struct tagap_entity_info *secondary;

    f32 reload_time;   // Set to 0 for no reloading
    i32 magazine_size;
};

#endif
