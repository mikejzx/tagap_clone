#include "pch.h"
#include "tagap.h"
#include "tagap_theme.h"
#include "tagap_linedef.h"
#include "tagap_entity.h"
#include "tagap_polygon.h"
#include "state_level.h"
#include "renderer.h"
#include "entity_pool.h"

struct level *g_map;
struct state_level *g_level;

void
level_init(void)
{
    LOG_INFO("[state] level init");

    g_level = &g_state.l;
    g_map = &g_state.l.map;

    // Allocate memory
    g_map->linedefs = malloc(LEVEL_MAX_LINEDEFS * sizeof(struct tagap_linedef));
    g_map->linedef_count = 0;
    g_map->polygons = calloc(LEVEL_MAX_POLYGONS, sizeof(struct tagap_polygon));
    g_map->polygon_count = 0;
    g_map->entities = malloc(LEVEL_MAX_ENTITIES * sizeof(struct tagap_entity));
    g_map->entity_count = 0;
    g_map->tmp_entities =
        malloc(LEVEL_MAX_TMP_ENTITIES * sizeof(struct tagap_entity));
    g_map->tmp_entity_count = 0;

    g_state.l.entity_infos =
        malloc(GAME_ENTITY_INFO_LIMIT * sizeof(struct tagap_entity_info));
    g_state.l.entity_info_count = 0;

    g_state.l.theme_infos =
        calloc(GAME_THEME_INFO_LIMIT, sizeof(struct tagap_theme_info));
    g_state.l.theme_info_count = 1;

    g_state.l.sprite_infos =
        malloc(GAME_SPRITE_INFO_LIMIT * sizeof(struct tagap_sprite_info));
    g_state.l.sprite_info_count = 0;

    g_map->theme = &g_state.l.theme_infos[0];
}

void
level_reset(void)
{
    // Cleanup entities
    for (u32 i = 0; i < g_map->entity_count; ++i)
    {
        entity_free(&g_map->entities[i]);
    }
    for (u32 i = 0; i < g_map->tmp_entity_count; ++i)
    {
        entity_free(&g_map->tmp_entities[i]);
    }

    // Clear out all current data
    g_map->title[0] = g_map->desc[0] = '\0';
    g_map->linedef_count = 0;
    g_map->polygon_count = 0;
    g_map->entity_count = 0;
    g_map->tmp_entity_count = 0;

    // Need to set this to zero to reset polygon point counters
    memset(g_map->polygons, 0,
        LEVEL_MAX_POLYGONS * sizeof(struct tagap_polygon));
}

void
level_deinit(void)
{
    entity_pool_deinit();

    LOG_INFO("[state] level cleanup");
    free(g_map->linedefs);
    free(g_map->polygons);
    free(g_map->entities);
    free(g_state.l.entity_infos);
    free(g_state.l.theme_infos);
    free(g_state.l.sprite_infos);
}

/*
 * Add polygons, linedefs, etc. to the renderer
 */
void
level_submit_to_renderer(void)
{
    // Polygons are rendered first so add them to the renderables list first
    i32 i;
    for (i = 0; i < g_map->polygon_count; ++i)
    {
        renderer_add_polygon(&g_map->polygons[i]);
    }

    // Finally generate line geometry.
    renderer_add_linedefs(g_map->linedefs, g_map->linedef_count);
}

/*
 * Spawn all initial entities in the level
 */
void
level_spawn_entities()
{
    for (u32 i = 0; i < g_map->entity_count; ++i)
    {
        // Spawn non-user things first
        if (g_map->entities[i].info->think.mode == THINK_AI_USER) continue;
        entity_spawn(&g_map->entities[i]);
    }

    // Spawn the pooled entities
    entity_pool_init();

    // Spawn user themself last
    for (u32 i = 0; i < g_map->entity_count; ++i)
    {
        // Spawn non-user things first
        if (g_map->entities[i].info->think.mode != THINK_AI_USER) continue;
        entity_spawn(&g_map->entities[i]);
    }
}

/*
 * Update all entities in the level
 */
void
level_update()
{
    for (u32 i = 0; i < g_map->entity_count; ++i)
    {
        entity_update(&g_map->entities[i]);
    }
    for (u32 i = 0; i < LEVEL_MAX_TMP_ENTITIES; ++i)
    {
        if (!g_map->tmp_entities[i].active) continue;

        entity_update(&g_map->tmp_entities[i]);
    }
    // Update pooled entities
    entity_pool_update();
}

/*
 * Adds an entity to the entity list
 */
struct tagap_entity *
level_add_entity(struct tagap_entity_info *info)
{
    if (g_map->entity_count + 1 >= LEVEL_MAX_ENTITIES)
    {
        LOG_ERROR("[state_level]: entity limit (%d) exceeded", 
            LEVEL_MAX_ENTITIES);
        return NULL;
    }
    struct tagap_entity *e = &g_map->entities[g_map->entity_count++];
    memset(e, 0, sizeof(struct tagap_entity));
    e->info = info;
    return e;
}

/*
 * Spawn a temporary entity in the level
 */
struct tagap_entity *
level_spawn_entity(
    struct tagap_entity_info *ei,
    vec2s position,
    f32 aim_angle,
    bool flipped)
{
    if (g_map->tmp_entity_count + 1 >= LEVEL_MAX_TMP_ENTITIES)
    {
        // No entity slots left
        LOG_WARN("[state_level] cannot spawn temporary entity; "
            "limit (%d) reached ", LEVEL_MAX_TMP_ENTITIES);
        return NULL;
    }
    struct tagap_entity *e = &g_map->tmp_entities[g_map->tmp_entity_count++];

    memset(e, 0, sizeof(struct tagap_entity));
    e->info = ei;
    e->position = position;
    e->aim_angle = aim_angle;
    e->flipped = flipped;

    // Actually spawn it in
    ++g_map->tmp_entity_count;
    entity_spawn(e);
    return e;
}
