#include "pch.h"
#include "tagap.h"
#include "renderer.h"
#include "vulkan_renderer.h"
#include "vulkan_swapchain.h"
#include "particle.h"

struct particle_system *g_parts;

const char *PARTICLE_TEX_NAMES[] =
{
    [PARTICLE_BEAM] = TAGAP_DATA_MOD_DIR "/art/effects/fx_beam.tga",
    [PARTICLE_SMOKE] = TAGAP_EFFECTS_DIR "/particle_smoke.tga",
    [PARTICLE_EXPLOSION] = TAGAP_EFFECTS_DIR "/fx_explosion.tga",
};

static inline i32
particle_lookup_tex_index(enum particle_type part)
{
    if (g_parts->tex_indices[part] != -1)
    {
        // Already loaded
        return g_parts->tex_indices[part];
    }

    // No texture for index
    if (!PARTICLE_TEX_NAMES[part]) return 0;

    g_parts->tex_indices[part] = vulkan_texture_load(PARTICLE_TEX_NAMES[part]);
    return g_parts->tex_indices[part];
}

void
particles_init(void)
{
    g_parts = malloc(sizeof(struct particle_system));
    g_parts->pool = calloc(MAX_PARTICLES, sizeof(struct particle));

    // Render from top down
    g_parts->index = MAX_PARTICLES - 1;

    g_parts->frame_count = g_vulkan->swapchain->image_count;
    g_parts->frames = 
        malloc(sizeof(struct particle_frame) * g_parts->frame_count);

    // Set textures to -1 by default to make lookup work
    for (u32 i = 0; i < _PARTICLE_COUNT; ++i)
    {
        g_parts->tex_indices[i] = -1;
    }

    // Generate index buffer
    g_parts->ib.index_count = 0;
    static const size_t INDEX_COUNT = MAX_PARTICLES * 3 * 2;
    static const size_t INDICES_SIZE = INDEX_COUNT * sizeof(ib_type);
    ib_type *indices = malloc(INDICES_SIZE);
    size_t offset = 0;
    for (u32 i = 0; i < INDEX_COUNT; i += 6)
    {
        indices[i + 0] = 0 + offset;
        indices[i + 1] = 1 + offset;
        indices[i + 2] = 2 + offset;
        indices[i + 3] = 2 + offset;
        indices[i + 4] = 3 + offset;
        indices[i + 5] = 0 + offset;
        offset += 4;
    }
    ib_new(&g_parts->ib, indices, INDICES_SIZE);
    free(indices);

    const size_t vertices_size = MAX_PARTICLES * sizeof(struct quad_ptl);
    for (u32 f = 0; f < g_parts->frame_count; ++f)
    {
        struct particle_frame *frame = &g_parts->frames[f];

        // Generate vertex buffers
        vb_new_empty(&frame->vb, vertices_size, true);

        // Map to the vertex buffer directly
        vmaMapMemory(g_vulkan->vma,
            frame->vb.vma_alloc, (void **)&frame->quad_buffer);
    }
}

void
particles_deinit(void)
{
    for (u32 f = 0; f < g_parts->frame_count; ++f)
    {
        struct particle_frame *frame = &g_parts->frames[f];
        vmaUnmapMemory(g_vulkan->vma, frame->vb.vma_alloc);
        vb_free(&frame->vb);
    }
    ib_free(&g_parts->ib);
    free(g_parts->frames);
    free(g_parts->pool);
    free(g_parts);
}

void
particles_update(void)
{
    // Update all particle states
    for (u32 i = 0; i < MAX_PARTICLES; ++i)
    {
        struct particle *p = &g_parts->pool[i];

        // Don't update inactive particles
        if (!p->active) continue;

        // Set to die next on this frame
        if (p->die_next)
        {
            p->die_next = false;
            p->active = false;
            continue;
        }

        // Dead particle
        if (p->life_remain <= 0.0f)
        {
            p->active = false;
            continue;
        }

        // Check if particle is at it's endpoint yet.  Done with a simple
        // check for when signs of position differences change
        if (p->props.has_precise_endpoint)
        {
            // Adjusted endpoint (to account for pivot)
            vec2s ep = (vec2s)
            {
                p->props.precise_endpoint.x + lerpf(
                    -p->props.size_x.now / 2.0f,
                    p->props.size_x.now / 2.0f,
                    p->props.pivot_bias.x * 0.5f + 0.5f) *
                    cosf(glm_rad(p->props.rot)),
                p->props.precise_endpoint.y + lerpf(
                    -p->props.size_y.now / 2.0f,
                    p->props.size_y.now / 2.0f,
                    p->props.pivot_bias.y * 0.5f + 0.5f) *
                    sinf(glm_rad(p->props.rot)),
            };
            vec2s diff_sgn = (vec2s)
            {
                sign(p->props.pos.x - ep.x),
                sign(p->props.pos.y - ep.y),
            };
            bool eq_sgn_x = diff_sgn.x == p->old_diff_sgn.x,
                 eq_sgn_y = diff_sgn.y == p->old_diff_sgn.y;
            if (p->old_diff_sgn_init &&
                (!eq_sgn_x || !eq_sgn_y))
            {
                p->die_next = true;
                if (!eq_sgn_x)
                {
                    p->props.pos.x = ep.x;
                }
                if (!eq_sgn_y)
                {
                    //p->props.pos.y = ep.y;
                }
            }
            p->old_diff_sgn = diff_sgn;
            p->old_diff_sgn_init = true;
        }

        p->life_remain -= DT;
        f32 life_norm = 1.0f - (p->life_remain / p->props.lifetime);

        // Update position, rotation, etc.
        p->props.pos =
            glms_vec2_add(p->props.pos, glms_vec2_scale(p->props.velo, DT));
        p->props.rot += p->props.rot_speed * DT;

        // Linearly interpolate properties
        p->props.size_x.now =
            lerpf(p->props.size_x.begin, p->props.size_x.end, life_norm);
        if (p->props.independent_sizes)
        {
            p->props.size_y.now =
                lerpf(p->props.size_y.begin, p->props.size_y.end, life_norm);
        }
        else
        {
            p->props.size_y.now = p->props.size_x.now;
        }
        p->props.colour.now = glms_vec4_lerp(
            p->props.colour.begin, p->props.colour.end, life_norm);
    }
}

void
particles_update_frame(u32 frame_index)
{
    struct particle_frame *frame = &g_parts->frames[frame_index];

    /* Begin render batch */
    frame->quad_ptr = (struct quad_ptl *)frame->quad_buffer;
    frame->index_count = 0;

    for (u32 i = 0; i < MAX_PARTICLES; ++i)
    {
        struct particle *p = &g_parts->pool[i];

        // Don't add inactive particles
        if (!p->active) continue;

        f32 xflip = (f32)p->props.flip_x * -2.0f + 1.0f;
        struct vertex_ptl vertices[4] =
        {
            {
                {
                    (p->props.pivot_bias.x * 0.5f - 0.5f) * xflip,
                    (p->props.pivot_bias.y * 0.5f - 0.5f)
                },
                p->props.colour.now,
                p->props.tex_index
            },
            {
                {
                    (p->props.pivot_bias.x * 0.5f - 0.5f) * xflip,
                    (p->props.pivot_bias.y * 0.5f + 0.5f)
                },
                p->props.colour.now,
                p->props.tex_index
            },
            {
                {
                    (p->props.pivot_bias.x * 0.5f + 0.5f) * xflip,
                    (p->props.pivot_bias.y * 0.5f + 0.5f)
                },
                p->props.colour.now,
                p->props.tex_index
            },
            {
                {
                    (p->props.pivot_bias.x * 0.5f + 0.5f) * xflip,
                    (p->props.pivot_bias.y * 0.5f - 0.5f)
                },
                p->props.colour.now,
                p->props.tex_index
            },
        };
        mat4s model = glms_scale(glms_rotate_z(
        glms_translate((mat4s)GLMS_MAT4_IDENTITY_INIT, (vec3s)
        {
            p->props.pos.x,
            p->props.pos.y,
            0.0f,
        }), glm_rad(p->props.rot)), (vec3s)
        {
            p->props.size_x.now,
            p->props.size_y.now,
            0.0f,
        });
        vec4s tmp;
        for (u32 v = 0; v < 4; ++v)
        {
            tmp = (vec4s){ vertices[v].pos.x, vertices[v].pos.y, 0.0f, 1.0f };
            tmp = glms_mat4_mulv(model, tmp);
            vertices[v].pos = (vec2s){ tmp.x, tmp.y };

            if (p->props.vertex_colour_muls)
            {
                vertices[v].colour = glms_vec4_mul(
                    vertices[v].colour, p->props.vertex_colours[v]);
            }
        }
        // Note we must write sequentially here as we declared
        memcpy(frame->quad_ptr++, vertices, sizeof(struct quad_ptl));
        frame->index_count += 6;
    }
}

void
particle_emit(struct particle_props *props)
{
    struct particle *p = &g_parts->pool[g_parts->index];
    //LOG_DBUG("[particle] emitting particle %d", g_parts->index);

    p->active = true;
    p->props = *props;
    p->life_remain = props->lifetime;
    p->props.tex_index = particle_lookup_tex_index(p->props.type);

    // Adjust index, just wrap back around when we run out
    --g_parts->index;
    g_parts->index %= MAX_PARTICLES;
}
