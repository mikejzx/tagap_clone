#ifndef TAGAP_H
#define TAGAP_H

#include "state_level.h"
#include "state_menu.h"
#include "vulkan_renderer.h"

// TAGAP data directory.  May make this adjustable
//#define TAGAP_DATA_DIR "/home/mike/games/TAGAP/data"
#define TAGAP_DATA_DIR "./data"
#define TAGAP_DATA_MOD_DIR "./data_add"
#define TAGAP_ART_DIR TAGAP_DATA_DIR "/art"
#define TAGAP_SCRIPT_DIR TAGAP_DATA_DIR "/script"
#define TAGAP_TEXTURES_DIR TAGAP_ART_DIR "/textures"
#define TAGAP_SPRITES_DIR TAGAP_ART_DIR "/sprites"
#define TAGAP_LAYERS_DIR TAGAP_ART_DIR "/layers"

enum game_state
{
    // Loading into the game
    GAME_STATE_BOOT,

    // On the main menu
    GAME_STATE_MENU,

    // Loading a game level
    GAME_STATE_LEVEL_LOAD,

    // In a game level
    GAME_STATE_LEVEL,
};

// Global game state
struct tagap
{
    // Current state type
    enum game_state type;

    // Current state info
    struct state_menu m;
    struct state_level l;

    // Current client-side input/state info
    vec3s cam_pos;
    i32 mouse_x, mouse_y, mouse_scroll;
    u8 m_state;
    const u8 *kb_state;

    // Renderer
    struct vulkan_renderer vulkan;

    // Internal state
    u64 now, last_frame, last_sec;
    f64 dt;
    u32 draw_calls;
};

#define DT (g_state.dt)

extern struct tagap g_state;

static inline void
tagap_set_state(enum game_state state)
{
    g_state.type = state;
}

#endif
