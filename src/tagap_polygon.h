#ifndef TAGAP_POLYGON_H
#define TAGAP_POLYGON_H

#define POLYGON_TEX_NAME_MAX 64
#define POLYGON_MAX_POINTS 16

struct tagap_polygon
{
    char tex_name[128];
    int tex_offset_point;
    int tex_is_shaded;

    vec2s points[POLYGON_MAX_POINTS];
    int point_count;
};

#endif
