#ifndef RENDERER_H
#define RENDERER_H

#include "types.h"
#include "index_buffer.h"
#include "tagap_polygon.h"
#include "tagap_linedef.h"
#include "vertex_buffer.h"

/*
 * renderer.h
 *
 * Issues all draw commands, etc.
 */

#define MAX_OBJECTS 1024

struct renderable
{
    bool hidden;
    struct vbuffer vb;
    struct ibuffer ib;
    i32 tex;
    vec2s pos;
    f32 rot;
};

struct renderer
{
    // Vertex buffers we want to render
    struct renderable *objs;
    size_t obj_count;
};

extern struct renderer g_renderer;

int renderer_init(SDL_Window *);
void renderer_render(vec3s);
void renderer_deinit(void);

struct renderable *renderer_get_renderable(void);
void renderer_add_polygon(struct tagap_polygon *);
void renderer_add_linedefs(struct tagap_linedef *, size_t);

#endif
