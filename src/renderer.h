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

/*
 * renderer.h
 *
 * Issues all draw commands, etc.
 */

static const u32 MAX_OBJECTS[SHADER_COUNT] =
{
    [SHADER_DEFAULT] = 1024, 
    [SHADER_VERTEXLIT] = 128
};

struct renderable
{
    // TODO: bitmask for booleans

    bool hidden;
    bool tex_scale;
    bool is_shaded;
    struct vbuffer vb;
    struct ibuffer ib;
    i32 tex;
    vec2s pos;
    vec2s offset;
    f32 rot;
    bool flipped;
    f32 parallax;

    // For culling objects outside of the viewport
    struct
    {
        vec2s min, max;
    } bounds;
    bool no_cull;
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
struct renderable *renderer_get_renderable_quad(void);
struct renderable * renderer_get_renderable_quad_dim(f32, f32, bool);
void renderer_add_polygon(struct tagap_polygon *);
void renderer_add_layer(struct tagap_layer *);
void renderer_add_linedefs(struct tagap_linedef *, size_t);
void renderer_add_trigger(struct tagap_trigger *);
void renderer_add_polygon_fade(struct tagap_polygon *);

#endif
