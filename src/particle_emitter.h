#ifndef PARTICLE_EMITTER_H
#define PARTICLE_EMITTER_H

#include "particle.h"

/*
 * particle_emitter.h
 *
 * Used on entities to indicate an emitter
 */

struct tagap_entity;

enum particle_fx_mode
{
    PARTICLE_FX_SMOKE,
    PARTICLE_FX_SMOKE_TRAIL,
    PARTICLE_FX_COUNT,
};

struct particle_emitter
{
    // Emission direction
    enum particle_emit_dir
    {
        PARTICLE_EMIT_N,
        PARTICLE_EMIT_E,
        PARTICLE_EMIT_S,
        PARTICLE_EMIT_W,
        PARTICLE_EMIT_SPIN,
        PARTICLE_EMIT_AIM,
    } dir;

    // Properties of the particle
    struct particle_props props;
    f32 speed_mul;

    // Emission rate
    f32 rate;
    f32 timer;
};

void entity_apply_particle_fx(struct tagap_entity *);
void entity_weapon_fired_particle_fx(
    struct tagap_entity *,
    struct tagap_entity_info *);

#endif
