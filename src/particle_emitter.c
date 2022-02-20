#include "pch.h"
#include "tagap.h"
#include "tagap_entity.h"
#include "particle_emitter.h"
#include "renderer.h"

static struct particle_emitter emitters[PARTICLE_FX_COUNT] =
{
    [PARTICLE_FX_SMOKE] =
    {
        .dir = PARTICLE_EMIT_N,
        .props =
        {
            .type = PARTICLE_SMOKE,
            .size.begin = 8.0f,
            .size.end = 16.0f,
            .opacity.begin = 1.0f,
            .opacity.end = 0.0f,
            .lifetime = 0.15f,
        },
        .rate = 0.1f,
        .speed_mul = 48.0f,
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

    f32 xflip = (f32)e->flipped * -2.0f + 1.0f;

    // Set particle emission position
    mat3s mat = GLMS_MAT3_IDENTITY_INIT;
    mat = glms_rotate2d(mat, glm_rad(e->aim_angle) * xflip);
    vec3s offset = (vec3s)
    {
        e->info->offsets[OFFSET_FX_OFFSET].x * xflip,
        e->info->offsets[OFFSET_FX_OFFSET].y,
    };
    offset = glms_mat3_mulv(mat, offset);
    vec2s offset2 = (vec2s){ offset.x, offset.y };

    emitter->props.pos = glms_vec2_add(e->position, offset2);
    emitter->props.pos.y += e->info->offsets[OFFSET_MODEL_OFFSET].y;
    emitter->props.pos.x += e->info->offsets[OFFSET_MODEL_OFFSET].x * xflip;

    particle_emit(g_parts, &emitter->props);
}
