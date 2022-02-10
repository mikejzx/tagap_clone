#ifndef TAGAP_THEME_H
#define TAGAP_THEME_H

#define THEME_NAME_MAX 16

enum tagap_theme_affect_id
{
    THEME_AFFECT_WORLD = 0,
    THEME_AFFECT_BACKGROUND,
    THEME_AFFECT_COUNT
};

enum tagap_theme_state
{
    THEME_STATE_BASE = 0,
    THEME_STATE_SHIFT,
    THEME_STATE_COUNT,
};

struct tagap_theme_info
{
    char name[THEME_NAME_MAX];

    vec3s colours[THEME_AFFECT_COUNT][THEME_STATE_COUNT];

    // TODO: other variables, rain, 'darkness', etc.
};

#endif
