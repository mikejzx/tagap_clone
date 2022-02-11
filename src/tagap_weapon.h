#ifndef TAGAP_WEAPON_H
#define TAGAP_WEAPON_H

#include "tagap_entity.h"

#define WEAPON_SLOT_COUNT 32
#define WEAPON_DISPLAY_NAME_MAX 32

struct tagap_weapon
{
    char display_name[WEAPON_DISPLAY_NAME_MAX];
    struct tagap_entity_info *primary;
    struct tagap_entity_info *secondary;
};

#endif
