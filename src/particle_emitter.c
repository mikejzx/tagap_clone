#include "pch.h"
#include "tagap.h"
#include "tagap_entity.h"
#include "particle_emitter.h"
#include "renderer.h"

static struct particle_emitter emitters[PARTICLE_FX_COUNT] =
{
    [PARTICLE_FX_SMOKE] =
    {
        .dir = PARTICLE_EMIT_AIM,
        .props =
        {
            .type = PARTICLE_SMOKE,
            .size.begin = 20.0f,
            .size.end = 48.0f,
            .opacity.begin = 1.0f,
            .opacity.end = 0.0f,
            .lifetime = 0.75f,
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
    //emitter->timer += DT;
    //if (emitter->timer < emitter->rate)
    //{
    //    return;
    //}
    //emitter->timer = 0.0f;
    static f32 timer = 0.0f;
    timer += DT;
    if (timer < 0.05f) return;
    timer = 0.0f;

    // Calculate particle velocity
    f32 emit_dir = 0.0f;
    switch(emitter->dir)
    {
    case PARTICLE_EMIT_N:
    {
        emit_dir = 90.0f;
    } break;
    case PARTICLE_EMIT_E:
    {
        emit_dir = 0.0f;
    } break;
    case PARTICLE_EMIT_S:
    {
        emit_dir = -90.0f;
    } break;
    case PARTICLE_EMIT_W:
    {
        emit_dir = 180.0f;
    } break;
    case PARTICLE_EMIT_AIM:
    {
        emit_dir = e->aim_angle;
    } break;
    default: break;
    }
    emit_dir += (f32)((rand() % 100 - 50) / 2.0f);
    emitter->props.velo.x = cosf(glm_rad(emit_dir));
    emitter->props.velo.y = sinf(glm_rad(emit_dir));
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

void
entity_weapon_fired_particle_fx(
    struct tagap_entity *e,
    struct tagap_entity_info *missile)
{
    f32 xflip = (f32)e->flipped * -2.0f + 1.0f;
    mat3s mat = GLMS_MAT3_IDENTITY_INIT;
    mat = glms_rotate2d(mat, glm_rad(e->aim_angle) * xflip);
    vec3s offset = (vec3s)
    {
        missile->offsets[OFFSET_WEAPON_OFFSET].x * xflip,
        missile->offsets[OFFSET_WEAPON_OFFSET].y,
    };
    offset = glms_mat3_mulv(mat, offset);
    vec2s offset2 = (vec2s){ offset.x, offset.y };

    // Apply smoke effect
    f32 ang = e->aim_angle + (f32)((rand() % 100 - 50) / 2.0f);
    struct particle_props props =
    {
        .type = PARTICLE_SMOKE,
        .size.begin = 24.0f,
        .size.end = 40.0f,
        .opacity.begin = 0.6f,
        .opacity.end = 0.0f,
        .lifetime = 0.4f,
        .velo = (vec2s)
        {
            cosf(glm_rad(ang)) * 96.0f * xflip,
            sinf(glm_rad(ang)) * 96.0f,
        },

        .pos = glms_vec2_add(e->position, offset2),
    };
    props.pos.y += missile->offsets[OFFSET_WEAPON_ORIGIN].y;
    props.pos.x += missile->offsets[OFFSET_WEAPON_ORIGIN].x * xflip;
    particle_emit(g_parts, &props);
}
