#include "pch.h"
#include "tagap.h"
#include "renderer.h"
#include "vulkan_renderer.h"
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

    // For now we just use a regular renderable from the renderer
    g_parts->r = renderer_get_renderable(SHADER_PARTICLE);
    if (!g_parts->r)
    {
        return;
    }
    g_parts->r->flags |= RENDERABLE_NO_CULL_BIT;

    // Set textures to -1 by default to make lookup work
    for (u32 i = 0; i < _PARTICLE_COUNT; ++i)
    {
        g_parts->tex_indices[i] = -1;
    }

    // Generate vertex buffer
    size_t vertices_size = MAX_PARTICLES * sizeof(struct vertex_ptl);
    vb_new_empty(&g_parts->r->vb, vertices_size);

    // Generate index buffer
    static const size_t INDEX_COUNT = MAX_PARTICLES * 3 * 2;
    static const size_t INDICES_SIZE = INDEX_COUNT * sizeof(IB_TYPE);
    IB_TYPE *indices = malloc(INDICES_SIZE);
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
    ib_new(&g_parts->r->ib, indices, INDICES_SIZE);
    free(indices);

    // Set the index count to zero so we don't draw the (initially empty)
    // vertex buffer.
    g_parts->r->ib.index_count = 0;

    // Create staging buffer
    if (vulkan_create_buffer(
        (VkDeviceSize)vertices_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &g_parts->staging_buf, &g_parts->staging_buf_alloc) < 0)
    {
        LOG_ERROR("[particle] failed to create staging buffer");
        return;
    }

    // Allocate command buffer for staging buffer-->vertex buffer copies on
    // particle updates.  This should fix the massive particle slowdowns (500+
    // FPS drops) we were having.
    const VkCommandBufferAllocateInfo cmdbuf_alloc_info =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = g_vulkan->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    if (vkAllocateCommandBuffers(g_vulkan->d, &cmdbuf_alloc_info,
        &g_parts->cmdbuf) != VK_SUCCESS)
    {
        LOG_ERROR("[particle] failed to allocate command buffer");
        return;
    }

    static const VkFenceCreateInfo fence_info =
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    if (vkCreateFence(g_vulkan->d, &fence_info, NULL,
        &g_parts->fence) != VK_SUCCESS)
    {
        LOG_ERROR("[particle] failed to create buffer-copy fence!");
        return;
    }

    vmaMapMemory(g_vulkan->vma,
        g_parts->staging_buf_alloc, (void **)&g_parts->quad_buffer);
}

void
particles_deinit(void)
{
    vmaUnmapMemory(g_vulkan->vma, g_parts->staging_buf_alloc);
    vkDestroyFence(g_vulkan->d, g_parts->fence, NULL);
    vmaDestroyBuffer(g_vulkan->vma,
        g_parts->staging_buf, g_parts->staging_buf_alloc);
    free(g_parts->pool);
    free(g_parts);
}

void
particles_update(void)
{
    /* Begin render batch */
    g_parts->quad_ptr = (struct quad_ptl *)g_parts->quad_buffer;
    g_parts->r->ib.index_count = 0;

    /*
     * Update particles, this will create writes to the mapped staging buffer
     * memory
     */
    for (u32 i = 0; i < MAX_PARTICLES; ++i)
    {
        struct particle *p = &g_parts->pool[i];

        // Don't update inactive particles
        if (!p->active) continue;

        // Particle dies
        if (p->life_remain <= 0.0f)
        {
            p->active = false;
            continue;
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
        p->props.opacity.now =
            lerpf(p->props.opacity.begin, p->props.opacity.end, life_norm);

        u32 tex_index = particle_lookup_tex_index(p->props.type);

        // Add the quad to the buffer
        f32 xflip = (f32)p->props.flip_x * -2.0f + 1.0f;
        struct vertex_ptl vertices[4] =
        {
            {
                {
                    (p->props.pivot_bias.x * 0.5f - 0.5f) * xflip,
                    (p->props.pivot_bias.y * 0.5f - 0.5f)
                },
                p->props.opacity.now, tex_index
            },
            {
                {
                    (p->props.pivot_bias.x * 0.5f - 0.5f) * xflip,
                    (p->props.pivot_bias.y * 0.5f + 0.5f)
                },
                p->props.opacity.now, tex_index
            },
            {
                {
                    (p->props.pivot_bias.x * 0.5f + 0.5f) * xflip,
                    (p->props.pivot_bias.y * 0.5f + 0.5f)
                },
                p->props.opacity.now, tex_index
            },
            {
                {
                    (p->props.pivot_bias.x * 0.5f + 0.5f) * xflip,
                    (p->props.pivot_bias.y * 0.5f - 0.5f)
                },
                p->props.opacity.now, tex_index
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
        }
        // Note we must write sequentially here as we declared
        // VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        memcpy(g_parts->quad_ptr++, vertices, sizeof(struct quad_ptl));
        g_parts->r->ib.index_count += 6;
    }

    /*
     * End batch: unmap staging buffer, copy the data into the vertex buffer
     */
    if ((struct vertex_ptl *)g_parts->quad_ptr == g_parts->quad_buffer)
    {
        return;
    }
    if (vulkan_copy_buffer_using_cmdbuffer(
        g_parts->cmdbuf,
        g_parts->fence,
        g_parts->staging_buf,
        g_parts->r->vb.vk_buffer,
        (size_t)(
            (struct vertex_ptl *)g_parts->quad_ptr -
            g_parts->quad_buffer) * sizeof(struct vertex_ptl)) < 0)
    {
        LOG_ERROR("[vbuffer] failed to copy staging buffer to vertex buffer");
        g_parts->r->ib.index_count = 0;
    }
    // The buffer is now ready to be rendered
}

void
particle_emit(struct particle_props *props)
{
    struct particle *p = &g_parts->pool[g_parts->index];
    //LOG_DBUG("[particle] emitting particle %d", g_parts->index);

    p->active = true;
    p->props = *props;
    p->life_remain = props->lifetime;

    // Adjust index, just wrap back around when we run out
    --g_parts->index;
    g_parts->index %= MAX_PARTICLES;
}
