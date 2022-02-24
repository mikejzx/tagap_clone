#include "pch.h"
#include "tagap.h"
#include "tagap_entity.h"
#include "tagap_entity_think.h"
#include "renderer.h"

static void entity_think_user(struct tagap_entity *);
static void entity_think_missile(struct tagap_entity *);
static void entity_think_item(struct tagap_entity *);

void
entity_think(struct tagap_entity *e)
{
    switch(e->info->think.mode)
    {
    // Entity is controlled by the player
    case THINK_AI_USER: entity_think_user(e); break;

    // Entity is a projectile
    case THINK_AI_MISSILE: entity_think_missile(e); break;

    // Entity is an item
    case THINK_AI_ITEM: entity_think_item(e); break;

    // Static entity
    default:
    case THINK_NONE: break;
    }
}

static void
entity_think_user(struct tagap_entity *e)
{
    // Calculate world point of cursor
    vec2s cursor_world = { g_state.cam_pos.x, g_state.cam_pos.y };
    cursor_world.x += ((f32)g_state.mouse_x / WIDTH) * WIDTH_INTERNAL;
    cursor_world.y += ((f32)g_state.mouse_y / HEIGHT) * HEIGHT_INTERNAL;

    // Calculate aiming angle
    vec2s pos = e->position;
    pos.y += 24.0f;
    f32 diff_x = cursor_world.x - pos.x;
    f32 diff_y = cursor_world.y + pos.y;
    f32 ang = 0.0f;
    if (diff_x != 0.0f)
    {
        ang = glm_deg(atan(diff_y / diff_x));
    }
    f32 factor_x = clamp01(fabs(diff_x) / 50.0f);
    f32 sgn_x = sign(diff_x);
    if (sgn_x == 0.0f) sgn_x = 1.0f;
    f32 target = lerpf(0.0f, 90.0f, fabs(diff_y) / 10.0f);
    ang = lerpf(ang, target * sign(diff_y) * sgn_x,
        (1.0f - factor_x) * clamp01(fabs(diff_y) / 100.0f));

    e->flipped = diff_x < 0.0f;

    // Aim at the cursor
    e->aim_angle = -ang * (e->flipped ? -1 : 1);

    // Set inputs to player input
    if (g_state.kb_state[SDL_SCANCODE_A]) e->inputs.horiz = -1.0f;
    else if (g_state.kb_state[SDL_SCANCODE_D]) e->inputs.horiz = 1.0f;
    else e->inputs.horiz = 0.0f;

    if (g_state.kb_state[SDL_SCANCODE_S]) e->inputs.vert = -1.0f;
    else if (g_state.kb_state[SDL_SCANCODE_W]) e->inputs.vert = 1.0f;
    else e->inputs.vert = 0.0f;

    e->inputs.fire = !!(g_state.m_state & SDL_BUTTON(1));

    // Mouse scroll: weapon slot changes
    if (g_state.mouse_scroll > 0)
    {
        i32 new_slot = 0;
        for (i32 i = PLAYER_WEAPON_COUNT - 1; i > -1; --i)
        {
            if (e->weapons[i].ammo || i == 0)
            {
                new_slot = i;
                break;
            }
        }
        for (i32 i = (i32)e->weapon_slot - 1; i > -1; --i)
        {
            if (e->weapons[i].ammo || i == 0)
            {
                new_slot = i;
                break;
            }
        }
        entity_change_weapon_slot(e, new_slot);
    }
    else if (g_state.mouse_scroll < 0)
    {
        i32 new_slot = 0;
        for (i32 i = (i32)e->weapon_slot + 1; i < PLAYER_WEAPON_COUNT; ++i)
        {
            if (e->weapons[i].ammo || i == 0)
            {
                new_slot = i;
                break;
            }
        }
        entity_change_weapon_slot(e, new_slot);
    }

#ifdef DEBUG
    // Debug controls for fast movement across the map
    const f32 god_mode_speed = 2000.0f * DT;
    if (g_state.kb_state[SDL_SCANCODE_H]) e->position.x -= god_mode_speed;
    if (g_state.kb_state[SDL_SCANCODE_J]) e->position.y -= god_mode_speed / 2.0f;
    if (g_state.kb_state[SDL_SCANCODE_K]) e->position.y += god_mode_speed / 2.0f;
    if (g_state.kb_state[SDL_SCANCODE_L]) e->position.x += god_mode_speed;
#endif
}

static void
entity_think_missile(struct tagap_entity *e)
{
    // Fixes gunentity glitches
    if (e->owner && e->with_owner)
    {
        return;
    }

    // Missile velocity
    static const f32 MISSILE_SPEED_MUL = 1.5f;
    e->velo.x = cos(glm_rad(e->aim_angle)) * e->info->think.speed_mod *
        (e->flipped ? -1.0f : 1.0f) * MISSILE_SPEED_MUL;
    e->velo.y = sin(glm_rad(e->aim_angle)) * e->info->think.speed_mod *
        MISSILE_SPEED_MUL;

    // Missile lifespan
    f32 missile_lifespan = e->info->stats[STAT_TEMPMISSILE] / 1000.0f;
    if (missile_lifespan <= 0.0f) missile_lifespan = 2.0f;
    if (e->timer_tempmissile >= missile_lifespan)
    {
        entity_die(e);
    }
    e->timer_tempmissile += DT;
    f32 completion = clamp01(e->timer_tempmissile / missile_lifespan);

    // Expand missile
    if (e->info->stats[STAT_FX_EXPAND])
    {
        f32 new_scale = 1.0f + completion;
        for (u32 i = 0; i < e->info->sprite_count; ++i)
        {
            e->sprites[i]->scale = new_scale * 2.0f;
        }

        // Expand light
        if (e->fx.r_light) e->fx.r_light->scale = new_scale;
    }

    // Fade missile
    if (e->info->stats[STAT_FX_FADE])
    {
        f32 new_alpha = 1.0f - completion;
        for (u32 i = 0; i < e->info->sprite_count; ++i)
        {
            // Modify the renderable opacity
            e->sprites[i]->extra_shading.w = new_alpha;
        }

        // Fade light
        if (e->fx.r_light) e->fx.r_light->light_colour.w = new_alpha;
    }
}

static void
entity_think_item(struct tagap_entity *e)
{
    // Check if we come in proximity to player
    static const f32 ITEM_RADIUS = 32.0f;
    if (glms_vec2_distance2(e->position, g_map->player->position) <
        ITEM_RADIUS * ITEM_RADIUS)
    {
        // Copy the ammunition from item to player's store
        i32 set_slot = -1;
        for (u32 w = 0; w < WEAPON_SLOT_COUNT; ++w)
        {
            if (e->weapons[w].ammo > 0 &&
                g_map->player->weapons[w].ammo == 0)
            {
                // Player doesn't have this weapon; we set their slot to it.
                set_slot = w;
            }

            g_map->player->weapons[w].ammo += e->weapons[w].ammo;
        }

        // Destroy the pickup
        entity_die(e);

        // Set player weapon slot
        if (set_slot > -1)
        {
            entity_change_weapon_slot(g_map->player, set_slot);
        }
    }
}
