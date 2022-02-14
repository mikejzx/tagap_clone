#ifndef TAGAP_ENTITY_MOVETYPE_H
#define TAGAP_ENTITY_MOVETYPE_H

enum tagap_entity_movetype_id
{
    MOVETYPE_NONE = 0, // Static entity
    MOVETYPE_FLY,      // Flying/floating movement
    MOVETYPE_WALK,     // Affected by gravity

    MOVETYPE_COUNT,
};

static const char *MOVETYPE_NAMES[] =
{
    [MOVETYPE_NONE] = "NONE",
    [MOVETYPE_FLY]  = "FLY",
    [MOVETYPE_WALK] = "WALK",
};

CREATE_LOOKUP_FUNC(lookup_tagap_movetype, MOVETYPE_NAMES, MOVETYPE_COUNT);

struct tagap_entity_movetype
{
    enum tagap_entity_movetype_id type;
    f32 speed;
};

void entity_movetype(struct tagap_entity *);

#endif
