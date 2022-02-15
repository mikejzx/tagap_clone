#ifndef TAGAP_LAYER_H
#define TAGAP_LAYER_H

#define LAYER_TEX_NAME_MAX 64

enum tagap_layer_rendering_flag
{
    LAYER_RENDER_NORMAL   = 0, // No layer flags
    LAYER_RENDER_FADING   = 1, // Fades from solid to transparent
    LAYER_RENDER_GLOOM    = 2, // Layer illuminates through windows/doors
    LAYER_RENDER_DISABLED = 3, // Not drawn in backgrounds or monitor screens
};

struct renderable;

struct tagap_layer
{
    struct renderable *r;
    char tex_name[LAYER_TEX_NAME_MAX];
    f32 scroll_speed_mul;
    f32 offset_y;
    vec2s movement_speed;

    enum tagap_layer_rendering_flag rendering_flag;

    u32 depth;
};

#endif
