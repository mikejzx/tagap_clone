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
    const f32 INPUT_GRAVITY = 0.06f;

    // TODO: framerate-independant velocity
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
    e->velo = (vec2s)GLMS_VEC2_ZERO_INIT;
    e->inputs.horiz = e->inputs.vert = 0.0f;
    e->inputs.horiz_smooth = e->inputs.vert_smooth = 0.0f;
    e->bobbing_timer = 0.0f;
    e->next_blink = 0;

    // Create sprites
    for (u32 s = 0; s < e->info->sprite_info_count; ++s)
    {
        struct tagap_sprite *spr = &e->sprites[s];
        struct sprite_info *sprinfo = &e->info->sprite_infos[s];

        // TODO: reuse sprites

        // Count sprite frame textures
        char sprpath[128];
        spr->frame_count = -1;
        do
        {
            ++spr->frame_count;
            sprintf(sprpath, "%s/%s_%02d.tga",
                TAGAP_SPRITES_DIR,
                sprinfo->name,
                spr->frame_count);
        } while(access(sprpath, F_OK) == 0);

        // Allocate frames
        spr->frames = calloc(spr->frame_count,
            sizeof(struct tagap_sprite_frame));

        // Load each of the individual frames textures
        for (u32 f = 0; f < spr->frame_count; ++f)
        {
            sprintf(sprpath, "%s/%s_%02d.tga",
                TAGAP_SPRITES_DIR,
                sprinfo->name,
                f);
            spr->frames[f].tex_index = vulkan_texture_load(sprpath);
            if (spr->frames[f].tex_index < 0)
            {
                LOG_WARN("[entity] sprite texture '%s' couldn't be loaded",
                    sprpath);
                spr->frames[f].tex_index = 0;
            }
        }

        // Create the sprite renderable
        spr->r = renderer_get_renderable();

        // Create quad for the sprite
        f32 w = (f32)g_vulkan->textures[spr->frames[0].tex_index].w / 2.0f,
            h = (f32)g_vulkan->textures[spr->frames[0].tex_index].h / 2.0f;
        const struct vertex vertices[4] =
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
        const u16 indices[3 * 4] =
        {
            0, 1, 2,
            0, 2, 3
        };

        vb_new(&spr->r->vb, vertices, 4 * sizeof(struct vertex));
        ib_new(&spr->r->ib, indices, 3 * 4 * sizeof(u16));
        spr->r->tex = spr->frames[0].tex_index;
        spr->r->pos = e->position;
        spr->r->offset = sprinfo->offset;
        spr->r->pos.y *= -1.0f;
        if (e->info->think.mode == THINK_AI_AIM)
        {
            spr->r->rot = e->aim_angle;
            spr->r->flipped = false;
        }
        else
        {
            spr->r->rot = 0;
            spr->r->flipped = !!e->facing;
        }
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
        const u8 *keystate = SDL_GetKeyboardState(NULL);

        if (keystate[SDL_SCANCODE_A]) e->inputs.horiz = -1.0f;
        else if (keystate[SDL_SCANCODE_D]) e->inputs.horiz = 1.0f;
        else e->inputs.horiz = 0.0f;

        if (keystate[SDL_SCANCODE_S]) e->inputs.vert = -1.0f;
        else if (keystate[SDL_SCANCODE_W]) e->inputs.vert = 1.0f;
        else e->inputs.vert = 0.0f;
    } break;

    // Static entity
    default:
    case THINK_NONE: break;
    }

    // Collision info
    struct collision_result collision;
    memset(&collision, 0, sizeof(struct collision_result));

    // Move the entity
    switch(e->info->move.type)
    {
    // Static entity
    case MOVETYPE_NONE:
    default:
        break;

    // Entity is affected by gravity (i.e. walks the earth)
    case MOVETYPE_WALK:
    {
        check_collision(e, &collision);

        // Calculate smooth horizontal input
        e->inputs.horiz_smooth =
            smooth_in(e->inputs.horiz_smooth, e->inputs.horiz);

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
            e->jump_reset = true;
        }
        if (e->jump_timer >= 0.0f)
        {
            e->jump_timer += 1.0f / 60.0f;
        }

        // Apply gravity
        if (e->jump_timer < 0.0f)
        {
            static const f32 GRAVITY_DEFAULT = -2.8f;
            // Not jumping; normal gravity
            if (e->velo.y > GRAVITY_DEFAULT)
            {
                e->velo.y += 10.0f * GRAVITY_DEFAULT * 1.0f / 60.0f;
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

        // Move faster when walking in direction of facing
        static const f32 FORWARD_SPEED = 1.4f;
        f32 facing_mul = lerpf(1.0f,
            FORWARD_SPEED,
            ((f32)!e->flipped) == sign(e->inputs.horiz / 2.0f + 0.5f));

        e->velo.x = e->inputs.horiz_smooth * e->info->move.speed * facing_mul;

        if (collision.below && e->jump_timer < 0.0f)
        {
            // We are on a floor, so follow the floor's path
            e->position.x += e->velo.x;
            e->position.y =
                collision.floor_gradient * e->position.x +
                collision.floor_shift + e->info->colsize.x;
        }
        else
        {
            // Standard left/right movement
            e->position.x += e->velo.x;
            e->position.y += e->velo.y;
        }

        e->bobbing_timer += e->velo.x * (f32)collision.below;
    } break;

    // Entity is "floating"
    // Not properly tested yet
    case MOVETYPE_FLY:
    {
        check_collision(e, &collision);

        e->velo.x = smooth_in(e->velo.x, e->inputs.horiz);
        e->velo.y = smooth_in(e->velo.y, e->inputs.vert);

        e->position.x += e->velo.x * e->info->move.speed;
        e->position.y += e->velo.y * e->info->move.speed;

        e->bobbing_timer += 1.0f;
    } break;
    }

    if (e->info->think.mode == THINK_AI_USER)
    {
        // This is the player; update camera position
        g_state.cam_pos = (vec3s)
        {{
             e->position.x - (f32)WIDTH / 2,
             -e->position.y - (f32)HEIGHT / 2 - 100.0f,
             0.0f
        }};
    }

    // Update sprites
    for (u32 s = 0; s < e->info->sprite_info_count; ++s)
    {
        struct tagap_sprite *spr = &e->sprites[s];
        struct sprite_info *sprinfo = &e->info->sprite_infos[s];

        // SPRITEVAR animations/bobbing effects
        vec2s sprite_offset = (vec2s)GLMS_VEC2_ZERO_INIT;
        f32 sprite_rot_offset = 0.0f;
        f32 bob_sin = sinf(e->bobbing_timer / 3.0f);
        if (sprinfo->vars[SPRITEVAR_BOB].value && e->velo.x != 0.0f)
        {
            // Bobbing animation
            f32 bob_var = (f32)sprinfo->vars[SPRITEVAR_BOB].value;
            sprite_offset.y =
                e->velo.x *
                bob_sin * bob_var / 25.0f;
        }
        else if (sprinfo->vars[SPRITEVAR_BIAS].value)
        {
            if (e->velo.x != 0.0f)
            {
                // Set to the second frame
                if (spr->frame_count > 0)
                {
                    // Not sure if this is a good idea; however something like
                    // this is needed for leg animations to work properly.
                    spr->r->tex = spr->frames[1].tex_index;
                }

                // Apply rotation offset
                sprite_rot_offset = bob_sin *
                    (f32)sprinfo->vars[SPRITEVAR_BIAS].value / 2.0f;
            }
            else
            {
                // Set legs to first frame
                spr->r->tex = spr->frames[0].tex_index;

                // Set angle to the linedef tangent
                sprite_rot_offset = glm_deg(atan(collision.floor_gradient));
                if (e->flipped)
                {
                    sprite_rot_offset *= -1.0f;
                }
            }
        }

        // By default use entity position and normal rotation
        spr->r->pos = glms_vec2_add(e->position, sprite_offset);
        spr->r->pos.y *= -1.0f;
        spr->r->rot = entity_get_rot(e) + sprite_rot_offset;

        // Flip sprites based on entity facing
        spr->r->flipped = e->flipped;

        // Update sprite positions
        switch(sprinfo->anim)
        {
        case ANIM_WEAPON:
        case ANIM_WEAPON2:
        {
            spr->r->rot = e->aim_angle;
        } break;

        // Head animation
        case ANIM_FACE:
        {
            if (spr->r->rot  < 90.0f)
            {
                spr->r->rot = 30.0f * (e->aim_angle / 90.0f);
            }

            // Blink animation
            if (SDL_GetTicks() > e->next_blink)
            {
                e->next_blink = SDL_GetTicks() + (rand() % 3500) + 500;
                e->blinked_frames = 0;
                spr->r->tex = spr->frames[1].tex_index;
            }
            else if (e->blinked_frames > 8)
            {
                spr->r->tex = spr->frames[0].tex_index;
            }
            ++e->blinked_frames;
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
    for (u32 s = 0; s < e->info->sprite_info_count; ++s)
    {
        struct tagap_sprite *spr = &e->sprites[s];
        free(spr->frames);
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
            if (!c->below &&
                e->position.y - e->info->colsize.x <= y_line &&
                e->position.y >= y_line)
            {
                c->below = true;
                c->floor_gradient = gradient;
                c->floor_shift = shift;
            }
        }
    }
}
