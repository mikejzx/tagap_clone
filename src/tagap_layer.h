#ifndef TAGAP_LAYER_H
#define TAGAP_LAYER_H

#define LAYER_TEX_NAME_MAX 64

struct renderable;

struct tagap_layer
{
    struct renderable *r;
    char tex_name[LAYER_TEX_NAME_MAX];
    f32 scroll_speed_mul;
    f32 offset_y;
    vec2s movement_speed;

    // TODO: enum for this
    i32 rendering_flag;
};

#endif
