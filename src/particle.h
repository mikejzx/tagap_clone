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
    u32 tex_index;

    // Starting position of the particle
    vec2s pos;

    // Initial velocity angle of particle (deg)
    f32 dir;

    // Initial speed of particle
    f32 speed;

    // Starting rotation of the particle
    f32 rot;

    // Rotation speed
    f32 rot_speed;

    // Beginning and end size of the particle
    union
    {
        struct timed_f32 size;
        struct timed_f32 size_x;
    };
    struct timed_f32 size_y;
    bool independent_sizes; // Whether X/Y are seperate

    // Flip texture along X
    bool flip_x;

    // Set to 0,0 for centred particle
    vec2s pivot_bias;

    // Beginning/end colours of particle
    struct
    {
        vec4s begin, end, now;
    } colour;

    bool vertex_colour_muls;
    vec4s vertex_colours[4];

    // Pre-computed particle death point (e.g. for faking collision for use
    // with bullets/beams).  For bullets this point is the point where trace
    // attack collides
    bool has_precise_endpoint;
    vec2s precise_endpoint;

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

    // Particle velocity
    vec2s velo;

    // Time left until particle dies.
    f32 life_remain;

    // For 'precise endpoint' stuff
    vec2s old_diff_sgn;
    bool old_diff_sgn_init;

    // Whether to die on next loop
    bool die_next;
};

struct quad_ptl
{
    struct vertex_ptl vertices[4];
};

// Manages all particles
struct particle_system
{
    // Particle pool
    struct particle *pool;

    // Current pool index
    u32 index;

    // Vertex buffer
    struct particle_frame
    {
        struct vbuffer vb;
        struct vertex_ptl *quad_buffer;
        struct quad_ptl *quad_ptr;

        u32 index_count;
    } *frames;
    u32 frame_count;

    // Single index buffer for all frames
    struct ibuffer ib;

    // List of loaded texture indices
    i32 tex_indices[_PARTICLE_COUNT];
};

extern struct particle_system *g_parts;

void particles_init(void);
void particles_deinit(void);
void particles_update(void);
void particles_update_frame(u32);
void particle_emit(struct particle_props *);

#endif
