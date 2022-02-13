#include "pch.h"
#include "tagap.h"
#include "tagap_entity.h"
#include "tagap_sprite.h"
#include "renderer.h"
#include "vulkan_renderer.h"
#include "state_level.h"

static inline f32
smooth_in(f32 v, f32 target)
{
    const f32 INPUT_GRAVITY = 5.0f * DT;

    if (v < target)
    {
        v += INPUT_GRAVITY;
        v = clamp(v, -1.0f, target);
    }
    else if (v > target)
    {
        v -= INPUT_GRAVITY;
        v = clamp(v, target, 1.0f);
    }
    else
    {
        v = target;
    }
    return clamp(v, -1.0f, 1.0f);
}

struct collision_result
{
    bool above;
    bool left;
    bool right;
    bool below;
    f32 floor_gradient, floor_shift;
};

static void check_collision(struct tagap_entity *, struct collision_result *);

/*
 * Spawn an entity into the game, and add all sprites, stats, etc.
 */
void
entity_spawn(struct tagap_entity *e)
{
    e->active = true;

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
        e->sprites[s] = renderer_get_renderable();
        struct renderable *r = e->sprites[s];

        // Create quad for the sprite
        static const f32 w = 0.5f, h = 0.5f;
        static const struct vertex vertices[4] =
        {
            // Top left
            {
                .pos      = (vec2s) { -w, h },
                .texcoord = (vec2s) { 0.0f, 0.0f, },
            },
            // Top right
            {
                .pos      = (vec2s) { w, h },
                .texcoord = (vec2s) { 1.0f, 0.0f, },
            },
            // Bottom right
            {
                .pos      = (vec2s) { w, -h },
                .texcoord = (vec2s) { 1.0f, 1.0f, },
            },
            // Bottom left
            {
                .pos      = (vec2s) { -w, -h },
                .texcoord = (vec2s) { 0.0f, 1.0f, },
            },
        };
        static const u16 indices[3 * 4] =
        {
            0, 1, 2,
            0, 2, 3
        };

        // Copy vertex/index data into sprite renderable
        vb_new(&r->vb, vertices, 4 * sizeof(struct vertex));
        ib_new(&r->ib, indices, 3 * 4 * sizeof(u16));
        r->tex = spr->info->frames[0].tex;
        r->pos = e->position;
        r->offset = spr->offset;
        r->pos.y *= -1.0f;
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

    if (e->info->think.mode == THINK_AI_USER)
    {
        e->info->stats[STAT_S_WEAPON] = 2;
    }

    // Spawn the gun entity
    struct tagap_entity_info *missile_info =
        g_state.l.weapons[e->info->stats[STAT_S_WEAPON]].primary;
    if (missile_info->gun_entity)
    {
        // TODO move to the level state file
        // TODO bounds check
        if (g_map->entity_count + 1 >= LEVEL_MAX_ENTITIES)
        {
            LOG_ERROR("[tagap_entity] level entity limit (%d) exceeded",
                LEVEL_MAX_ENTITIES);
            return;
        }
        struct tagap_entity *gunent = &g_map->entities[g_map->entity_count++];
        memset(gunent, 0, sizeof(struct tagap_entity));
        gunent->info = missile_info->gun_entity;
        gunent->position = e->position;
        gunent->aim_angle = e->aim_angle;
        gunent->flipped = e->flipped;
        gunent->owner = e;
        gunent->with_owner = true;

        // Don't need to call entity_spawn as we are in a loop which calls it
        // for us (in state_level)
    }
}

static void
entity_reset(
    struct tagap_entity *e,
    vec2s pos,
    f32 aim,
    bool flipped)
{
    e->active = true;
    e->position = pos;
    e->aim_angle = aim;
    e->flipped = flipped;
    for (u32 i = 0; i < e->info->sprite_count; ++i)
    {
        e->sprites[i]->hidden = false;
    }

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

    // Check if we have any pooled entities which are the correct type
    struct tagap_entity *missile_e = NULL;
    for (u32 i = 0; i < owner->pool_count; ++i)
    {
        if (owner->pool[i].mark == owner->info->stats[STAT_S_WEAPON] &&
            !owner->pool[i].e->active)
        {
            missile_e = owner->pool[i].e;
            entity_reset(missile_e,
                spawn_pos,
                owner->aim_angle,
                owner->flipped);
            break;
        }
    }
    if (!missile_e)
    {
        // Nothing in the pool that matches, so just spawn an new entity
        missile_e = level_spawn_entity(missile,
            spawn_pos,
            owner->aim_angle,
            owner->flipped);
        if (missile_e)
        {
            missile_e->owner = owner;
        }
    }
}

void
entity_die(struct tagap_entity *e)
{
    // Die effects (e.g. explosion, etc.)
    // ...

    // Move the entity to the missile pool and mark it with owner's weapon slot
    e->active = false;
    for (u32 i = 0; i < e->info->sprite_count; ++i)
    {
        e->sprites[i]->hidden = true;
    }
    if (!e->pooled && e->owner)
    {
        // Add to owner pool
        e->owner->pool[e->owner->pool_count++] = (struct pooled_entity)
        {
            // This is a bit dodgey, but it works for now I guess.  We use the
            // weapon slot as the marker
            .mark = e->owner->info->stats[STAT_S_WEAPON],
            .e = e,
        };
    }
}

void
entity_update(struct tagap_entity *e)
{
    switch(e->info->think.mode)
    {
    // Entity is controlled by the player
    case THINK_AI_USER:
    {
        // Calculate world point of cursor
        vec2s cursor_world = { g_state.cam_pos.x, g_state.cam_pos.y };
        cursor_world.x += (f32)g_state.mouse_x;
        cursor_world.y += (f32)g_state.mouse_y;

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
    } break;

    case THINK_AI_MISSILE:
    {
        // Fixes gunentity glitches
        if (e->owner && e->with_owner)
        {
            break;
        }

        // Missile velocity
        e->velo.x = cos(glm_rad(e->aim_angle)) * e->info->think.speed_mod *
            (e->flipped ? -1.0f : 1.0f);
        e->velo.y = sin(glm_rad(e->aim_angle)) * e->info->think.speed_mod;

        // Missile lifespan
        f32 missile_lifespan = e->info->stats[STAT_TEMPMISSILE] / 1000.0f;
        if (missile_lifespan <= 0.0f) missile_lifespan = 2.0f;
        if (e->timer_tempmissile >= missile_lifespan)
        {
            entity_die(e);
        }
        e->timer_tempmissile += DT;
    } break;

    // Static entity
    default:
    case THINK_NONE: break;
    }

    // Weapons
    struct tagap_entity_info *missile_info =
        g_state.l.weapons[e->info->stats[STAT_S_WEAPON]].primary;
    if (e->inputs.fire && e->attack_timer > missile_info->think.attack_delay)
    {
        entity_spawn_missile(e, missile_info);
        e->attack_timer = 0.0f;
    }
    e->attack_timer += DT;

    // Collision info
    struct collision_result collision;
    memset(&collision, 0, sizeof(struct collision_result));

    // Move the entity
    switch(e->info->move.type)
    {
    // Static entity
    case MOVETYPE_NONE:
    default:
    {
        if (!glms_vec2_eq(e->velo, 0.0f))
        {
            check_collision(e, &collision);

            // Just apply velocity if we have any
            e->position.x += e->velo.x * DT * 60.0f;
            e->position.y += e->velo.y * DT * 60.0f;
        }
    } break;

    // Entity is affected by gravity (i.e. walks the earth)
    case MOVETYPE_WALK:
    {
        check_collision(e, &collision);

        // Jumping
        static const f32 MAX_JUMP_TIME = 0.30f;
        if (e->inputs.vert > 0.0f)
        {
            if (e->jump_timer < 0.0f && collision.below && e->jump_reset)
            {
                // Reset timer to initiate jump
                e->jump_timer = 0.0f;
                e->jump_reset = false;
            }
            else if (e->jump_timer > MAX_JUMP_TIME)
            {
                // Keep jumping until limit
                e->jump_timer = -1.0f;
            }
        }
        else
        {
            // Stop timer so that gravity kicks in
            e->jump_timer = -1.0f;

            if (collision.below)
            {
                e->jump_reset = true;
            }
        }
        if (e->jump_timer >= 0.0f)
        {
            e->jump_timer += DT;
        }

        // Apply gravity
        if (e->jump_timer < 0.0f)
        {
            static const f32 GRAVITY_DEFAULT = -3.2f;
            // Not jumping; normal gravity
            if (e->velo.y > GRAVITY_DEFAULT)
            {
                e->velo.y += 10.0f * GRAVITY_DEFAULT * DT;
            }
            else
            {
                e->velo.y = GRAVITY_DEFAULT;
            }
            e->velo.y *= (f32)!collision.below;
        }
        else
        {
            static const f32 JUMP_SPEED = 8.0f;
            e->velo.y = JUMP_SPEED * (f32)!collision.above;
            if (collision.above)
            {
                // Stop jumping if we hit ceiling
                e->velo.y = 0.0f;
                e->jump_timer = -1.0f;
            }
        }

        // Keeps horizontal input during jump
        if (!(!collision.below && e->inputs.horiz == 0.0f))
        {
            // Calculate smooth horizontal input
            e->inputs.horiz_smooth =
                smooth_in(e->inputs.horiz_smooth, e->inputs.horiz);
        }

        // Move faster when walking in direction of facing
        static const f32 FORWARD_SPEED = 1.4f;
        f32 facing_mul = lerpf(1.0f,
            FORWARD_SPEED,
            ((f32)!e->flipped) == sign(e->inputs.horiz / 2.0f + 0.5f));

        e->velo.x = e->inputs.horiz_smooth * e->info->move.speed * facing_mul;

        if (collision.below && e->jump_timer < 0.0f)
        {
            // We are on a floor, so follow the floor's path
            e->position.x += e->velo.x * DT * 60.0f;
            e->position.y =
                collision.floor_gradient * e->position.x +
                collision.floor_shift + e->info->offsets[OFFSET_SIZE].x;
        }
        else
        {
            // Standard left/right movement
            e->position.x += e->velo.x * DT * 60.0f;
            e->position.y += e->velo.y * DT * 60.0f;
        }

        if (collision.below)
        {
            e->bobbing_timer += e->velo.x * DT * 20.0f;
            e->bobbing_timer = fmodf(e->bobbing_timer, 2.0f * GLM_PI);
        }
        else
        {
            // This is a bit tricky; this is to move the player's leg 'forward'
            // when they are in mid-air.  The fmodf in the above block clamps
            // the angle from 0 to 2pi radians.  We need the angle to be at
            // pi/2 rad (90 deg) for the desired effect, and adjust the bobbing
            // timer as such to get there
            if (e->bobbing_timer > GLM_PI_2)
            {
                if (e->bobbing_timer > GLM_PI)
                {
                    // Send bob timer forwards to get to 90 degrees
                    e->bobbing_timer = clamp(
                        e->bobbing_timer + DT * 10.0f,
                        GLM_PI_2,
                        GLM_PI_2 + GLM_PI * 2.0f);
                }
                else
                {
                    // Send bob timer backwards to get to 90 degrees
                    e->bobbing_timer = clamp(
                        e->bobbing_timer - DT * 10.0f,
                        GLM_PI_2,
                        GLM_PI * 2.0f);
                }
            }
            else if (e->bobbing_timer < GLM_PI_2)
            {
                // Send bob timer forwards to get to 90 degrees
                e->bobbing_timer = clamp(
                    e->bobbing_timer + DT * 10.0f,
                    0.0f,
                    GLM_PI_2);
            }
            else
            {
                e->bobbing_timer = GLM_PI_2;
            }
        }
    } break;

    // Entity is "floating"
    // Not properly tested yet
    case MOVETYPE_FLY:
    {
        check_collision(e, &collision);

        e->velo.x = smooth_in(e->velo.x, e->inputs.horiz);
        e->velo.y = smooth_in(e->velo.y, e->inputs.vert);

        e->position.x += e->velo.x * DT * 60.0f * e->info->move.speed;
        e->position.y += e->velo.y * DT * 60.0f * e->info->move.speed;

        e->bobbing_timer += DT;
    } break;
    }

    // Post-movement/collision think stuff
    switch(e->info->think.mode)
    {
    case THINK_AI_USER:
    {
        // This is the player; update camera position
        g_state.cam_pos = (vec3s)
        {{
             e->position.x - (f32)WIDTH / 2,
             -e->position.y - (f32)HEIGHT / 2 - 100.0f,
             0.0f
        }};
    } break;

    case THINK_AI_MISSILE:
    {
        // If the missile collides with something then destroy it
        // TODO: check AI_BLOW
        if (collision.above ||
            collision.left ||
            collision.right ||
            collision.below)
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

                // Set angle to the linedef tangent
                sprite_rot_offset = glm_deg(atan(collision.floor_gradient));
                if (e->flipped)
                {
                    sprite_rot_offset *= -1.0f;
                }
            }
        }

        // By default use entity position and normal rotation
        spr_r->pos = glms_vec2_add(e->position, sprite_offset);
        spr_r->pos.y *= -1.0f;
        spr_r->rot = entity_get_rot(e) + sprite_rot_offset;

        // Flip sprites based on entity facing
        spr_r->flipped = e->flipped;

        // Update sprite positions
        switch(spr->anim)
        {
        case ANIM_WEAPON2:
        {
            // Hide second weapon sprite if entity does not have akimbo
            spr_r->hidden = !e->info->stats[STAT_S_AKIMBO];
        }
        // ... falls through
        case ANIM_WEAPON:
        {
            spr_r->rot = e->aim_angle;

            if (!e->owner)
            {
                // Use akimbo texture frame on uzi (slot 0) if we have it
                u32 weapon_slot = e->info->stats[STAT_S_WEAPON];
                u32 tex_slot = weapon_slot + 2;
                if (weapon_slot == 0 &&
                    e->info->stats[STAT_S_AKIMBO])
                {
                    tex_slot = 0;
                }
                if (tex_slot < spr->info->frame_count)
                {
                    spr_r->tex = spr->info->frames[tex_slot].tex;
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
            if (spr_r->rot  < 90.0f)
            {
                spr_r->rot = 30.0f * (e->aim_angle / 90.0f);
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

static void
check_collision(
    struct tagap_entity *e,
    struct collision_result *c)
{
    // For all moving entities:
    // Check collision with all linedefs in the level
    for (u32 i = 0; i < g_map->linedef_count; ++i)
    {
        struct tagap_linedef *l = &g_map->linedefs[i];

        vec2s leftmost;
        vec2s rightmost;
        if (l->start.x < l->end.x)
        {
            leftmost = l->start;
            rightmost = l->end;
        }
        else
        {
            leftmost = l->end;
            rightmost = l->start;
        }

        // Vertical line (wall) checks
        // TODO

        // Horizontal line checks
        // Check that our position is in the range of the line
        if (e->position.x >= leftmost.x &&
            e->position.x <= rightmost.x)
        {
            // Get Y-position of line at the entity's point
            f32 gradient = (rightmost.y - leftmost.y) /
                (rightmost.x - leftmost.x);
            f32 shift = leftmost.y - gradient * leftmost.x;
            f32 y_line = gradient * e->position.x + shift;

            /*
             * Floor collision check
             */
            // If we collide with floor
            if ((l->style == LINEDEF_STYLE_FLOOR ||
                    l->style == LINEDEF_STYLE_PLATE_FLOOR) &&
                !c->below &&
                e->velo.y <= 0.0f &&
                e->position.y - e->info->offsets[OFFSET_SIZE].x <= y_line &&
                e->position.y >= y_line)
            {
                c->below = true;
                c->floor_gradient = gradient;
                c->floor_shift = shift;
            }
        }
    }
}
