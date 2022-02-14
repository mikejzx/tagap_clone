#ifndef LINEDEF_H
#define LINEDEF_H

/*
 * tagap_linedef.h
 *
 * TAGAP's line definition structure
 */

enum tagap_linedef_style
{
    // "floor or facing left"
    LINEDEF_STYLE_FLOOR = 0,

    // "ceiling or facing right"
    LINEDEF_STYLE_CEILING = 1,

    // "plate floor"
    LINEDEF_STYLE_PLATE_FLOOR = 2,

    // "plate ceiling"
    LINEDEF_STYLE_PLATE_CEILING = 3,
};

struct tagap_linedef
{
    // Line start/ end positions
    vec2s start, end;

    // Line style
    enum tagap_linedef_style style;
};

#endif
