#include "pch.h"
#include "renderer.h"
#include "tagap.h"
#include "tagap_polygon.h"
#include "tagap_linedef.h"
#include "tagap_layer.h"
#include "tagap_trigger.h"
#include "vulkan_renderer.h"
#include "shader.h"

struct renderer g_renderer;

int
renderer_init(SDL_Window *winhandle)
{
    LOG_INFO("[renderer] initialising");
    memset(&g_renderer, 0, sizeof(struct renderer));

    if (vulkan_renderer_init(winhandle) < 0) return -1;

    // Allocate object groups
    for (u32 i = 0; i < SHADER_COUNT; ++i)
    {
        g_renderer.objgroups[i].objs = 
            malloc(MAX_OBJECTS[i] * sizeof(struct renderable));
    }


    return 0;
}

void
renderer_deinit(void)
{
    LOG_INFO("[renderer] deinitialising");
    vulkan_renderer_wait_for_idle();

    // Free object vertex and index buffers
    for (u32 i = 0; i < SHADER_COUNT; ++i)
    {
        for (u32 o = 0; o < g_renderer.objgroups[i].obj_count; ++o)
        {
            vb_free(&g_renderer.objgroups[i].objs[o].vb);
            ib_free(&g_renderer.objgroups[i].objs[o].ib);
        }
    }

    vulkan_renderer_deinit();

    // Free object groups
    for (u32 i = 0; i < SHADER_COUNT; ++i)
    {
        free(g_renderer.objgroups[i].objs);
    }
}

void
renderer_render(vec3s *cam_pos)
{
    // Record command buffers and render
    vulkan_render_frame_pre();
    vulkan_record_command_buffers(
        g_renderer.objgroups,
        SHADER_COUNT,
        cam_pos);
    vulkan_render_frame();
}

struct renderable *
renderer_get_renderable(enum shader_type shader)
{
    if (g_renderer.objgroups[shader].obj_count + 1 >= MAX_OBJECTS[shader])
    {
        LOG_ERROR("[renderer] object capacity exceeded!");
        return NULL;
    }
    struct renderable *r = &g_renderer.objgroups[shader].objs[
        g_renderer.objgroups[shader].obj_count++];
    memset(r, 0, sizeof(struct renderable));
    return r;
}

/*
 * Add polygon to the renderer
 */
void
renderer_add_polygon(struct tagap_polygon *p)
{
    // Check for special 'fade' polygons
    if (strcmp(p->tex_name, "(fade)") == 0)
    {
        renderer_add_polygon_fade(p);
        return;
    }

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
    struct renderable *r = renderer_get_renderable(SHADER_DEFAULT);

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
        /* Calculate vertex */
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

        /* Calculate bounds */
        if (pos.x < r->bounds.min.x) { r->bounds.min.x = pos.x; }
        if (pos.y < r->bounds.min.y) { r->bounds.min.y = pos.y; }
        if (pos.x > r->bounds.max.x) { r->bounds.max.x = pos.x; }
        if (pos.y > r->bounds.max.y) { r->bounds.max.y = pos.y; }
    }
    vb_new(&r->vb, vertices, vertices_size);
    free(vertices);

    /* Calculate indices */
    i32 tri_count = p->point_count - 2;
    size_t index_buf_size = sizeof(IB_TYPE) * tri_count * 3;
    IB_TYPE *indices = alloca(index_buf_size);
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
        IB_TYPE *i;
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
            info->i_size += 6 * sizeof(IB_TYPE);
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
        struct renderable *r = renderer_get_renderable(SHADER_DEFAULT);
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

/*
 * Add a layer to the renderer
 */
void
renderer_add_layer(struct tagap_layer *l)
{
    // Load layer texture
    char texpath[256];
    sprintf(texpath, "%s/%s.tga", TAGAP_LAYERS_DIR, l->tex_name);
    i32 tex_index = vulkan_texture_load(texpath);
    if (tex_index < 0)
    {
        LOG_WARN("[renderer] can't add layer; couldn't load texture '%s'",
            texpath);
        return;
    }

    // Get renderable
    struct renderable *r = renderer_get_renderable(SHADER_DEFAULT);
    l->r = r;

    // Set texture
    r->tex = tex_index;
    vec2s tex_size = 
    {
        (f32)g_vulkan->textures[tex_index].w,
        (f32)g_vulkan->textures[tex_index].h,
    };
    r->is_shaded = true;
    r->no_cull = true;
    r->parallax = l->scroll_speed_mul;

    f32 w = WIDTH_INTERNAL; 
    f32 h = tex_size.y;
    struct vertex vertices[4] =
    {
        // Top left
        {
            .pos      = (vec2s) { 0.0f, h },
            .texcoord = (vec2s) { 0.0f, 0.0f, },
        },
        // Top right
        {
            .pos      = (vec2s) { w, h },
            .texcoord = (vec2s) { w / tex_size.x, 0.0f, },
        },
        // Bottom right
        {
            .pos      = (vec2s) { w, 0.0f },
            .texcoord = (vec2s) { w / tex_size.x, 1.0f, },
        },
        // Bottom left
        {
            .pos      = (vec2s) { 0.0f, 0.0f },
            .texcoord = (vec2s) { 0.0f, 1.0f, },
        },
    };
    static const IB_TYPE indices[3 * 4] =
    {
        0, 1, 2,
        0, 2, 3
    };

    vb_new(&r->vb, vertices, 4 * sizeof(struct vertex));
    ib_new(&r->ib, indices, 3 * 4 * sizeof(IB_TYPE));
}

/*
 * Add any trigger renderers (if any)
 */
void
renderer_add_trigger(struct tagap_trigger *t)
{
    switch (t->id)
    {
    /* Trigger renders a layer */
    case TRIGGER_LAYER:
    {
        struct tagap_layer *l = NULL;
        if (t->target_index < 0 || t->target_index >= g_map->layer_count)
        {
            LOG_WARN("[renderer] trigger with id 'layer' has invalid target");
            break;
        }
        l = &g_map->layers[t->target_index];

        // Load layer texture
        char texpath[256];
        sprintf(texpath, "%s/%s.tga", TAGAP_LAYERS_DIR, l->tex_name);
        i32 tex_index = vulkan_texture_load(texpath);
        if (tex_index < 0)
        {
            LOG_WARN("[renderer] can't render layer; no texture '%s'", texpath);
            return;
        }
        f32 w = g_vulkan->textures[tex_index].w,
            h = g_vulkan->textures[tex_index].h;

        // Create the quad
        struct renderable *r = renderer_get_renderable_quad_dim(w, h, false);
        r->tex = tex_index;
        r->pos.x = t->corner_tl.x;
        r->pos.y = -t->corner_br.y;

        r->bounds.min.x = 0.0f;
        r->bounds.max.x = w;
        r->bounds.min.y = 0.0f;
        r->bounds.max.y = h;
    } break;

    default: break;
    }
}

struct renderable *
renderer_get_renderable_quad_dim(f32 w, f32 h, bool centre)
{
    struct renderable *r = renderer_get_renderable(SHADER_DEFAULT);
    if (!r) return NULL;

    struct vertex vertices[4];
    if (centre)
    {
        // Top left
        vertices[0] = (struct vertex)
        {
            .pos      = (vec2s) { -w, h },
            .texcoord = (vec2s) { 0.0f, 0.0f, },
        };
        // Top right
        vertices[1] = (struct vertex)
        {
            .pos      = (vec2s) { w, h },
            .texcoord = (vec2s) { 1.0f, 0.0f, },
        };
        // Bottom right
        vertices[2] = (struct vertex)
        {
            .pos      = (vec2s) { w, -h },
            .texcoord = (vec2s) { 1.0f, 1.0f, },
        };
        // Bottom left
        vertices[3] = (struct vertex)
        {
            .pos      = (vec2s) { -w, -h },
            .texcoord = (vec2s) { 0.0f, 1.0f, },
        };
    }
    else
    {
        // Top left
        vertices[0] = (struct vertex)
        {
            .pos      = (vec2s) { 0.0f, h },
            .texcoord = (vec2s) { 0.0f, 0.0f, },
        };
        // Top right
        vertices[1] = (struct vertex)
        {
            .pos      = (vec2s) { w, h },
            .texcoord = (vec2s) { 1.0f, 0.0f, },
        };
        // Bottom right
        vertices[2] = (struct vertex)
        {
            .pos      = (vec2s) { w, 0.0f },
            .texcoord = (vec2s) { 1.0f, 1.0f, },
        };
        // Bottom left
        vertices[3] = (struct vertex)
        {
            .pos      = (vec2s) { 0.0f, 0.0f },
            .texcoord = (vec2s) { 0.0f, 1.0f, },
        };
    }
    static const IB_TYPE indices[3 * 4] =
    {
        0, 1, 2,
        0, 2, 3
    };

    vb_new(&r->vb, vertices, 4 * sizeof(struct vertex));
    ib_new(&r->ib, indices, 3 * 4 * sizeof(IB_TYPE));

    return r;
}

struct renderable *
renderer_get_renderable_quad(void)
{
    return renderer_get_renderable_quad_dim(0.5f, 0.5f, true);
}

/*
 * Adds special 'fade' polygon to the renderer
 */
void
renderer_add_polygon_fade(struct tagap_polygon *p)
{
    // Get renderable
    struct renderable *r = renderer_get_renderable(SHADER_VERTEXLIT);
    r->is_shaded = p->tex_is_shaded;

    r->bounds.min.x = FLT_MAX;
    r->bounds.min.y = FLT_MAX;
    r->bounds.max.x = FLT_MIN;
    r->bounds.max.y = FLT_MIN;

    size_t vertices_size = sizeof(struct vertex_vl) * p->point_count;
    struct vertex_vl *vertices = malloc(vertices_size);
    for (u32 i = 0; i < p->point_count; ++i)
    {
        const vec2s pos = p->points[i];
        vertices[i] = (struct vertex_vl)
        {
            .pos = pos,
            .colour = (vec4s)
            {
                0.0f, 
                0.0f, 
                0.0f, 
                1.0f,
            },
        };

        /* Calculate bounds */
        if (pos.x < r->bounds.min.x) { r->bounds.min.x = pos.x; }
        if (pos.y < r->bounds.min.y) { r->bounds.min.y = pos.y; }
        if (pos.x > r->bounds.max.x) { r->bounds.max.x = pos.x; }
        if (pos.y > r->bounds.max.y) { r->bounds.max.y = pos.y; }
    }

    // For 'fade' texture, the offset point is transparent, and if it is
    // greater than 0, then the point before it is transparent too
    vertices[p->tex_offset_point].colour.w = 0.0f;
    if (p->tex_offset_point > 0)
    {
        vertices[p->tex_offset_point - 1].colour.w = 0.0f;
    }
    
    vb_new(&r->vb, vertices, vertices_size);
    free(vertices);

    // Calculate indices
    i32 tri_count = p->point_count - 2;
    size_t index_buf_size = sizeof(IB_TYPE) * tri_count * 3;
    IB_TYPE *indices = alloca(index_buf_size);
    for (i32 i = 0; i < tri_count; ++i)
    {
        indices[i * 3 + 0] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }
    ib_new(&r->ib, indices, index_buf_size);
}

