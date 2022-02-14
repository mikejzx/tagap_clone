#ifndef TAGAP_TRIGGER_H
#define TAGAP_TRIGGER_H

enum tagap_trigger_id
{
    TRIGGER_UNKNOWN = 0,

    //TRIGGER_GUIDE, // Lamps, monitors, guide arrows, etc.
    TRIGGER_LAYER, // Renders layer as part of the world

    TRIGGER_COUNT
};

static const char *TRIGGER_NAMES[TRIGGER_COUNT] =
{
    [TRIGGER_UNKNOWN] = "",
    [TRIGGER_LAYER] = "layer",
};

CREATE_LOOKUP_FUNC(lookup_tagap_trigger, TRIGGER_NAMES, TRIGGER_COUNT);

struct tagap_trigger
{
    // Top-left/bottom-right corners of region
    vec2s corner_tl, corner_br;

    // Target index reference
    i32 target_index;

    // Trigger type
    enum tagap_trigger_id id;

    // TODO: link class, additional variable value, etc.
};

//void tagap_trigger_init(struct tagap_trigger *);

#endif
