#include "pch.h"
#include "tagap.h"
#include "tagap_entity.h"
#include "tagap_sprite.h"
#include "renderer.h"
#include "vulkan_renderer.h"

/*
 * Spawn an entity into the game, and add all sprites, stats, etc.
 */
void
entity_spawn(struct tagap_entity *e)
{
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
        spr->r->pos = glms_vec2_add(e->position, sprinfo->offset);
        spr->r->pos.y *= -1.0f;
        spr->r->rot = e->aim_angle;
    }

    // This is the player; move the camera to here
    if (e->info->think.mode == THINK_AI_USER)
    {
        g_state.cam_pos = (vec3s)
        {{
             e->position.x - (f32)WIDTH / 2.5f,
             e->position.y - (f32)HEIGHT / 4.0f,
             0.0f
        }};
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
        pos.y += 128.0f + 32.0f;
        f32 diff_x = cursor_world.x - pos.x;
        f32 diff_y = cursor_world.y - pos.y;
        f32 ang = glm_deg(atan(diff_y / diff_x));

        e->aim_angle = -ang;
    } break;

    // Static entity
    default:
    case THINK_NONE: break;
    }

    // Update sprites
    for (u32 s = 0; s < e->info->sprite_info_count; ++s)
    {
        struct tagap_sprite *spr = &e->sprites[s];
        struct sprite_info *sprinfo = &e->info->sprite_infos[s];

        vec2s pos_normal = glms_vec2_add(e->position, sprinfo->offset);
        pos_normal.y *= -1.0f;
        f32 rot_normal = e->aim_angle;

        spr->r->pos = pos_normal;

        // Update sprite positions
        switch(sprinfo->anim)
        {
        case ANIM_WEAPON:
        case ANIM_WEAPON2:
        {
            spr->r->rot = rot_normal;
        } break;
        case ANIM_FACE:
        {
            spr->r->rot = rot_normal;
            if (rot_normal < 90.0f)
            {
                spr->r->rot = 30.0f * (rot_normal / 90.0f);
            }
        } break;
        default:
        case ANIM_NONE:
        {
            spr->r->rot = 0;
        } break;
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
