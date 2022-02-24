#include "pch.h"
#include "tagap_entity.h"
#include "tagap.h"
#include "tagap_entity_fx.h"
#include "renderer.h"
#include "particle.h"

#define MUZZLE_BASE_SIZE_MUL (1.6f)
#define MUZZLE_INTENSITY (30.0f)
#define MUZZLE_FLASH_SPEED (12.5f)

static i32 entity_fx_init_light(struct tagap_entity *);
static i32 entity_fx_init_muzzle(struct tagap_entity *);
static i32 entity_fx_init_flashlight(struct tagap_entity *);

i32
entity_fx_init(struct tagap_entity *e)
{
    i32 status;

    // Initialise muzzle flash
    (void)((status = entity_fx_init_muzzle(e)) < 0 ||

    // Create entity light (if it has one)
    (status = entity_fx_init_light(e)) < 0 ||

    // Create entity flashlight (if it has one)
    (status = entity_fx_init_flashlight(e)) < 0);

    return status;
}

void
entity_fx_toggle(struct tagap_entity_fx *fx, bool h)
{
    if (fx->r_light)
    {
        SET_BIT(fx->r_light->flags, RENDERABLE_HIDDEN_BIT, h);
    }
    if (fx->r_flashlight)
    {
        SET_BIT(fx->r_flashlight->flags, RENDERABLE_HIDDEN_BIT, h);
    }
    if (fx->r_muzzle)
    {
        SET_BIT(fx->r_muzzle->flags, RENDERABLE_HIDDEN_BIT, h);
    }
}

void
entity_fx_update(struct tagap_entity *e)
{
    struct tagap_entity_fx *const fx = &e->fx;

    struct tagap_entity_info *missile = NULL;
    if (e->weapon_slot >= 0 && e->weapon_slot < WEAPON_SLOT_COUNT)
    {
        missile = g_level->weapons[e->weapon_slot].primary;
    }

    f32 xflip = (f32)e->flipped * -2.0f + 1.0f;

    mat3s mat = GLMS_MAT3_IDENTITY_INIT;
    if (fx->r_muzzle || e->info->stats[STAT_FX_SMOKE])
    {
        mat = glms_rotate2d(mat, glm_rad(e->aim_angle) * xflip);
    }
    vec3s offset;

    /*
     * Light effects
     */

    if (fx->r_light)
    {
        // Update light position
        fx->r_light->pos = (vec2s)
        {
            e->position.x + e->info->offsets[OFFSET_FX_OFFSET].x +
                e->info->stats[STAT_FX_OFFSXFACE] * xflip,
            (e->position.y + e->info->offsets[OFFSET_FX_OFFSET].y) * -1.0f,
        };

        // Entity light pulsing via DIM
        // (we don't modify alpha as e.g. FX_FADE can modify it)
        fx->timer_dim += (DT + e->info->stats[STAT_FX_DIM]) * 6.75f;
        f32 dim = (sinf(fx->timer_dim) + 1.0f) / 4.0f + 0.5f;
        fx->r_light->light_colour.x =
            e->info->light.colour.x * e->info->light.intensity * dim;
        fx->r_light->light_colour.y =
            e->info->light.colour.y * e->info->light.intensity * dim;
        fx->r_light->light_colour.z =
            e->info->light.colour.z * e->info->light.intensity * dim;
    }

    if (fx->r_flashlight)
    {
        // Update flashlight position and rotation
        vec2s origin = e->info->flashlight.origin;
        fx->r_flashlight->pos = e->position;
        fx->r_flashlight->pos.y *= -1.0f;
        fx->r_flashlight->offset = (vec2s)
        {
            origin.x - 24.0f * cosf(glm_rad(e->aim_angle)),
            origin.y - 24.0f * sinf(glm_rad(e->aim_angle)),
        };
        fx->r_flashlight->rot = e->aim_angle;
        SET_BIT(fx->r_flashlight->flags, RENDERABLE_FLIPPED_BIT, e->flipped);
    }

    if (fx->r_muzzle && !e->info->stats[STAT_FX_DISABLE])
    {
        offset = glms_mat3_mulv(mat, (vec3s)
        {
            missile->offsets[OFFSET_WEAPON_OFFSET].x * xflip,
            missile->offsets[OFFSET_WEAPON_OFFSET].y,
            0.0f,
        });

        // Update muzzle light
        fx->r_muzzle->pos = (vec2s)
        {
            e->position.x + offset.x +
                missile->offsets[OFFSET_WEAPON_ORIGIN].x * xflip,
            (e->position.y + offset.y +
                missile->offsets[OFFSET_WEAPON_ORIGIN].y) * -1.0f,
        };
        fx->r_muzzle->scale = clamp01(fx->muzzle_timer) *
            g_level->weapons[e->weapon_slot].primary->stats[STAT_FX_MUZZLE] /
            100.0f;
        if (fx->muzzle_timer >= 0.0f)
        {
            fx->muzzle_timer -= DT * MUZZLE_FLASH_SPEED;
        }
    }

    /*
     * Particle effects
     */
    if (e->info->stats[STAT_FX_DISABLE]) return;

    // Aim angle with some randomisation
    f32 ang = e->aim_angle + (f32)((rand() % 100 - 50) / 2.0f);
    f32 ang_cos = cosf(glm_rad(ang));
    f32 ang_sin = sinf(glm_rad(ang));

    /*
     * Bullet effect for trace attacks.  These are not actual projectiles and
     * are simply for visual feedback of where trace attack weapon is shooting
     */
    if (missile && missile->stats[STAT_FX_BULLET] && e->firing_now)
    {
        static const f32
            TRACER_W = 144.0f,
            TRACER_W_GROW = 0.75f,
            TRACER_H = 20.0f,
            TRACER_H_GROW = 0.0f,
            TRACER_SPEED = 3000.0f;
        struct particle_props props =
        {
            .type = PARTICLE_BEAM,
            .size_x.begin = TRACER_W,
            .size_x.end = TRACER_W + TRACER_W * TRACER_W_GROW,
            .size_y.begin = TRACER_H,
            .size_y.end = TRACER_H + TRACER_H * TRACER_H_GROW,
            .independent_sizes = true,
            .flip_x = !e->flipped,
            .colour.begin = { 1.0f, 1.0f, 1.0f, 1.0f },
            .colour.end = { 1.0f, 1.0f, 1.0f, 0.0f },
            .vertex_colour_muls = true,
            .vertex_colours =
            {
                [0] = { 1.0f, 0.7f, 0.2f, 1.0f },
                [1] = { 1.0f, 0.7f, 0.2f, 1.0f },
                [2] = { 0.6f, 0.2f, 0.2f, 1.0f },
                [3] = { 0.6f, 0.2f, 0.2f, 1.0f },
            },
            .lifetime = 0.15f,
        };
        offset = glms_mat3_mulv(mat, (vec3s)
        {
            missile->offsets[OFFSET_WEAPON_OFFSET].x * xflip,
            missile->offsets[OFFSET_WEAPON_OFFSET].y,
            0.0f,
        });
        props.pos = (vec2s)
        {
            e->position.x + offset.x +
                missile->offsets[OFFSET_WEAPON_ORIGIN].x * xflip,
            e->position.y + offset.y +
                missile->offsets[OFFSET_WEAPON_ORIGIN].y,
        };
        props.pivot_bias = (vec2s)
        {
            -0.7f, 0.0f
        };

        // Emit tracer for each multishot
        for (u32 s = 0; s < e->weapon_multishot; ++s)
        {
            f32 angle = e->weapon_multishot_angles[s];
            f32 c = cosf(glm_rad(angle)), s = sinf(glm_rad(angle));
            props.rot = angle * xflip,
            props.velo = (vec2s)
            {
                c * TRACER_SPEED * xflip,
                s * TRACER_SPEED
            };
            particle_emit(&props);
        }
    }

    /*
     * Weapon firing smoke
     */
    static const f32 FIRE_SMOKE_VELO = 96.0f;
    struct particle_props props =
    {
        .type = PARTICLE_SMOKE,
        .size.begin = 24.0f,
        .size.end = 40.0f,
        .colour.begin = { 1.0f, 1.0f, 1.0f, 0.6f },
        .colour.end = { 1.0f, 1.0f, 1.0f, 0.0f },
        .lifetime = 0.4f,
    };
    if (e->firing_now)
    {
        offset = glms_mat3_mulv(mat, (vec3s)
        {
            missile->offsets[OFFSET_WEAPON_OFFSET].x * xflip,
            missile->offsets[OFFSET_WEAPON_OFFSET].y,
            0.0f,
        });
        props.pos = (vec2s)
        {
            e->position.x + offset.x +
                missile->offsets[OFFSET_WEAPON_ORIGIN].x * xflip,
            e->position.y + offset.y +
                missile->offsets[OFFSET_WEAPON_ORIGIN].y,
        };
        props.velo = (vec2s)
        {
            ang_cos * FIRE_SMOKE_VELO * xflip,
            ang_sin * FIRE_SMOKE_VELO,
        };
        // Emit weapon fire smoke
        particle_emit(&props);
    }

    /*
     * Smoke trail
     */
    if (!e->info->stats[STAT_FX_SMOKE]) return;

    static const f32 SMOKE_TRAIL_RATE = 0.07f, SMOKE_TRAIL_VELO = 64.0f;
    fx->smoke_timer += DT;
    if (fx->smoke_timer < SMOKE_TRAIL_RATE) return;
    fx->smoke_timer  = 0.0f;

    props = (struct particle_props)
    {
        .type = PARTICLE_SMOKE,
        .size.begin = 20.0f,
        .size.end = 48.0f,
        .colour.begin = { 1.0f, 1.0f, 1.0f, 1.0f },
        .colour.end = { 1.0f, 1.0f, 1.0f, 0.0f },
        .lifetime = 0.75f,
    };

    // Get smoke trail emission position
    offset = glms_mat3_mulv(mat, (vec3s)
    {
        e->info->offsets[OFFSET_FX_OFFSET].x * xflip,
        e->info->offsets[OFFSET_FX_OFFSET].y,
        0.0f,
    });
    props.pos = (vec2s)
    {
        e->position.x + offset.x +
            e->info->offsets[OFFSET_MODEL_OFFSET].x * xflip,
        e->position.y + offset.y +
            e->info->offsets[OFFSET_MODEL_OFFSET].y,
    };
    props.velo = (vec2s)
    {
        ang_cos * SMOKE_TRAIL_VELO * xflip,
        ang_sin * SMOKE_TRAIL_VELO,
    };

    // Emit smoke trail
    particle_emit(&props);
}

static i32
entity_fx_init_light(struct tagap_entity *e)
{
    if (e->info->light.radius <= 0.0f ||
        e->info->light.intensity <= 0.0f)
    {
        // No light
        return 0;
    }
    struct tagap_entity_fx *const fx = &e->fx;

    // Load default light texture
    i32 tex = vulkan_texture_load(TAGAP_EFFECTS_DIR "/fx_light.tga");
    if (tex <= 0)
    {
        LOG_ERROR("[tagap_entity_fx] failed to add light (no texture)");
        return -1;
    }

    // Create the light quad
    struct renderable_quad_info quad =
    {
        .shader = SHADER_LIGHT,
        .w = g_vulkan->textures[tex].w * e->info->light.radius * 2.0f,
        .h = g_vulkan->textures[tex].h * e->info->light.radius * 2.0f,
        .centre_x = true,
        .centre_y = true,
        .make_bounds = true,
    };
    fx->r_light = renderer_get_renderable_quad(&quad);
    if (!fx->r_light)
    {
        LOG_ERROR("[tagap_entity_fx] failed to add light");
        return -1;
    }
    fx->r_light->tex = tex;
    fx->r_light->light_colour = (vec4s)
    {
        e->info->light.colour.x * e->info->light.intensity,
        e->info->light.colour.y * e->info->light.intensity,
        e->info->light.colour.z * e->info->light.intensity,
        1.0f,
    };

    // Enable light expanding
    if (e->info->stats[STAT_FX_EXPAND])
    {
        fx->r_light->flags |= RENDERABLE_SCALED_BIT;
        fx->r_light->scale = 1.0f;
    }
    return 0;
}

static i32
entity_fx_init_muzzle(struct tagap_entity *e)
{
    // Need a weapon for muzzle flash
    if (e->weapon_slot < 0 || e->weapon_slot >= WEAPON_SLOT_COUNT) return 0;

    i32 tex = vulkan_texture_load(TAGAP_EFFECTS_DIR "/fx_light.tga");
    if (tex <= 0)
    {
        LOG_ERROR("[tagap_entity_fx] failed to add muzzle (no texture)");
        return -1;
    }
    struct tagap_entity_fx *const fx = &e->fx;

    // Create muzzle flash light
    struct renderable_quad_info quad =
    {
        .shader = SHADER_LIGHT,
        .w = g_vulkan->textures[tex].w * MUZZLE_BASE_SIZE_MUL,
        .h = g_vulkan->textures[tex].h * MUZZLE_BASE_SIZE_MUL,
        .centre_x = true,
        .centre_y = true,
        .make_bounds = true,
    };
    fx->r_muzzle = renderer_get_renderable_quad(&quad);
    if (!fx->r_muzzle)
    {
        LOG_ERROR("[tagap_entity_fx] failed to add muzzle flash light");
        return -1;
    }
    fx->r_muzzle->tex = tex;
    fx->r_muzzle->light_colour = (vec4s)
    {
        1.0f * MUZZLE_INTENSITY,
        0.8f * MUZZLE_INTENSITY,
        0.0f * MUZZLE_INTENSITY,
        1.0f,
    };
    fx->r_muzzle->flags |= RENDERABLE_SCALED_BIT;
    fx->r_muzzle->scale = 1.0f;

    return 0;
}

static i32
entity_fx_init_flashlight(struct tagap_entity *e)
{
    if (e->info->flashlight.halo_radius <= 0.0f ||
        e->info->flashlight.beam_length <= 0.0f)
    {
        // No flashlight
        return 0;
    }
    struct tagap_entity_fx *const fx = &e->fx;

    // Load flashlight texture
    i32 tex = vulkan_texture_load(TAGAP_EFFECTS_DIR "/fx_flashlight.tga");
    if (tex <= 0)
    {
        LOG_ERROR("[tagap_entity_fx] failed to add flashlight (no texture)");
        return -1;
    }

    /* Create the flashlight quad */
    struct renderable_quad_info quad =
    {
        .shader = SHADER_LIGHT,
        .w = g_vulkan->textures[tex].w *
            e->info->flashlight.beam_length * 2.0f,
        .h = g_vulkan->textures[tex].h *
            e->info->flashlight.halo_radius * 0.65f,
        .centre_x = false,
        .centre_y = true,
        .make_bounds = true,
    };
    fx->r_flashlight = renderer_get_renderable_quad(&quad);
    if (!fx->r_flashlight)
    {
        LOG_ERROR("[tagap_entity_fx] failed to add flashlight");
        return -1;
    }
    fx->r_flashlight->tex = tex;
    fx->r_flashlight->light_colour = (vec4s)
    {
        e->info->flashlight.colour.x * 0.1f,
        e->info->flashlight.colour.y * 0.1f,
        e->info->flashlight.colour.z * 0.1f,
        1.0f,
    };

    /* Culling */
    if (e->info->think.mode == THINK_AI_USER)
    {
        // Don't cull the player flashlight
        fx->r_flashlight->flags |= RENDERABLE_NO_CULL_BIT;
        return 0;
    }

    // Simple fix for rotated flashlights being culled; not sure if the
    // rotated points are exactly on point, but it seems to work fine
    // as-is
    vec2s bounds_min_new, bounds_max_new;
    switch ((i32)e->aim_angle)
    {
    case -270:
    case 90:
        bounds_min_new.x = fx->r_flashlight->bounds.min.x;
        bounds_min_new.y = fx->r_flashlight->bounds.min.y;
        bounds_max_new.x = fx->r_flashlight->bounds.min.y;
        bounds_max_new.y = fx->r_flashlight->bounds.max.x;
        break;
    case -180:
    case 180:
        bounds_min_new.x = -fx->r_flashlight->bounds.max.x;
        bounds_min_new.y = fx->r_flashlight->bounds.min.y;
        bounds_max_new.x = fx->r_flashlight->bounds.min.x;
        bounds_max_new.y = fx->r_flashlight->bounds.max.y;
        break;
    case -90:
    case 270:
        bounds_min_new.x = fx->r_flashlight->bounds.min.y;
        bounds_min_new.y = -fx->r_flashlight->bounds.max.x;
        bounds_max_new.x = fx->r_flashlight->bounds.max.y;
        bounds_max_new.y = fx->r_flashlight->bounds.max.y;
        break;
    default:
        LOG_WARN("[tagap_entity_fx] '%s' flashlight has unusual angle "
            "%.0f deg; disabling culling", e->aim_angle);
        fx->r_flashlight->flags |= RENDERABLE_NO_CULL_BIT;
    case 360:
    case 0:
        break;
    }
    fx->r_flashlight->bounds.min = bounds_min_new;
    fx->r_flashlight->bounds.max = bounds_max_new;
    LOG_DBUG("[tagap_entity_fx] rotated flashlight bounds: "
        "%.2f %.2f  %.2f %.2f",
        fx->r_flashlight->bounds.min.x,
        fx->r_flashlight->bounds.min.y,
        fx->r_flashlight->bounds.max.x,
        fx->r_flashlight->bounds.max.y);

    return 0;
}
