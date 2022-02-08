#ifndef ENTITY_H
#define ENTITY_H

#include "types.h"

struct entity
{
    // Entity position
    union 
    {
        vec3s pos;
        struct { f32 x, y, z; };
    };
};

#endif
