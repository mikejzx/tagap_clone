#include "pch.h"
#include "entity_pool.h"
#include "tagap.h"
#include "tagap_entity.h"

static struct entity_pool
{
    // Name of the entity in this pool
    char name[ENTITY_NAME_MAX];
    
    // Max size and number of entities in the pool
    size_t limit;

    // List of the entities in the pool
    struct tagap_entity *e;
} pools[POOL_ID_COUNT] =
{
    [POOL_ID_UNKNOWN]       = { 0 },
    [POOL_ID_LASER]         = { .name = "laser",         .limit = 128 },
    [POOL_ID_HEAVY_LASER]   = { .name = "redlaser",      .limit = 128 },
    [POOL_ID_FLAME]         = { .name = "flame",         .limit = 128 },
    [POOL_ID_ROCKET]        = { .name = "rocket",        .limit = 64 },
    [POOL_ID_GUIDED_ROCKET] = { .name = "rocket_guided", .limit = 32 },
    [POOL_ID_HOMING_ROCKET] = { .name = "",              .limit = 32 },
};

/* Initialise the entity pools */
void 
entity_pool_init(void)
{
    // Create the pools
    for (u32 i = 1; i < POOL_ID_COUNT; ++i)
    {
        // Skip unused pools
        if (pools[i].name[0] == '\0') continue;

        // Get the info of the entity that we want to pool
        struct tagap_entity_info *info = NULL;
        for (u32 e = 0; e < g_level->entity_info_count; ++e)
        {
            struct tagap_entity_info *ei = &g_level->entity_infos[e];
            if (strcmp(ei->name, pools[i].name) == 0)
            {
                info = ei;
                ei->pool_id = i;
                break;
            }
        }
        if (!info)
        {
            LOG_WARN("[entity_pool] pool '%s' has no entities", pools[i].name);
            continue;
        }

        pools[i].e = calloc(pools[i].limit, sizeof(struct tagap_entity));

        // Fill the pool with entities and "spawn" them into the level
        for (u32 e = 0; e < pools[i].limit; ++e)
        {
            pools[i].e[e].info = info;
            entity_spawn(&pools[i].e[e]);

            // Set the entity inactive
            entity_set_inactive_hidden(&pools[i].e[e], true);
        }
    }
}

/* Free the entity pools */
void
entity_pool_deinit(void)
{
    for (u32 i = 1; i < POOL_ID_COUNT; ++i)
    {
        if (!pools[i].e) continue;

        // Free entities in the pool
        for (u32 e = 0; e < pools[i].limit; ++e) entity_free(&pools[i].e[e]);

        // Free the pool
        free(pools[i].e);
    }
}

/* Update all entities in all pools */
void
entity_pool_update(void)
{
    for (u32 i = 1; i < POOL_ID_COUNT; ++i)
    {
        // Skip unused pools
        if (pools[i].name[0] == '\0' || !pools[i].e) continue;

        // Update all entities in the pool
        for (u32 e = 0; e < pools[i].limit; ++e) entity_update(&pools[i].e[e]);
    }
}

/* Get pooled entity by pool ID */
struct tagap_entity *
entity_pool_get_by_id(enum entity_pool_id id)
{
    // Invalid pool ID
    if (id <= 0 || id >= POOL_ID_COUNT)
    {
        LOG_WARN("[entity_pool] invalid pool ID %d", id);
        return NULL;
    }

    for (u32 e = 0; e < pools[id].limit; ++e)
    {
        if (!pools[id].e[e].active)
        {
            entity_set_inactive_hidden(&pools[id].e[e], false);
            return &pools[id].e[e];
        }
    }
    LOG_WARN("[entity_pool] out of entities for pool %d", id);
    return NULL;
}

/* Get pooled entity by entity info */
struct tagap_entity *
entity_pool_get(struct tagap_entity_info *e)
{
    return entity_pool_get_by_id(e->pool_id);
}

/* Return an entity to the pool */
void 
entity_pool_return(struct tagap_entity *e)
{
    if (e->info->pool_id <= 0 || e->info->pool_id >= POOL_ID_COUNT) return;
    entity_set_inactive_hidden(e, true);
}
