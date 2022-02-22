#ifndef PARTICLE_H
#define PARTICLE_H

#define MAX_PARTICLES 1024

#include "shader.h"

struct tagap_entity;

struct timed_f32 { f32 begin, end, now; };

/*
 * particle.h
 *
 * Handles particle systems.  All the particles are rendered in a single vertex
 * buffer, which is pre-initialised with quads, and have their positions, etc.
 * modified each frame.
 */

// List of basic particle types that the engine supports.  Defines the texture
// to use for a particle
enum particle_type
{
    PARTICLE_SMOKE = 0,
    PARTICLE_BEAM,
    PARTICLE_EXPLOSION,
    PARTICLE_TELEPORT,
    PARTICLE_ELECTRIC,
    PARTICLE_BLOOD,
    PARTICLE_BLOOD_MESS,
    PARTICLE_BUBBLES,
    PARTICLE_DEBRIS,
    PARTICLE_GIBS,
    PARTICLE_GLASS,
    PARTICLE_PAPER,
    PARTICLE_RUBBLE,
    PARTICLE_SHELL1,
    PARTICLE_WATER,

    _PARTICLE_COUNT,
};

extern const char *PARTICLE_TEX_NAMES[];

// Used for emitting individual particle
struct particle_props
{
    // Type of particle (determines texture index)
    enum particle_type type;

    // Starting position/velocity of the particle
    vec2s pos, velo;

    // Starting rotation of the particle
    f32 rot;

    // Rotation speed
    f32 rot_speed;

    // Beginning and end size of the particle
    struct timed_f32 size;

    // Beginning and end opacity of particle
    struct timed_f32 opacity;

    // Time in seconds that particle takes to die.
    f32 lifetime;
};

// A single particle used by particle system
struct particle
{
    // Whether this particle is in use
    bool active;

    // Properties of the particle
    struct particle_props props;

    // Time left until particle dies.
    f32 life_remain;
};

// Manages all particles
struct particle_system
{
    // Main renderable that we give to the renderer
    struct renderable *r;

    // Particle pool
    struct particle *pool;

    // Current pool index
    u32 index;

    // Vertex buffer
    struct vertex_ptl *quad_buffer;
    struct quad_ptl
    {
        struct vertex_ptl vertices[4];
    } *quad_ptr;

    // Staging buffer for copies
    VkBuffer staging_buf;
    VmaAllocation staging_buf_alloc;

    // Command buffer for copy operation
    VkCommandBuffer cmdbuf;
    VkFence fence;

    // List of loaded texture indices
    i32 tex_indices[_PARTICLE_COUNT];
};

extern struct particle_system *g_parts;

void particles_init(void);
void particles_deinit(void);
void particles_update(void);
void particle_emit(struct particle_system *, struct particle_props *);

#endif
