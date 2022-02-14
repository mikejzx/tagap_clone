#include "pch.h"
#include "collision.h"
#include "tagap.h"
#include "tagap_entity.h"
#include "tagap_linedef.h"

void
collision_check(struct tagap_entity *e, struct collision_result *c)
{
    // For all moving entities:
    // Check collision with all linedefs in the level
    for (u32 i = 0; i < g_map->linedef_count; ++i)
    {
        struct tagap_linedef *l = &g_map->linedefs[i];

        f32 max_radius = max(e->info->offsets[OFFSET_SIZE].x,
            e->info->offsets[OFFSET_SIZE].y);

        vec2s leftmost;
        vec2s rightmost;
        if (l->start.x < l->end.x)
        {
            leftmost = l->start;
            rightmost = l->end;
        }
        else
        {
            leftmost = l->end;
            rightmost = l->start;
        }

        /*
         * Horizontal line checks
         */
        // Check that our position is in the range of the line
        if (e->position.x >= leftmost.x &&
            e->position.x <= rightmost.x)
        {
            // Get Y-position of line at the entity's point
            f32 gradient = (rightmost.y - leftmost.y) /
                (rightmost.x - leftmost.x);
            f32 shift = leftmost.y - gradient * leftmost.x;
            f32 y_line = gradient * e->position.x + shift;

            /* Floor collision check */
            if ((l->style == LINEDEF_STYLE_FLOOR ||
                    l->style == LINEDEF_STYLE_PLATE_FLOOR) &&
                !c->below &&
                e->velo.y <= 0.0f &&
                e->position.y - e->info->offsets[OFFSET_SIZE].x <= y_line &&
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
        if (!(leftmost.x == rightmost.x)) continue;
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

        f32 line_x = leftmost.x;

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
