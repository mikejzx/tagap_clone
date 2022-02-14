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

void collision_check(struct tagap_entity *, struct collision_result *);

#endif
