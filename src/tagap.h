#ifndef TAGAP_H
#define TAGAP_H

#include "state_level.h"
#include "state_menu.h"
#include "vulkan_renderer.h"

// TAGAP data directory.  May make this adjustable
//#define TAGAP_DATA_DIR "/home/mike/games/TAGAP/data"
#define TAGAP_DATA_DIR "./data"
#define TAGAP_DATA_MOD_DIR "./data_add"
#define TAGAP_SCRIPT_DIR TAGAP_DATA_DIR "/script"
#define TAGAP_TEXTURES_DIR "./data/art/textures"

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
    struct
    {
        struct state_menu m;
        struct state_level l;
    };

    // Renderer
    struct vulkan_renderer vulkan;
};

extern struct tagap g_state;

static inline void
tagap_set_state(enum game_state state)
{
    g_state.type = state;
}

#endif
