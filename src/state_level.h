#ifndef STATE_LEVEL_H
#define STATE_LEVEL_H

#include "types.h"
#include "player.h"
#include "tagap_script.h"

/*
 * state_level.h
 *
 * Represents state whilst actually in the game
 */

#define LEVEL_TITLE_MAX 32
#define LEVEL_DESC_MAX 64

// Hard-coded constants for now
#define LEVEL_MAX_LINEDEFS 512
#define LEVEL_MAX_POLYGONS 512
#define LEVEL_MAX_ENTITIES 512
#define GAME_ENTITY_LIMIT 1024

struct state_level
{
    // Level map file path, used solely for loading the map
    // TODO use a proper constant size for map path
    char map_path[128];

    // Level data
    struct level 
    {
        // Level name and description
        char title[LEVEL_TITLE_MAX];
        char desc[LEVEL_DESC_MAX];

        // Level linedefs
        struct tagap_linedef *linedefs;
        i32 linedef_count;

        // Level polygons
        struct tagap_polygon *polygons;
        i32 polygon_count;

        // Entities that have been added into the level
        struct tagap_entity *entities;
        i32 entity_count;
    } map;

    // Global entity definitions (do not need to be in the level).  These are
    // read when the game starts up.
    struct tagap_entity *entity_list;
    i32 entity_list_count;

    // Player state
    struct player player;
};

extern struct level *g_map;

void state_level_init(void);
void state_level_reset(void);
void state_level_deinit(void);

void state_level_submit_to_renderer(void);

static inline i32
level_load(const char *fpath)
{
    LOG_INFO("[level] loading '%s'", fpath);
    i32 status = tagap_script_run(fpath);
    LOG_INFO("[level] load %s", status ? "failed" : "complete");
    return status;
}

#endif
