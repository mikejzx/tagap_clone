#include "pch.h"
#include "vulkan_renderer.h"
#include "renderer.h"
#include "tagap_polygon.h"

struct renderer g_renderer;

static struct renderable *get_renderable(void);

int
renderer_init(SDL_Window *winhandle)
{
    LOG_INFO("[renderer] initialising");
    memset(&g_renderer, 0, sizeof(struct renderer));

    if (vulkan_renderer_init(winhandle) < 0) return -1;

    g_renderer.objs = malloc(MAX_OBJECTS * sizeof(struct renderable));

    // Create temporary renderable objects
#if 0
    {
        const struct vertex vertices[] = 
        {
            { 10.0f, 10.0f, }, // Top left
            { 200.0f, 200.0f, }, // Bottom right
            { 10.0f, 200.0f, }, // Bottom left
            { 200.0f, 10.0f, }, // Top right
        };
        const u16 indices[] = 
        {
            0, 1, 2, 
            0, 3, 1,
        };
        struct renderable *r = get_renderable();
        vb_new(&r->vb, vertices, sizeof(vertices));
        ib_new(&r->ib, indices, sizeof(indices));
        r->pos = (vec2s){{ 0.0f, 1.0f }};
        r->rot = 0.0f;
    }
#endif

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
renderer_render(void)
{
    g_renderer.objs[0].pos = (vec2s){{ SDL_GetTicks() / 10.0f, 200.0f }};
    g_renderer.objs[0].rot = SDL_GetTicks() / 1.0f;

    // Record command buffers and render
    vulkan_render_frame_pre();
    vulkan_record_command_buffers(
        g_renderer.objs, 
        g_renderer.obj_count);
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
    struct renderable *r = get_renderable();
    memset(r, 0, sizeof(struct renderable));

    vb_new(&r->vb, p->points, sizeof(vec2s) * p->point_count);

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
