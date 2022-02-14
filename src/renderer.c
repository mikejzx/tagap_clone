#include "pch.h"
#include "renderer.h"
#include "tagap.h"
#include "tagap_polygon.h"
#include "vulkan_renderer.h"

struct renderer g_renderer;

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

struct renderable *
renderer_get_renderable(void)
{
    if (g_renderer.obj_count + 1 >= MAX_OBJECTS)
    {
        LOG_ERROR("[renderer] object capacity exceeded!");
        return NULL;
    }
    struct renderable *r = &g_renderer.objs[g_renderer.obj_count++];
    memset(r, 0, sizeof(struct renderable));
    return r;
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
        LOG_WARN("[renderer] can't add polygon; couldn't load texture '%s'",
            texpath);
        return;
    }

    // Get renderable
    struct renderable *r = renderer_get_renderable();

    r->tex = tex_index;
    vec2s tex_size =
    {
        (f32)g_vulkan->textures[tex_index].w,
        (f32)g_vulkan->textures[tex_index].h,
    };
    r->is_shaded = p->tex_is_shaded;

    r->bounds.min.x = FLT_MAX;
    r->bounds.min.y = FLT_MAX;
    r->bounds.max.x = FLT_MIN;
    r->bounds.max.y = FLT_MIN;

    size_t vertices_size = sizeof(struct vertex) * p->point_count;
    struct vertex *vertices = malloc(vertices_size);
    for (u32 i = 0; i < p->point_count; ++i)
    {
        /*
         * Calculate vertex
         */
        const vec2s pos = p->points[i];
        vertices[i] = (struct vertex)
        {
            .pos = pos,
            .texcoord = (vec2s)
            {{
                 -(p->points[p->tex_offset_point].x - p->points[i].x)
                     / tex_size.x,
                 (p->points[p->tex_offset_point].y - p->points[i].y)
                     / tex_size.y,
            }},
        };

        /*
         * Calculate bounds
         */
        // Min X
        if (pos.x < r->bounds.min.x) { r->bounds.min.x = pos.x; }
        if (pos.y < r->bounds.min.y) { r->bounds.min.y = pos.y; }
        if (pos.x > r->bounds.max.x) { r->bounds.max.x = pos.x; }
        if (pos.y > r->bounds.max.y) { r->bounds.max.y = pos.y; }
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

/*
 * Add list of level linedefs to the renderer
 */
void
renderer_add_linedefs(struct tagap_linedef *ldefs, size_t lc)
{
    struct displayed_linedef
    {
        enum tagap_linedef_style style;
        char tex[32];
        struct vertex *v;
        u16 *i;
        size_t v_size, i_size;
    } linfo[] =
    {
        // List of linedefs styles that we actually draw
        { LINEDEF_STYLE_FLOOR, "(world-floor)", 0, 0, 0, 0 },
        { LINEDEF_STYLE_PLATE_FLOOR, "(world-plate)", 0, 0, 0, 0 },
        // Don't draw ceilings
    };
    static const u32 displayed_linedef_count =
        sizeof(linfo) / sizeof(struct displayed_linedef);

    for (u32 d = 0; d < displayed_linedef_count; ++d)
    {
        struct displayed_linedef *info = &linfo[d];

        // Count number of linedefs of this style
        u32 i;
        for (i = 0; i < lc; ++i)
        {
            if (ldefs[i].style != info->style) continue;

            // Skip vertical lines (walls)
            if (ldefs[i].start.x == ldefs[i].end.x) continue;

            info->v_size += 4 * sizeof(struct vertex);
            info->i_size += 6 * sizeof(u16);
        }
        if (!info->v_size || !info->i_size)
        {
            LOG_WARN("[renderer] skipping linedef style %d as "
                "it has no lines", info->style);
            continue;
        }

        // Load texture
        char tex_path[256];
        sprintf(tex_path, "%s/%s.tga", TAGAP_TEXTURES_DIR, info->tex);
        i32 tex_index = vulkan_texture_load(tex_path);
        if (tex_index < 0)
        {
            LOG_WARN("[renderer] skipping linedef style %d as "
                "texture '%s' failed to load", info->style, info->tex);
            continue;
        }
        f32 tex_w = (f32)g_vulkan->textures[tex_index].w;
        f32 tex_h = (f32)g_vulkan->textures[tex_index].h;

        // Allocate buffers
        info->v = malloc(info->v_size);
        info->i = malloc(info->i_size);

        // Fill buffers
        u32 cur_l = 0;
        for (i = 0; i < lc; ++i)
        {
            struct tagap_linedef *l = &ldefs[i];
            if (l->style != info->style) continue;
            if (ldefs[i].start.x == ldefs[i].end.x) continue;

            // Length of the line
            f32 llen = glms_vec2_distance(l->start, l->end);

            // Top left
            info->v[cur_l * 4 + 0] = (struct vertex)
            {
                .pos = (vec2s)
                {
                    l->start.x,
                    l->start.y,
                },
                .texcoord = (vec2s)
                {
                    0.0f,
                    0.0f,
                },
            };
            // Top right
            info->v[cur_l * 4 + 1] = (struct vertex)
            {
                .pos = (vec2s)
                {
                    l->end.x,
                    l->end.y,
                },
                .texcoord = (vec2s)
                {
                    llen / tex_w,
                    0.0f,
                },
            };
            // Bottom right
            info->v[cur_l * 4 + 2] = (struct vertex)
            {
                .pos = (vec2s)
                {
                    l->end.x,
                    l->end.y - tex_h,
                },
                .texcoord = (vec2s)
                {
                    llen / tex_w,
                    1.0f,
                },
            };
            // Bottom left
            info->v[cur_l * 4 + 3] = (struct vertex)
            {
                .pos = (vec2s)
                {
                    l->start.x,
                    l->start.y - tex_h,
                },
                .texcoord = (vec2s)
                {
                    0.0f,
                    1.0f,
                },
            };

            // Indices are pretty straightforward
            info->i[cur_l * 6 + 0] = cur_l * 4;
            info->i[cur_l * 6 + 1] = cur_l * 4 + 1;
            info->i[cur_l * 6 + 2] = cur_l * 4 + 2;
            info->i[cur_l * 6 + 3] = cur_l * 4;
            info->i[cur_l * 6 + 4] = cur_l * 4 + 2;
            info->i[cur_l * 6 + 5] = cur_l * 4 + 3;

            ++cur_l;
        }

        // Create renderables
        struct renderable *r = renderer_get_renderable();
        if (r)
        {
            LOG_DBUG("[renderer] adding linedef vertex buffer "
                "(style %d) with %d lines", info->style, cur_l);
            memset(r, 0, sizeof(struct renderable));
            r->tex = tex_index;
            r->no_cull = true;
            vb_new(&r->vb, info->v, info->v_size);
            ib_new(&r->ib, info->i, info->i_size);
        }

        // Cleanup
        free(info->v);
        free(info->i);
    }
}

struct renderable *
renderer_get_renderable_quad(void)
{
    struct renderable *r = renderer_get_renderable();
    if (!r) return NULL;

    // Simple quad vertices and indices
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

    vb_new(&r->vb, vertices, 4 * sizeof(struct vertex));
    ib_new(&r->ib, indices, 3 * 4 * sizeof(u16));

    return r;
}

