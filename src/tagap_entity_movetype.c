#include "pch.h"
#include "tagap.h"
#include "tagap_entity.h"
#include "tagap_entity_movetype.h"

static f32 smooth_in(f32, f32);
static void entity_movetype_walk(struct tagap_entity *);
static void entity_movetype_float(struct tagap_entity *);

void entity_movetype(struct tagap_entity *e)
{
    // Move the entity
    switch(e->info->move.type)
    {
    // Static entity
    case MOVETYPE_NONE:
    default:
    {
        if (!glms_vec2_eq(e->velo, 0.0f))
        {
            collision_check(e, &e->collision);

            // Just apply velocity if we have any
            e->position.x += e->velo.x * DT * 60.0f;
            e->position.y += e->velo.y * DT * 60.0f;
        }
    } break;

    // Entity is affected by gravity (i.e. walks the earth)
    case MOVETYPE_WALK:
    {
        collision_check(e, &e->collision);
        entity_movetype_walk(e);
    } break;

    // Entity is "floating"
    // Not properly tested yet
    case MOVETYPE_FLY:
    {
        collision_check(e, &e->collision);
        entity_movetype_float(e);
    } break;
    }
}

static void
entity_movetype_walk(struct tagap_entity *e)
{
    // Jumping
    static const f32 MAX_JUMP_TIME = 0.30f;
    if (e->inputs.vert > 0.0f && !e->collision.above)
    {
        if (e->jump_timer < 0.0f &&
            e->collision.below &&
            e->jump_reset)
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

        if (e->collision.below ||
            e->collision.above)
        {
            e->jump_reset = true;
        }
    }
    if (e->jump_timer >= 0.0f) e->jump_timer += DT;

    // Belly sliding (not properly implemented)
    static const f32 MAX_SLIDE_TIME = 1.2f, MIN_SLIDE_TIME = 1.2f;
#if SLIDE_ENABLED
    if (e->inputs.vert < 0.0f)
    {
        if ((e->slide_timer < 0.0f || e->slide_timer > 0.0f) &&
            e->collision.below)
        {
            // Reset timer to initiate slide
            e->slide_timer = 0.0f;
        }
        else if (e->slide_timer > MAX_SLIDE_TIME)
        {
            // Keep sliding until limit
            e->slide_timer = -1.0f;
        }
    }
    else if (e->slide_timer > MIN_SLIDE_TIME)
    {
        // Stop timer
        if (e->collision.below)
        {
            e->slide_timer = -1.0f;
        }
    }
    if (e->aim_angle > 90.0f)
    {
        e->slide_timer = -1.0f;
    }
#endif

    // Apply gravity
    if (e->jump_timer < 0.0f || e->collision.above)
    {
        static const f32 GRAVITY_DEFAULT = -3.2f;
        // Not jumping; normal gravity
        if (e->velo.y > GRAVITY_DEFAULT && !e->collision.above)
        {
            e->velo.y += 10.0f * GRAVITY_DEFAULT * DT;
        }
        else
        {
            e->velo.y = GRAVITY_DEFAULT;
        }
        e->velo.y *= (f32)!e->collision.below;
    }
    else
    {
        static const f32 JUMP_SPEED = 8.0f;
        e->velo.y = JUMP_SPEED;
    }

    // Keeps horizontal input during jump
    if (!(!e->collision.below && e->inputs.horiz == 0.0f))
    {
        // Calculate smooth horizontal input
        e->inputs.horiz_smooth =
            smooth_in(e->inputs.horiz_smooth, e->inputs.horiz);
    }

    // Move faster when walking in direction of facing
    static const f32 FORWARD_SPEED = 1.4f,
                     BACKWARD_SPEED = 1.2f,
                     SLIDE_SPEED = 3.2f;
    f32 facing_mul = lerpf(BACKWARD_SPEED,
        FORWARD_SPEED,
        ((f32)!e->flipped) == sign(e->inputs.horiz / 2.0f + 0.5f));

    // Stop input when colliding with walls
    f32 col_x_mul = 1.0f;
    if (e->collision.left && e->inputs.horiz_smooth <= 0.0f)
    {
        col_x_mul = 0.0f;
    }
    else if (e->collision.right && e->inputs.horiz_smooth >= 0.0f)
    {
        col_x_mul = 0.0f;
    }

    if (e->slide_timer < 0.0f)
    {
        // Normal walking movement
        e->velo.x = e->inputs.horiz_smooth * e->info->move.speed * facing_mul *
            col_x_mul;
    }
    else
    {
        // Epic bellyslide
        //LOG_DBUG("BELLYSLIDE");
        e->velo.x = e->info->move.speed * SLIDE_SPEED *
            ((1.0f - clamp01(e->slide_timer / MAX_SLIDE_TIME)) * 0.5f + 0.5f) *
            ((f32)e->flipped * -2.0f + 1.0f) * col_x_mul;

        e->slide_timer += DT;
    }

    if (e->collision.below &&
        e->jump_timer < 0.0f)
    {
        // We are on a floor, so follow the floor's path
        e->position.x += e->velo.x * DT * 60.0f;
        e->position.y =
            e->collision.floor_gradient * e->position.x +
            e->collision.floor_shift + e->info->offsets[OFFSET_SIZE].x;
    }
    else
    {
        // Standard left/right movement
        e->position.x += e->velo.x * DT * 60.0f;
        e->position.y += e->velo.y * DT * 60.0f;
    }

    if (e->collision.below)
    {
        e->bobbing_timer += e->velo.x * DT * 20.0f * (f32)(e->slide_timer < 0.0f);
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
}

static void
entity_movetype_float(struct tagap_entity *e)
{
    e->velo.x = smooth_in(e->velo.x, e->inputs.horiz);
    e->velo.y = smooth_in(e->velo.y, e->inputs.vert);

    e->position.x += e->velo.x * DT * 60.0f * e->info->move.speed;
    e->position.y += e->velo.y * DT * 60.0f * e->info->move.speed;

    e->bobbing_timer += DT;
}

static f32
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
