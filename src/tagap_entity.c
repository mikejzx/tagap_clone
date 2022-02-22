#include "pch.h"
#include "tagap.h"
#include "tagap_entity.h"
#include "tagap_sprite.h"
#include "renderer.h"
#include "vulkan_renderer.h"
#include "state_level.h"
#include "entity_pool.h"

#define KICK_TIMER_MAX (0.1f)
#define KICK_AMOUNT (17.0f)
#define PUSHUP_MAX_ANGLE (5.0f)

static void entity_spawn_missile(struct tagap_entity *,
    struct tagap_entity_info *);
static void create_gunent(struct tagap_entity *, bool);

/*
 * Spawn an entity into the game, and add all sprites, stats, etc.
 */
void
entity_spawn(struct tagap_entity *e)
{
    // Don't spawn entities with no info or that are already spawned in
    if (!e->info || e->is_spawned) return;

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
        struct renderable_quad_info quad =
        {
            .shader = SHADER_DEFAULT_NO_ZBUFFER,
            .centre_x = true,
            .centre_y = true,
            .depth = DEPTH_ENTITIES + g_map->current_entity_depth / 10.0f,
            .make_bounds = true,
        };
        e->sprites[s] = renderer_get_renderable_quad(&quad);
        struct renderable *r = e->sprites[s];

        r->tex = spr->info->frames[spr->vars[SPRITEVAR_KEEPFRAME]].tex;
        r->pos = e->position;
        r->offset = spr->offset;
        r->pos.y *= -1.0f;

        // Set flip mode
        if (e->info->think.mode == THINK_AI_AIM ||
            spr->vars[SPRITEVAR_AIM])
        {
            r->rot = e->aim_angle;
            r->flags &= ~RENDERABLE_FLIPPED_BIT;
        }
        else
        {
            r->rot = 0;
            SET_BIT(r->flags, RENDERABLE_FLIPPED_BIT, !!e->facing);
        }

        // We want the renderer to do the scaling for us on sprites
        r->flags |= RENDERABLE_TEX_SCALE_BIT;

        if (e->info->think.mode == THINK_AI_USER)
        {
            // Don't waste CPU cycles doing culling calculations on player
            // sprites
            r->flags |= RENDERABLE_NO_CULL_BIT;
        }

        if (e->info->stats[STAT_FX_EXPAND])
        {
            // Enable scaling on texture
            r->flags |= RENDERABLE_SCALED_BIT;
            r->scale = 1.0f;
        }

        if (e->info->stats[STAT_FX_FADE])
        {
            // Enable extra shading, and modify the alpha channel to fade it
            r->flags |= RENDERABLE_EXTRA_SHADING_BIT;
            r->extra_shading = (vec4s)GLMS_VEC4_ONE_INIT;
        }

        ++g_map->current_entity_depth;
    }

    // Write the initial ammo amount
    for (u32 w = 0; w < WEAPON_SLOT_COUNT; ++w)
    {
        e->weapons[w].ammo = e->info->ammo[w];
    }

    if (e->info->think.mode == THINK_AI_USER)
    {
        // Set this entity as the player
        g_map->player = e;

        e->weapons[0].ammo = 15;
#if 0 //DEBUG
        // Temporary: give user a bunch of ammo to start with (to test
        // different weapons)
        e->weapons[1].ammo = 300;
        e->weapons[2].ammo = 400;
        e->weapons[3].ammo = 700;
        e->weapons[4].ammo = 500;
        e->weapons[5].ammo = 500;
        e->weapons[6].ammo = 500;
#endif
        //e->weapons[0].has_akimbo = true;
    }

    // Create non-RENDERFIRST gun entities
    create_gunent(e, false);

    if (e->weapon_slot >= 0)
    {
        entity_change_weapon_slot(e, e->weapon_slot);

        // Reset reload timer of all weapons
        for (u32 i = 0; i < WEAPON_SLOT_COUNT; ++i)
        {
            e->weapons[i].reload_timer = -1.0f;
        }
    }

    // Initialise entity effects
    if (entity_fx_init(e) < 0) return;

    entity_reset(e, e->position, e->aim_angle, e->flipped);
    e->is_spawned = true;
}

void
entity_update(struct tagap_entity *e)
{
    if (!e->active) return;

    // Reset collision result
    memset(&e->collision, 0, sizeof(struct collision_result));

    // Perform think routine
    entity_think(e);

    // Update weapon state
    e->firing_now = false;
    if (e->info->has_weapon)
    {
        // Update weapon kickback timer
        e->weapon_kick_timer = max(e->weapon_kick_timer - DT, 0.0f);

        struct tagap_entity_info *missile_info =
            g_level->weapons[e->weapon_slot].primary;

        // Fire weapon
        f32 attack_delay = missile_info->think.attack_delay;
        if (e->weapon_slot == 0 && !e->weapons[0].has_akimbo)
        {
            attack_delay *= 1.2f;
        }
        if (e->weapons[e->weapon_slot].reload_timer < 0.0f &&
            e->weapons[e->weapon_slot].ammo > 0 &&
            e->inputs.fire &&
            e->attack_timer > attack_delay)
        {
            entity_spawn_missile(e, missile_info);
            e->fx.muzzle_timer = 1.0f;
            e->attack_timer = 0.0f;
            e->weapon_kick_timer = KICK_TIMER_MAX;
            e->firing_now = true;

            // Decrement ammo
            --e->weapons[e->weapon_slot].ammo;

            // Begin reload
            if (e->weapons[e->weapon_slot].ammo <= 0 &&
                g_level->weapons[e->weapon_slot].reload_time > 0.0f)
            {
                e->weapons[e->weapon_slot].reload_timer = 0.0f;
            }
        }

        // Update reload timer
        if (e->weapons[e->weapon_slot].reload_timer >= 0.0f)
        {
            // Increment timer
            e->weapons[e->weapon_slot].reload_timer += DT;

            if (e->weapons[e->weapon_slot].reload_timer >=
                g_level->weapons[e->weapon_slot].reload_time)
            {
                // Reload finished
                e->weapons[e->weapon_slot].reload_timer = -1.0f;
                e->weapons[e->weapon_slot].ammo =
                    g_level->weapons[e->weapon_slot].magazine_size;
            }
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

    f32 flip_mul = (f32)e->flipped * -2.0f + 1.0f;

    // TODO: only enable bobbing if entity during load detects that sprites
    //       need bobbing?
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
            tangent *= flip_mul;
            sprite_rot_offset += tangent;
        }

        // Apply Y-wise float effect (used for items)
        if (e->info->stats[STAT_FX_FLOAT])
        {
            static const f32 FLOATING_BOB_AMOUNT = 8.0f;
            sprite_offset.y = bob_sin * FLOATING_BOB_AMOUNT + 16.0f;
        }

#if SLIDE_ENABLED
        if (e->slide_timer >= 0.0f)
        {
            sprite_rot_offset =
                lerpf(0.0f, -90.0f, clamp01(e->slide_timer / 0.1f));
        }
#endif

        // By default use entity position and normal rotation
        vec2s spr_pos = sprite_offset;
        f32 spr_rot =  entity_get_rot(e) + sprite_rot_offset;

        // Flip sprites based on entity facing
        SET_BIT(spr_r->flags, RENDERABLE_FLIPPED_BIT, e->flipped);

        // Update sprite positions
        switch(spr->anim)
        {
        case ANIM_WEAPON2:
        {
            if (!e->info->has_weapon) break;

            // Hide second weapon sprite if entity does not have akimbo
            SET_BIT(spr_r->flags,
                RENDERABLE_HIDDEN_BIT,
                !(e->weapon_slot == 0 && e->weapons[0].has_akimbo));

            if (!(spr_r->flags & RENDERABLE_HIDDEN_BIT)) goto weapon_anim;
        } break;
        // ... falls through
        case ANIM_WEAPON:
        {
        weapon_anim:
            // On reload we rotate the weapon 360 degrees
            f32 reload_angle = 0.0f;
            if (e->weapons[e->weapon_slot].reload_timer >= 0.0f)
            {
                reload_angle =
                    (e->weapons[e->weapon_slot].reload_timer /
                    g_level->weapons[e->weapon_slot].reload_time) * -360.0f;
            }

            // Recoil effect
            f32 pushup_rot = 0.0f;
            if (e->weapon_slot >= 0 && e->weapon_slot < WEAPON_SLOT_COUNT &&
                g_level->weapons[e->weapon_slot].primary &&
                g_level->weapons[e->weapon_slot].primary->
                    stats[STAT_FX_PUSHUP])
            {
                pushup_rot = clamp01(e->weapon_kick_timer / KICK_TIMER_MAX)
                    * PUSHUP_MAX_ANGLE;
            }

            // Rotate weapons to the aim angle
            spr_rot = e->aim_angle + reload_angle + pushup_rot;

            // Apply weapon kick animation
            f32 kick_value = KICK_AMOUNT * e->weapon_kick_timer;
            spr_pos.x += kick_value * cosf(glm_rad(e->aim_angle)) * -flip_mul;
            spr_pos.y += kick_value * -sinf(glm_rad(e->aim_angle));

            // Texture adjustments (only apply to the weapon owner)
            if (!e->info->has_weapon) break;
            if (!e->owner)
            {
                // Use akimbo texture frame on uzi (slot 0) if we have it
                u32 tex_slot = e->weapon_slot + 2;
                if (e->weapon_slot == 0 && e->weapons[0].has_akimbo)
                {
                    // Use akimbo texture
                    tex_slot = 0;
                }
                if (tex_slot < spr->info->frame_count)
                {
                    spr_r->tex = spr->info->frames[tex_slot].tex;
                    if (spr->anim != ANIM_WEAPON2)
                    {
                        // Remove hidden flag from weapon1 texture
                        spr_r->flags &= ~RENDERABLE_HIDDEN_BIT;
                    }
                }
                else
                {
                    // Hide the texture
                    spr_r->tex = spr->info->frames[0].tex;
                    spr_r->flags |= RENDERABLE_HIDDEN_BIT;
                }
            }
            else
            {
                // Gun entity texture (unhide it)
                spr_r->tex = spr->info->frames[0].tex;
                spr_r->flags &= ~RENDERABLE_HIDDEN_BIT;
            }
        } break;

        // Head animation
        case ANIM_FACE:
        {
            if (spr_rot < 90.0f)
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

    // Apply effects
    entity_fx_update(e);
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
    // Die effects/gibs (e.g. explosion, etc.) and SFX
    // ...

    // Move missile back into pool if it is pooled
    if (!entity_pool_return(e))
    {
        // Wasn't in pool, so just deactivate for now
        entity_set_inactive_hidden(e, true);
    }
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
    e->jump_timer = -1.0f;
    e->jump_reset = false;
    e->slide_timer = -1.0f;
    e->next_blink = 0;
    e->blink_timer = 0;
    e->timer_tempmissile = 0.0f;
    e->attack_timer = 0.0f;
    entity_fx_reset(&e->fx);
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
    missile_e->owner = owner;
    missile_e->with_owner = false;
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
        SET_BIT(e->sprites[i]->flags, RENDERABLE_HIDDEN_BIT, h);
    }

    // Toggle lights
    entity_fx_toggle(&e->fx, h);
}

/*
 * Creates gunentities
 */
static void
create_gunent(struct tagap_entity *e, bool render_first)
{
    // No weapon
    if (e->weapon_slot < 0) return;

    for (u32 w = 0; w < WEAPON_SLOT_COUNT; ++w)
    {
        // The missile contains info about what gunentity to use
        struct tagap_entity_info *missile_info =
            g_level->weapons[w].primary;

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

        // Manually spawn the entity in
        entity_spawn(e->weapons[w].gunent);

        // Apply model offset
        for (u32 s = 0; s < e->weapons[w].gunent->info->sprite_count; ++s)
        {
            // A bit dodgey?
            e->weapons[w].gunent->sprites[s]->offset = (vec2s)
            {
                e->weapons[w].gunent->sprites[s]->offset.x +
                    missile_info->gun_entity->offsets[OFFSET_MODEL_OFFSET].x,
                e->weapons[w].gunent->sprites[s]->offset.y +
                    missile_info->gun_entity->offsets[OFFSET_MODEL_OFFSET].y,
            };
        }
    }
}

/* Change active weapon slot of entity */
void
entity_change_weapon_slot(struct tagap_entity *e, i32 slot)
{
    if (slot < 0 || slot >= WEAPON_SLOT_COUNT)
    {
        LOG_WARN("[tagap_entity] invalid weapon slot %d", slot);
        return;
    }

    e->weapon_slot = slot;

    // Enable the correct gunentity
    for (u32 w = 0; w < WEAPON_SLOT_COUNT; ++w)
    {
        if (!e->weapons[w].gunent) continue;

        entity_set_inactive_hidden(e->weapons[w].gunent, w != slot);
    }
}
