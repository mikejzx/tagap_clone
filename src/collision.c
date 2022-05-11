#include "pch.h"
#include "collision.h"
#include "tagap.h"
#include "tagap_entity.h"
#include "tagap_linedef.h"
#include "renderer.h"

/* Get leftmost point */
static inline vec2s
get_lmost(vec2s a, vec2s b) { return a.x < b.x ? a : b; }

/* Get rightmost point */
static inline vec2s
get_rmost(vec2s a, vec2s b) { return a.x < b.x ? b : a; }

/* Get topmost point */
static inline vec2s
get_tmost(vec2s a, vec2s b) { return a.y < b.y ? a : b; }

/* Get bottommost point */
static inline vec2s
get_bmost(vec2s a, vec2s b) { return a.y < b.y ? b : a; }

void
collision_check(struct tagap_entity *e, struct collision_result *c)
{
    // For all moving entities:
    // Check collision with all linedefs in the level
    for (u32 i = 0; i < g_map->linedef_count; ++i)
    {
        struct tagap_linedef *l = &g_map->linedefs[i];

        // These radii are really dodgey (not 100% sure how they should be
        // handled), but seems to work alright for now
        f32 max_radius = max(e->info->offsets[OFFSET_SIZE].x,
            e->info->offsets[OFFSET_SIZE].y);
        f32 radius_y = 0.0f;
        //if (e->info->offsets[OFFSET_SIZE].y > 0.0f)
        //{
            // Note we deliberately use X here (works much better for player)
            radius_y = e->info->offsets[OFFSET_SIZE].x;
#if 0
        }
        else if (e->info->sprite_count)
        {
            radius_y = e->info->offsets[OFFSET_SIZE].x;
            //radius_y = g_vulkan->textures[e->sprites[0]->tex].h / 2.0f;
            //radius_y = 1;
        }
#endif

        vec2s lmost = get_lmost(l->start, l->end),
              rmost = get_rmost(l->start, l->end);

        /*
         * Horizontal line checks
         */
        // Check that our position is in the range of the line
        if (e->position.x >= lmost.x &&
            e->position.x <= rmost.x)
        {
            // Get Y-position of line at the entity's point
            f32 gradient = (rmost.y - lmost.y) /
                (rmost.x - lmost.x);
            f32 shift = lmost.y - gradient * lmost.x;
            f32 y_line = gradient * e->position.x + shift;

            // Use gradient of velocity to see if we get a collision
            // (allows us to come in from underneath floors)
            f32 velo_gradient;
            if (e->velo.x != 0.0f)
            {
                //LOG_DBUG("%.2f vs %.2f", e->velo.y / e->velo.x, gradient);
                velo_gradient = e->velo.y / fabs(e->velo.x);
            }
            else
            {
                velo_gradient = e->velo.y;
            }

            /* Floor collision check */
            if ((l->style == LINEDEF_STYLE_FLOOR ||
                    l->style == LINEDEF_STYLE_PLATE_FLOOR) &&
                !c->below &&
                //e->velo.y <= 0.0f &&
                velo_gradient <= gradient &&
                e->position.y - radius_y <= y_line &&
                e->position.y >= y_line)
            {
                c->below = true;
                c->floor_gradient = gradient;
                c->floor_shift = shift;
                continue;
            }

            /* Ceiling collision check */
            if ((l->style == LINEDEF_STYLE_CEILING ||
                    l->style == LINEDEF_STYLE_PLATE_CEILING) &&
                !c->above &&
                e->position.y + max_radius >= y_line &&
                e->position.y <= y_line)
            {
                c->above = true;
                continue;
            }
        }

        /*
         * Vertical line (wall) checks
         */
        // Make sure that the lines we are looking are indeed vertical and that
        // we are in a Y range capable of colling with them
        if (!(lmost.x == rmost.x)) continue;
        f32 line_y_max, line_y_min;
        if (l->start.y < l->end.y)
        {
            line_y_max = l->end.y;
            line_y_min = l->start.y;
        }
        else
        {
            line_y_max = l->start.y;
            line_y_min = l->end.y;
        }
        if (e->position.y > line_y_max ||
            e->position.y + max_radius < line_y_min)
        {
            continue;
        }

        f32 line_x = lmost.x;

        // Right-facing wall.  We check if the player is colliding from right
        if ((l->style == LINEDEF_STYLE_CEILING ||
                l->style == LINEDEF_STYLE_PLATE_CEILING) &&
            !c->left &&
            e->position.x - e->info->offsets[OFFSET_SIZE].x < line_x &&
            e->position.x >= line_x)
        {
            c->left = true;
            continue;
        }

        // Left-facing wall.  We check if the player is colliding from left
        if ((l->style == LINEDEF_STYLE_FLOOR ||
                l->style == LINEDEF_STYLE_PLATE_FLOOR) &&
            !c->right &&
            e->position.x + e->info->offsets[OFFSET_SIZE].x > line_x &&
            e->position.x <= line_x)
        {
            c->right = true;
            continue;
        }
    }
}

/* Perform 'trace' collision check */
void
collision_check_trace(
    struct tagap_entity *ignore_e,
    vec2s from,
    f32 angle,
    f32 range,
    struct collision_trace_result *result)
{
    u32 i;
    result->hit = false;

    f32 tgrad = tanf(glm_rad(angle));
    f32 x_dir = (angle > 90.0f && angle < 270.0f) ? -1.0f : 1.0f;

    // Check for entity collisions
    for (i = 0; i < g_map->entity_count; ++i)
    {
        struct tagap_entity *e = &g_map->entities[i];
        if (!e->active) continue;
        if (e == ignore_e) continue;

        f32 radius_x = 96.0f, radius_y = 96.0f;

        // Check range
        if (glms_vec2_distance2(from, e->position) > range * range) continue;

        // Check if trace line lies within entity's X-range
        f32 diff_sgn = sign(e->position.x - from.x);
        if (x_dir > 0.0f)
        {
            // We are facing right, so entity must be to right to hit
            if (diff_sgn <= 0.0f) continue;
        }
        else
        {
            if (diff_sgn >= 0.0f) continue;
        }

        // Get trace line Y point at entity location
        f32 y_e = tgrad * (e->position.x - from.x) + from.y;
        //LOG_DBUG("f:[%.2f %.2f] e:[%.2f %.2f] t:[%.2f %.2f] (%s)",
        //    from.x, from.y,
        //    e->position.x, e->position.y,
        //    e->position.x, y_e,
        //    e->info->name);
        if (y_e > e->position.y + radius_y ||
            y_e < e->position.y - radius_y)
        {
            continue;
        }
        //LOG_DBUG("%.2f (%.2f)", y_e, e->position.y);

        // Collided with entity
        vec2s point;
        point.x = e->position.x;
        point.y = y_e;
        result->point = point;
        result->hit = true;
        LOG_DBUG("%.2f %.2f COLLIDE (e) (%s)", point.x, point.y, e->info->name);
        return;
    }
#if 0
    vec2s lmost, rmost, tmost, bmost;

    // Check collisions for all linedefs in the level
    for (i = 0; i < g_map->linedef_count; ++i)
    {
        struct tagap_linedef *l = &g_map->linedefs[i];

        lmost = get_lmost(l->start, l->end),
        rmost = get_rmost(l->start, l->end),
        tmost = get_tmost(l->start, l->end),
        bmost = get_bmost(l->start, l->end);

        // We need to get Y-position at each of the linedef's endpoint's, and
        // check if they lie in the line's range

        f32 lgrad = (rmost.y - lmost.y) / (rmost.x - lmost.x);
        if (rmost.x == lmost.x)
        {
            // Verticel line
            lgrad = 0.0f;
        }
        if (tgrad >= fabs(lgrad))
        {
            // Lines cannot intersect if the gradient is too large, as we only
            // allow intersection from "above" the linedef (i.e. smaller
            // and non-parallel gradient)
            continue;
        }

        // y = m(x - x2) + y2
        f32 y_l = tgrad * (from.x - lmost.x) + from.y,
            y_r = tgrad * (from.x - rmost.x) + from.y;
        f32 y_min = min(y_l, y_r),
            y_max = min(y_l, y_r);

        // Skip if the trace line does not lie within the linedef's range of
        // Y-values
        if (y_min < bmost.y || y_max > tmost.y)
        {
            continue;
        }

        // These lines should produce a collision.  Calculate collision point
        result->hit = true;

        // Find intersection of trace line and linedef equations
        // mx + c = mx + c
        // grad_t * x + from_y = grad_l * x + c_l
        // grad_t * x - grad_l * x = c_l - from_y
        // x = (c_l - from_y) / (grad_t - grad_l)

        // c_linedef = y - mx
        f32 c_linedef = lmost.y - lgrad * lmost.x;

        vec2s point;
        point.x = (c_linedef - from.y) / (tgrad - lgrad);
        point.y = tgrad * point.x + from.y;
        result->point = point;
        LOG_DBUG("%.2f %.2f COLLIDE t:%.2f l:%.2f", point.x, point.y, tgrad, lgrad);
        return;
    }
#endif
}
