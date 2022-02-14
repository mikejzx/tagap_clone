#ifndef ENTITY_POOL_H
#define ENTITY_POOL_H

struct tagap_entity_info;

/*
 * entity_pool.h
 *
 * Manages entity pools.  At the moment the way we implement this is by
 * explicitly specifying what entities we want to be pooled, the most obvious
 * example being weapon projectiles.
 *
 * TODO: instanced rendering for these sorts of things, e.g. for flames. Should
 *       be fairly straightforward once a particle system is implemented (which 
 *       should definitely use batch rendering/instancing).
 */

enum entity_pool_id
{
    POOL_ID_UNKNOWN = 0,
    
    // NOTE: trace attacks (bullets, beams, etc.) are not in here as we don't
    //       create them as "entities" per se, but rather as general
    //       renderables?

    POOL_ID_LASER,         // Plasmagun laser
    POOL_ID_HEAVY_LASER,   // Heavy (red) laser
    POOL_ID_FLAME,         // Flamethrower flames
    POOL_ID_ROCKET,        // Rocket projectiles
    POOL_ID_GUIDED_ROCKET, // Guided rocket projectiles
    POOL_ID_HOMING_ROCKET, // Homing rocket projectiles

    POOL_ID_COUNT
};

// See implementation file for pool list

void entity_pool_init(void);
void entity_pool_deinit(void);
void entity_pool_update(void);
struct tagap_entity *entity_pool_get(struct tagap_entity_info *);
struct tagap_entity *entity_pool_get_by_id(enum entity_pool_id);
void entity_pool_return(struct tagap_entity *);

#endif
