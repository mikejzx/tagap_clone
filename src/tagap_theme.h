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

enum tagap_env_style
{
    ENVIRON_NONE = 0,
    ENVIRON_RAIN,       // Heavy outdoor rain
    ENVIRON_RAININT,    // Interior with rain outside (through windows, etc.)
    ENVIRON_SNOWING,    // Snowing
    ENVIRON_UNDERWATER, // Fully 'flooded'

    ENVIRON_COUNT
};

static const char *ENVIRON_NAMES[] =
{
    [ENVIRON_NONE]        = "",
    [ENVIRON_RAIN]        = "RAIN",
    [ENVIRON_RAININT]     = "RAININT",
    [ENVIRON_SNOWING]     = "SNOW",
    [ENVIRON_UNDERWATER]  = "UNDERWATER",
};

CREATE_LOOKUP_FUNC(lookup_tagap_env, ENVIRON_NAMES, ENVIRON_COUNT);

struct tagap_theme_info
{
    char name[THEME_NAME_MAX];

    vec3s colours[THEME_AFFECT_COUNT][THEME_STATE_COUNT];

    enum tagap_env_style env;

    i32 darkness[THEME_STATE_COUNT];
};

static inline f32
theme_get_darkness_value(i32 darkness)
{
    darkness = clamp(darkness, 0, 3);

    static const f32 DARKNESS_VALUES[4] =
    {
        1.0f,
        0.50f,
        0.25f,
        0.00f,
    };

    return DARKNESS_VALUES[darkness];
}

#endif
