#include "pch.h"
#include "tagap.h"
#include "tagap_entity.h"
#include "tagap_sprite.h"
#include "renderer.h"
#include "vulkan_renderer.h"
#include "state_level.h"
#include "entity_pool.h"

#define KICK_TIMER_MAX (0.1f)
#define KICK_AMOUNT (16.0f)

static void entity_spawn_missile(struct tagap_entity *, 
    struct tagap_entity_info *);
static void create_gunent(struct tagap_entity *, bool);

/*
 * Spawn an entity into the game, and add all sprites, stats, etc.
 */
void
entity_spawn(struct tagap_entity *e)
{
    // Set weapon slot
    e->weapon_slot = e->info->has_weapon ? e->info->stats[STAT_S_WEAPON] : -1;

    // Create gun entities that are flagged as RENDERFIRST
    create_gunent(e, true);

    // Create sprites
    for (u32 s = 0; s < e->info->sprite_count; ++s)
    {
        const struct tagap_entity_sprite *spr = &e->info->sprites[s];

        // Load the sprites into the sprite info
        if (!tagap_sprite_load(spr->info))
        {
            continue;
        }

        // Create the sprite renderable
        e->sprites[s] = renderer_get_renderable_quad();
        struct renderable *r = e->sprites[s];

        r->tex = spr->info->frames[0].tex;
        r->pos = e->position;
        r->offset = spr->offset;
        r->pos.y *= -1.0f;

        // Set flip mode
        if (e->info->think.mode == THINK_AI_AIM ||
            spr->vars[SPRITEVAR_AIM])
        {
            r->rot = e->aim_angle;
            r->flipped = false;
        }
        else
        {
            r->rot = 0;
            r->flipped = !!e->facing;
        }

        // We want the renderer to do the scaling for us on sprites
        r->tex_scale = true;

        if (e->info->think.mode == THINK_AI_USER)
        {
            // Don't waste CPU cycles doing culling calculations on player
            // sprites
            r->no_cull = true;
        }
    }

    // Temporary: give user a bunch of ammo to start with (to test different
    // weapons)
#if DEBUG
    if (e->info->think.mode == THINK_AI_USER)
    {
        e->weapons[1].ammo = 30;
        e->weapons[2].ammo = 40;
        e->weapons[3].ammo = 70;
        e->weapons[4].ammo = 50;
        e->weapons[5].ammo = 50;
        e->weapons[6].ammo = 50;
    }
#endif

    // Create non-RENDERFIRST gun entities
    create_gunent(e, false);

    if (e->weapon_slot >= 0) 
    {
        entity_change_weapon_slot(e, e->weapon_slot);
    }
}

void
entity_update(struct tagap_entity *e)
{
    // Reset collision result
    memset(&e->collision, 0, sizeof(struct collision_result));

    // Perform think routine
    entity_think(e);

    // Update weapon state
    if (e->info->has_weapon)
    {
        // Update weapon kickback timer
        e->weapon_kick_timer = max(e->weapon_kick_timer - DT, 0.0f);

        struct tagap_entity_info *missile_info =
            g_state.l.weapons[e->weapon_slot].primary;

        // Fire weapon
        if (e->inputs.fire && e->attack_timer > 
            missile_info->think.attack_delay)
        {
            entity_spawn_missile(e, missile_info);
            e->attack_timer = 0.0f;
            e->weapon_kick_timer = KICK_TIMER_MAX;
        }

        // Update attack timer
        e->attack_timer += DT;
    }

    // Apply movetype
    entity_movetype(e);

    // Post-movement/collision think stuff
    switch(e->info->think.mode)
    {
    case THINK_AI_USER:
    {
        // This is the player; update camera position
        g_state.cam_pos = (vec3s)
        {{
             e->position.x - (f32)WIDTH_INTERNAL / 2,
             -e->position.y - (f32)HEIGHT_INTERNAL / 2 - 100.0f,
             0.0f
        }};
    } break;

    case THINK_AI_MISSILE:
    {
        // If the missile collides with something then destroy it
        if (e->info->think.attack == THINK_ATTACK_AI_BLOW &&
            (e->collision.above ||
            e->collision.left ||
            e->collision.right ||
            e->collision.below))
        {
            entity_die(e);
        }
    } break;

    default: break;
    }

    // Gun entities
    if (e->with_owner && e->owner)
    {
        e->position = e->owner->position;
        e->aim_angle  = e->owner->aim_angle;
        e->flipped  = e->owner->flipped;
        e->weapon_kick_timer  = e->owner->weapon_kick_timer;
    }

    // TODO: only enable bobbing if entity during load detects that sprites
    // need bobbing
    f32 bob_sin = 0.0f;
    if (e->info->sprite_count)
    {
        if (e->owner && e->with_owner)
        {
            // Child entities use same bobbing timer as owner
            bob_sin = sinf(e->owner->bobbing_timer);
        }
        else
        {
            bob_sin = sinf(e->bobbing_timer);
        }
    }

    // Update sprites
    for (u32 s = 0; s < e->info->sprite_count; ++s)
    {
        struct tagap_entity_sprite *spr = &e->info->sprites[s];
        struct renderable *spr_r = e->sprites[s];

        // SPRITEVAR animations/bobbing effects
        vec2s sprite_offset = (vec2s)GLMS_VEC2_ZERO_INIT;
        f32 sprite_rot_offset = 0.0f;
        f32 bob_mul = e->velo.x;
        if (e->owner && e->with_owner) bob_mul = e->owner->velo.x;
        if (spr->vars[SPRITEVAR_BOB] && bob_mul != 0.0f)
        {
            // Bobbing animation
            f32 bob_var = (f32)spr->vars[SPRITEVAR_BOB];
            sprite_offset.y =
                bob_mul *
                bob_sin * bob_var / 25.0f;
        }
        else if (spr->vars[SPRITEVAR_BIAS])
        {
            if (e->velo.x != 0.0f)
            {
                // Set to the second frame
                if (spr->info->frame_count > 0)
                {
                    // Not sure if this is a good idea; however something like
                    // this is needed for leg animations to work properly.
                    spr_r->tex = spr->info->frames[1].tex;
                }

                // Apply rotation offset
                sprite_rot_offset = bob_sin *
                    (f32)spr->vars[SPRITEVAR_BIAS] / 2.0f;
            }
            else
            {
                // Set legs to first frame
                spr_r->tex = spr->info->frames[0].tex;
            }

            // Add the linedef's tangent angle
            f32 tangent = glm_deg(atan(e->collision.floor_gradient));
            if (e->flipped) tangent *= -1.0f;
            sprite_rot_offset += tangent;
        }

        // By default use entity position and normal rotation
        vec2s spr_pos = sprite_offset;
        f32 spr_rot =  entity_get_rot(e) + sprite_rot_offset;

        // Flip sprites based on entity facing
        spr_r->flipped = e->flipped;

        // Update sprite positions
        switch(spr->anim)
        {
        case ANIM_WEAPON2:
        {
            if (!e->info->has_weapon) break;

            // Hide second weapon sprite if entity does not have akimbo
            spr_r->hidden = true;//e->weapon_slot != 0 || 
                //!e->info->stats[STAT_S_AKIMBO];
        } break;
        // ... falls through
        case ANIM_WEAPON:
        {
            // Rotate weapons to the aim angle
            spr_rot = e->aim_angle;

            // Apply weapon kick animation
            f32 kick_value = KICK_AMOUNT * e->weapon_kick_timer * 
                ((f32)e->flipped * 2.0f - 1.0f);
            f32 kick_x = cosf(glm_rad(e->aim_angle));
            spr_pos.x += kick_value * kick_x;
            spr_pos.y += kick_value * sinf(glm_rad(e->aim_angle));

            // Texture adjustments (only apply to the weapon owner)
            if (!e->info->has_weapon) break;
            if (!e->owner)
            {
                // Use akimbo texture frame on uzi (slot 0) if we have it
                u32 tex_slot = e->weapon_slot + 2;
                if (e->weapon_slot == 0 &&
                    e->info->stats[STAT_S_AKIMBO])
                {
                    // Use akimbo texture
                    tex_slot = 0;
                }
                if (tex_slot < spr->info->frame_count)
                {
                    spr_r->tex = spr->info->frames[tex_slot].tex;
                    if (spr->anim != ANIM_WEAPON2) spr_r->hidden = false;
                }
                else
                {
                    spr_r->tex = spr->info->frames[0].tex;
                    spr_r->hidden = true;
                }
            }
            else
            {
                // Gun entity
                spr_r->tex = spr->info->frames[0].tex;
                spr_r->hidden = false;
            }
        } break;

        // Head animation
        case ANIM_FACE:
        {
            if (spr_rot  < 90.0f)
            {
                spr_rot = 30.0f * (e->aim_angle / 90.0f);
            }

            // Blink animation
            if (g_state.now > e->next_blink)
            {
                e->next_blink = g_state.now +
                    ((rand() % 4000) + 600) * NS_PER_MS;
                e->blink_timer = 0.0f;
            }
            if (e->blink_timer < 0.15f)
            {
                spr_r->tex = spr->info->frames[1].tex;
            }
            else
            {
                spr_r->tex = spr->info->frames[0].tex;
            }
            e->blink_timer += DT;
        } break;

        // No animation
        case ANIM_NONE:
        default: break;
        }

        spr_r->pos = glms_vec2_add(e->position, spr_pos);
        spr_r->pos.y *= -1.0f;
        spr_r->rot = spr_rot;
    }
}

void
entity_free(struct tagap_entity *e)
{
    for (u32 s = 0; s < e->info->sprite_count; ++s)
    {
        tagap_sprite_free(e->info->sprites[s].info);
    }
}

void
entity_die(struct tagap_entity *e)
{
    // Die effects (e.g. explosion, etc.)
    // ...

    // Move missile back into pool if it is pooled
    entity_pool_return(e);
}

void
entity_reset(
    struct tagap_entity *e,
    vec2s pos,
    f32 aim,
    bool flipped)
{
    e->position = pos;
    e->aim_angle = aim;
    e->flipped = flipped;

    // Reset timers
    e->bobbing_timer = 0.0f;
    e->bobbing_timer_last = 0.0f;
    e->jump_timer = 0.0f;
    e->jump_reset = false;
    e->next_blink = 0;
    e->blink_timer = 0;
    e->timer_tempmissile = 0.0f;
    e->attack_timer = 0.0f;
}

void
entity_spawn_missile(
    struct tagap_entity *owner,
    struct tagap_entity_info *missile)
{
    vec2s spawn_pos = missile->offsets[OFFSET_WEAPON_MISSILE];
    spawn_pos.x *= (owner->flipped ? -1.0f : 1.0f);
    spawn_pos = glms_vec2_add(owner->position, spawn_pos);

    struct tagap_entity *missile_e = entity_pool_get(missile);

    if (!missile_e) return;

    entity_reset(missile_e,
        spawn_pos,
        owner->aim_angle,
        owner->flipped);
}

/*
 * Sets an entity activity and hidden state
 */
void 
entity_set_inactive_hidden(struct tagap_entity *e, bool h)
{
    e->active = !h;
    for (u32 i = 0; i < e->info->sprite_count; ++i)
    {
        e->sprites[i]->hidden = h;
    }
}

/*
 * Creates gunentities
 */
static void 
create_gunent(struct tagap_entity *e, bool render_first)
{
    // No weapon
    if (e->weapon_slot < 0) return;

    for (u32 w = 0; w < PLAYER_WEAPON_COUNT; ++w)
    {
        // The missile contains info about what gunentity to use
        struct tagap_entity_info *missile_info =
            g_state.l.weapons[w].primary;

        if (!missile_info || !missile_info->gun_entity)
        {
            // This weapon has no gunentity
            e->weapons[w].gunent = NULL;
            continue;
        }

        // Check if it's in the render order we want
        if (missile_info->gun_entity->stats[STAT_FX_RENDERFIRST] != 
            (i32)render_first)
        {
            continue;
        }

        // Prevent doubling up on gunentities
        if (e->weapons[w].gunent != NULL) continue;

        // Create gunentity entity
        e->weapons[w].gunent = level_add_entity(missile_info->gun_entity);
        if (!e->weapons[w].gunent)
        {
            // Failed to add entitiy
            LOG_WARN("[tagap_entity] failed to add gunentity");
            continue;
        }
        // Set gunentity data
        e->weapons[w].gunent->position = e->position;
        e->weapons[w].gunent->aim_angle = e->aim_angle;
        e->weapons[w].gunent->flipped = e->flipped;
        e->weapons[w].gunent->owner = e;
        e->weapons[w].gunent->with_owner = true;

        // We need to manually spawn the entity in
        // TODO: less dodgey fix; non-AI_USER entities will cause duplicate
        //       gunentity.  The double-up prevention check above should be
        //       sufficient for now though
        entity_spawn(e->weapons[w].gunent);
    }
}

/* Change active weapon slot of entity */
void
entity_change_weapon_slot(struct tagap_entity *e, i32 slot)
{
    if (slot < 0 || slot >= PLAYER_WEAPON_COUNT)
    {
        LOG_WARN("[tagap_entity] invalid weapon slot %d", slot);
        return;
    }

    e->weapon_slot = slot;

    // Enable the correct gunentity
    for (u32 w = 0; w < PLAYER_WEAPON_COUNT; ++w)
    {
        if (!e->weapons[w].gunent) continue;

        entity_set_inactive_hidden(e->weapons[w].gunent, w != slot);
    }
}
