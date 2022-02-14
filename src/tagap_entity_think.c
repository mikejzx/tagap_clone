#include "pch.h"
#include "tagap.h"
#include "tagap_entity.h"
#include "tagap_entity_think.h"

static void entity_think_user(struct tagap_entity *);
static void entity_think_missile(struct tagap_entity *);

void
entity_think(struct tagap_entity *e)
{
    switch(e->info->think.mode)
    {
    // Entity is controlled by the player
    case THINK_AI_USER: entity_think_user(e); break;

    // Entity is a projectile
    case THINK_AI_MISSILE: entity_think_missile(e); break;

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
}
