#include "pch.h"
#include "renderer.h"
#include "tagap.h"
#include "tagap_polygon.h"
#include "vulkan_renderer.h"

struct renderer g_renderer;

static struct renderable *get_renderable(void);

int
renderer_init(SDL_Window *winhandle)
{
    LOG_INFO("[renderer] initialising");
    memset(&g_renderer, 0, sizeof(struct renderer));

    if (vulkan_renderer_init(winhandle) < 0) return -1;

    g_renderer.objs = malloc(MAX_OBJECTS * sizeof(struct renderable));

    return 0;
}

void
renderer_deinit(void)
{
    LOG_INFO("[renderer] deinitialising");
    vulkan_renderer_wait_for_idle();
    for (u32 i = 0; i < g_renderer.obj_count; ++i)
    {
        vb_free(&g_renderer.objs[i].vb);
        ib_free(&g_renderer.objs[i].ib);
    }

    vulkan_renderer_deinit();

    free(g_renderer.objs);
}

void
renderer_render(vec3s cam_pos)
{
    // Record command buffers and render
    vulkan_render_frame_pre();
    vulkan_record_command_buffers(
        g_renderer.objs,
        g_renderer.obj_count,
        cam_pos);
    vulkan_render_frame();
}

static struct renderable *
get_renderable(void)
{
    if (g_renderer.obj_count + 1 >= MAX_OBJECTS)
    {
        LOG_ERROR("[renderer] object capacity exceeded!");
        return NULL;
    }
    return &g_renderer.objs[g_renderer.obj_count++];
}

/*
 * Add polygon to the renderer
 */
void
renderer_add_polygon(struct tagap_polygon *p)
{
    // First load texture
    char texpath[256];
    sprintf(texpath, "%s/%s.tga", TAGAP_TEXTURES_DIR, p->tex_name);
    i32 tex_index = vulkan_texture_load(texpath);
    if (tex_index < 0)
    {
        LOG_ERROR("[renderer] can't add polygon; couldn't load texture '%s'",
            texpath);
        return;
    }

    // Get renderable
    struct renderable *r = get_renderable();
    memset(r, 0, sizeof(struct renderable));

    r->tex = tex_index;
    vec2s tex_size =
    {
        (f32)g_vulkan->textures[tex_index].w,
        (f32)g_vulkan->textures[tex_index].h,
    };

    size_t vertices_size = sizeof(struct vertex) * p->point_count;
    struct vertex *vertices = malloc(vertices_size);
    for (u32 i = 0; i < p->point_count; ++i)
    {
        vertices[i] = (struct vertex)
        {
            .pos = p->points[i],
            .texcoord = (vec2s)
            {{
                 -(p->points[p->tex_offset_point].x - p->points[i].x)
                     / tex_size.x,
                 (p->points[p->tex_offset_point].y - p->points[i].y)
                     / tex_size.y,
            }},
        };
    }
    vb_new(&r->vb, vertices, vertices_size);
    free(vertices);

    // Calculate indices
    i32 tri_count = p->point_count - 2;
    size_t index_buf_size = sizeof(u16) * tri_count * 3;
    u16 *indices = alloca(index_buf_size);
    for (i32 i = 0; i < tri_count; ++i)
    {
        indices[i * 3 + 0] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }
    ib_new(&r->ib, indices, index_buf_size);
}
