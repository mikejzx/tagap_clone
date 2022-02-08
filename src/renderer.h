#ifndef RENDERER_H
#define RENDERER_H

#include "types.h"
#include "vertex_buffer.h"
#include "index_buffer.h"
#include "tagap_polygon.h"

/*
 * renderer.h
 *
 * Issues all draw commands, etc.
 */

#define MAX_OBJECTS 512

struct renderable
{
    struct vbuffer vb;
    struct ibuffer ib;
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
void renderer_render(void);
void renderer_deinit(void);
void renderer_add_polygon(struct tagap_polygon *);

#endif
