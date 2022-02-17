#ifndef RENDERER_H
#define RENDERER_H

#include "types.h"
#include "index_buffer.h"
#include "vertex_buffer.h"
#include "shader.h"

struct tagap_polygon;
struct tagap_layer;
struct tagap_linedef;
struct tagap_trigger;
struct tagap_theme_info;

#define DEPTH_BACKGROUND (-150.0f)
#define DEPTH_POLYGONS (-75.0f)
#define DEPTH_TRIGGERS (75.0f)
#define DEPTH_LINEDEFS (100.0f)
#define DEPTH_ENTITIES (150.0f)
#define DEPTH_ENV (220.0f)

/*
 * renderer.h
 *
 * Issues all draw commands, etc.
 */

static const u32 MAX_OBJECTS[SHADER_COUNT] =
{
    [SHADER_DEFAULT] = 1024, 
    [SHADER_DEFAULT_NO_ZBUFFER] = 1024, 
    [SHADER_VERTEXLIT] = 128
};

enum renderable_flag
{
    RENDERABLE_NO_FLAGS = 0,
    RENDERABLE_HIDDEN_BIT = 1,
    RENDERABLE_NO_CULL_BIT = 2,
    RENDERABLE_SHADED_BIT = 4,
    RENDERABLE_TEX_SCALE_BIT = 8,
    RENDERABLE_FLIPPED_BIT = 16,
    RENDERABLE_EXTRA_SHADING_BIT = 32,
};

struct renderable
{
    enum renderable_flag flags;
    struct vbuffer vb;
    struct ibuffer ib;
    i32 tex;
    vec2s pos;
    vec2s offset;
    f32 rot;
    vec2s tex_offset;

    // Additional shading multiplier
    vec4s extra_shading;

    // For culling objects outside of the viewport
    struct
    {
        vec2s min, max;
    } bounds;
};

struct renderer
{
    // Vertex buffers we want to render
    struct renderer_obj_group
    {
        struct renderable *objs;
        size_t obj_count;
    } objgroups[SHADER_COUNT];
};

extern struct renderer g_renderer;

int renderer_init(SDL_Window *);
void renderer_render(vec3s *);
void renderer_deinit(void);

struct renderable *renderer_get_renderable(enum shader_type);
struct renderable *renderer_get_renderable_quad(f32);
struct renderable *renderer_get_renderable_quad_dim(f32, f32, bool, f32);
void renderer_add_polygon(struct tagap_polygon *);
void renderer_add_layer(struct tagap_layer *, i32);
void renderer_add_linedefs(struct tagap_linedef *, size_t);
void renderer_add_trigger(struct tagap_trigger *);
void renderer_add_polygon_fade(struct tagap_polygon *);
struct renderable *renderer_add_env(struct tagap_theme_info *);

#endif
