#include "pch.h"
#include "tagap.h"
#include "tagap_linedef.h"
#include "tagap_entity.h"
#include "tagap_polygon.h"
#include "state_level.h"
#include "renderer.h"

struct level *g_map;

void 
state_level_init(void)
{
    LOG_INFO("[state] level init");

    g_map = &g_state.l.map;

    // Allocate memory
    g_map->linedefs = malloc(LEVEL_MAX_LINEDEFS * sizeof(struct tagap_linedef));
    g_map->linedef_count = 0;
    g_map->polygons = calloc(LEVEL_MAX_POLYGONS, sizeof(struct tagap_polygon));
    g_map->polygon_count = 0;
    g_map->entities = malloc(LEVEL_MAX_ENTITIES * sizeof(struct tagap_entity));
    g_map->entity_count = 0;

    g_state.l.entity_list = malloc(GAME_ENTITY_LIMIT * sizeof(struct tagap_entity));
    g_state.l.entity_list_count = 0;
}

void
state_level_reset(void)
{
    // Clear out all current data
    g_map->title[0] = g_map->desc[0] = '\0';
    g_map->linedef_count = 0;
    g_map->polygon_count = 0;
    g_map->entity_count = 0;

    // Need to set this to zero to reset polygon point counters
    memset(g_map->polygons, 0, 
        LEVEL_MAX_POLYGONS * sizeof(struct tagap_polygon));
}

void 
state_level_deinit(void)
{
    LOG_INFO("[state] level cleanup");
    free(g_map->linedefs);
    free(g_map->polygons);
    free(g_map->entities);
    free(g_state.l.entity_list);
}

/*
 * Add polygons, linedefs, etc. to the renderer
 */
void 
state_level_submit_to_renderer(void)
{
    // Polygons are rendered first so add them to the renderables list first
    i32 i;
    for (i = 0; i < g_map->polygon_count; ++i)
    {
        renderer_add_polygon(&g_map->polygons[i]);
    }
}
