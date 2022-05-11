#ifndef TAGAP_ENTITY_FX_H
#define TAGAP_ENTITY_FX_H

struct tagap_entity;

struct tagap_entity_fx
{
    // Light renderer
    struct renderable *r_light;
    f32 timer_dim;

    // Flashlight renderer
    struct renderable *r_flashlight;

    // Muzzle flash (light) renderer
    struct renderable *r_muzzle;
    f32 muzzle_timer;

    // Smoke trail timer
    f32 smoke_timer;
};

i32 entity_fx_init(struct tagap_entity *);
void entity_fx_toggle(struct tagap_entity_fx *, bool);
void entity_fx_update(struct tagap_entity *);
void entity_fx_die(struct tagap_entity *);

inline void
entity_fx_reset(struct tagap_entity_fx *fx)
{
    fx->timer_dim = 0.0f;
    fx->muzzle_timer = -1.0f;
}

#endif
