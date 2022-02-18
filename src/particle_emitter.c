#include "pch.h"
#include "tagap.h"
#include "tagap_entity.h"
#include "particle_emitter.h"

static struct particle_emitter emitters[PARTICLE_FX_COUNT] =
{
    [PARTICLE_FX_SMOKE] =
    {
        .dir = PARTICLE_EMIT_N,
        .props =
        {
            .type = PARTICLE_SMOKE,
            .size.begin = 16.0f,
            .size.end = 64.0f,
            .opacity.begin = 1.0f,
            .opacity.end = 0.0f,
            .lifetime = 1.0f,
        },
        .rate = 0.1f,
        .speed_mul = 64.0f,
    },
};

static void particle_fx_emit(struct tagap_entity *, struct particle_emitter *);

void
entity_apply_particle_fx(struct tagap_entity *e)
{
    for (u32 i = 0; i < ENTITY_STAT_COUNT; ++i)
    {
        switch (i)
        {
        // Smoke trail
        case STAT_FX_SMOKE:
        {
            if (e->info->stats[i] == 1)
            {
                particle_fx_emit(e, &emitters[PARTICLE_FX_SMOKE]);
            }
        } break;

        default: break;
        }
    }
}

static void 
particle_fx_emit(struct tagap_entity *e, struct particle_emitter *emitter)
{
    emitter->timer += DT;
    if (emitter->timer < emitter->rate)
    {
        return;
    }
    emitter->timer = 0.0f;

    // Calculate particle velocity
    switch(emitter->dir)
    {
    case PARTICLE_EMIT_N:
    {
        emitter->props.velo.x = 0.0f;
        emitter->props.velo.y = 1.0f;
    } break;
    case PARTICLE_EMIT_E:
    {
        emitter->props.velo.x = 1.0f;
        emitter->props.velo.y = 0.0f;
    } break;
    case PARTICLE_EMIT_S:
    {
        emitter->props.velo.x = 0.0f;
        emitter->props.velo.y = -1.0f;
    } break;
    case PARTICLE_EMIT_W:
    {
        emitter->props.velo.x = -1.0f;
        emitter->props.velo.y = 0.0f;
    } break;
    default: break;
    }
    emitter->props.velo = glms_vec2_scale(emitter->props.velo, 
        emitter->speed_mul);

    // Set particle emission position
    emitter->props.pos = e->position;

    particle_emit(g_parts, &emitter->props);
}
