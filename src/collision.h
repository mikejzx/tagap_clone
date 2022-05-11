#ifndef COLLISION_H
#define COLLISION_H

struct tagap_entity;

/*
 * collision.h
 *
 * Collision detection code
 */

struct collision_result
{
    bool above;
    bool left;
    bool right;
    bool below;
    f32 floor_gradient, floor_shift;
};

struct collision_trace_result
{
    bool hit;
    vec2s point;
};

void collision_check(struct tagap_entity *, struct collision_result *);
void collision_check_trace(struct tagap_entity *,
    vec2s, f32, f32, struct collision_trace_result *);

#endif
