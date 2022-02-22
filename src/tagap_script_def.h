#ifndef TAGAP_SCRIPT_DEF_H
#define TAGAP_SCRIPT_DEF_H

/*
 * tagap_script_def.h
 *
 * TAGAP_Script command definitions
 */

/*
 * Implemented CVAR definitions
 */
enum tagap_cvar_id
{
    CVAR_UNKNOWN = 0,
    CVAR_MAP_TITLE,
    CVAR_MAP_SCHEME,
    CVAR_SND_SONG,
    _CVAR_COUNT,
};

const char *CVAR_NAMES[] =
{
    [CVAR_UNKNOWN]    = "",
    [CVAR_MAP_TITLE]  = "map_title",
    [CVAR_MAP_SCHEME] = "map_scheme",
    [CVAR_SND_SONG]   = "snd_song",
};

CREATE_LOOKUP_FUNC(lookup_tagap_cvar, CVAR_NAMES, _CVAR_COUNT);

// Top-level command names
enum tagap_script_atom_id
{
    ATOM_UNKNOWN = 0,

    /* Definition commands */
    ATOM_CVAR,
    ATOM_LINEDEF,
    ATOM_POLYGON,
    ATOM_POLYPOINT,
    ATOM_POLYGON_END,
    ATOM_THEME,
    ATOM_THEME_END,
    ATOM_COLOUR,
    ATOM_ENVIRONMENT,
    ATOM_DARKNESS,
    ATOM_ENTITY_START,
    ATOM_ENTITY_END,
    ATOM_ENTITY_SET,
    ATOM_AMMO,
    ATOM_CLONE,
    ATOM_SPRITE,
    ATOM_SPRITEVAR,
    ATOM_MOVETYPE,
    ATOM_THINK,
    ATOM_OFFSET,
    ATOM_STAT,
    ATOM_GUNENTITY,
    ATOM_LIGHT,
    ATOM_FLASHLIGHT,
    ATOM_WEAPON,
    ATOM_LAYER,
    ATOM_TRIGGER,
    ATOM_TEXCLONE,

    // (internal) Number of atoms we implement
    _ATOM_COUNT
};

// List of all implemented commands
static const struct tagap_script_command
TAGAP_SCRIPT_COMMANDS[] =
{
    // Unknown command
    [ATOM_UNKNOWN] = { 0 },

    // Used for general variables (e.g. map title)
    [ATOM_CVAR] =
    {
        .name = "CVAR",
        .token_count = 2,
        .tokens =
        {
            // #1: variable name
            { .type = TSCRIPT_TOKEN_LOOKUP, .lookup_func = lookup_tagap_cvar },
            // #2: variable value (string or int)
            { .type = TSCRIPT_TOKEN_STRING, .length = LEVEL_TITLE_MAX },
        }
    },
    // Line definitions
    [ATOM_LINEDEF] =
    {
        .name = "LINEDEF",
        .token_count = 5,
        .tokens =
        {
            // #1: start X point
            { .type = TSCRIPT_TOKEN_INT, },
            // #2: start Y point
            { .type = TSCRIPT_TOKEN_INT, },
            // #3: end X point
            { .type = TSCRIPT_TOKEN_INT, },
            // #4: end Y point
            { .type = TSCRIPT_TOKEN_INT, },
            // #5: style flag as integer
            { .type = TSCRIPT_TOKEN_INT, },
        }
    },
    // Textured polygon definition
    // + STR: texture name (hidden prefix of data/art/textures/)
    // + INT: texture offset
    // + BOOL: texture shading
    [ATOM_POLYGON] =
    {
        .name = "POLYGON",
        .token_count = 3,
        .tokens =
        {
            // #1: texture name (with hidden prefix of data/art/textures/)
            { .type = TSCRIPT_TOKEN_STRING, .length = POLYGON_TEX_NAME_MAX },
            // #2: polygon point to use as texture offset
            { .type = TSCRIPT_TOKEN_INT },
            // #3: whether to shade the texture
            { .type = TSCRIPT_TOKEN_BOOL },
        },
        .sets_mode = true,
        .sets_mode_to = TAGAP_PARSE_POLYGON,
    },
    // Defines point of current polygon
    // + INT: X position
    // + INT: Y position
    [ATOM_POLYPOINT] =
    {
        .name = "POLYPOINT",
        .token_count = 2,
        .tokens =
        {
            // #1: point X position
            { .type = TSCRIPT_TOKEN_FLOAT },
            // #2: point Y position
            { .type = TSCRIPT_TOKEN_FLOAT },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_POLYGON
    },
    // End of polygon definition
    [ATOM_POLYGON_END] =
    {
        .name = "POLYGON_END",
        .token_count = 0,
        .sets_mode = true,
        .sets_mode_to = TAGAP_PARSE_NORMAL,
    },
    // Begin theme definition
    [ATOM_THEME] =
    {
        .name = "THEME",
        .token_count = 1,
        .tokens =
        {
            // #1: theme name
            { .type = TSCRIPT_TOKEN_STRING, .length = THEME_NAME_MAX }
        },
        .sets_mode = true,
        .sets_mode_to = TAGAP_PARSE_THEME,
    },
    // End of theme definition
    [ATOM_THEME_END] =
    {
        .name = "THEME_END",
        .sets_mode = true,
        .sets_mode_to = TAGAP_PARSE_NORMAL,
    },
    // Define theme colour
    [ATOM_COLOUR] =
    {
        .name = "COLOR",
        .token_count = 5,
        .tokens =
        {
            // #1: part of world to affect (0=world, 1=backgrounds)
            { .type = TSCRIPT_TOKEN_BOOL },
            // #2: target of colour shift (0=base, 1=shifted)
            { .type = TSCRIPT_TOKEN_BOOL },
            // #3: colour component (R) from 0-255
            { .type = TSCRIPT_TOKEN_INT },
            // #4: colour component (G) from 0-255
            { .type = TSCRIPT_TOKEN_INT },
            // #5: colour component (B) from 0-255
            { .type = TSCRIPT_TOKEN_INT },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_THEME,
    },
    // Defines environment style for a theme
    [ATOM_ENVIRONMENT] =
    {
        .name = "ENVIRONMENT",
        .token_count = 1,
        .tokens =
        {
            // #1: environment style name
            { .type = TSCRIPT_TOKEN_LOOKUP, .lookup_func = lookup_tagap_env }
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_THEME,
    },
    // Defines theme darkness from 0 (bright) to 4 (pitch black)
    [ATOM_DARKNESS] =
    {
        .name = "ENVIRONMENT",
        .token_count = 2,
        .tokens =
        {
            // #1: base darkness
            { .type = TSCRIPT_TOKEN_INT },
            // #2: shifted darkness
            { .type = TSCRIPT_TOKEN_INT },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_THEME,
    },
    // Start of entity definition
    [ATOM_ENTITY_START] =
    {
        .name = "ENTITY_START",
        .token_count = 1,
        .tokens =
        {
            // #1: entity name
            { .type = TSCRIPT_TOKEN_STRING, .length = ENTITY_NAME_MAX }
        },
        .sets_mode = true,
        .sets_mode_to = TAGAP_PARSE_ENTITY,
    },
    // End of entity definition
    [ATOM_ENTITY_END] =
    {
        .sets_mode = true,
        .sets_mode_to = TAGAP_PARSE_NORMAL,
    },
    // Set entity into the game world
    [ATOM_ENTITY_SET] =
    {
        .name = "ENTITY_SET",
        .token_count = 5,
        .tokens =
        {
            // #1: entity name
            { .type = TSCRIPT_TOKEN_ENTITY },
            // #2: entity X coordinate
            { .type = TSCRIPT_TOKEN_INT },
            // #3: entity Y coordinate
            { .type = TSCRIPT_TOKEN_INT },
            // #4: entity angle in degrees (in case of AI_AIM) or facing as a
            //     boolean
            { .type = TSCRIPT_TOKEN_INT },
            // #5: entity active/inactive state (0=active, 1=inactive)
            { .type = TSCRIPT_TOKEN_BOOL }
        }
    },
    // Set ammo for entity weapon slot
    [ATOM_AMMO] =
    {
        .name = "AMMO",
        .token_count = 2,
        .tokens =
        {
            // #1: weapon slot
            { .type = TSCRIPT_TOKEN_INT },
            // #2: ammo amount
            { .type = TSCRIPT_TOKEN_INT },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_ENTITY,
    },
    // Copy entity info
    [ATOM_CLONE] =
    {
        .name = "CLONE",
        .token_count = 1,
        .tokens =
        {
            // #1: name of entity to clone
            { .type = TSCRIPT_TOKEN_ENTITY },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_ENTITY,
    },
    // Set entity sprite
    [ATOM_SPRITE] =
    {
        .name = "SPRITE",
        .token_count = 6,
        .tokens =
        {
            // #1: loading parameter (unused)
            { .type = TSCRIPT_TOKEN_STRING, .length = 32 },
            // #2: full bright flag
            { .type = TSCRIPT_TOKEN_BOOL },
            // #3: animation style
            { .type = TSCRIPT_TOKEN_LOOKUP, .lookup_func = lookup_tagap_anim },
            // #4: sprite X offset
            { .type = TSCRIPT_TOKEN_INT },
            // #5: sprite Y offset
            { .type = TSCRIPT_TOKEN_INT },
            // #6: sprite name
            { .type = TSCRIPT_TOKEN_STRING, .length = SPRITE_NAME_MAX },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_ENTITY,
    },
    // Set entity sprite variable
    [ATOM_SPRITEVAR] =
    {
        .name = "SPRITEVAR",
        .token_count = 3,
        .tokens =
        {
            // #1: index of sprite
            { .type = TSCRIPT_TOKEN_INT },
            // #2: variable name
            {
                .type = TSCRIPT_TOKEN_LOOKUP,
                .lookup_func = lookup_tagap_spritevar
            },
            // #3: variable value (not used with [toggle] variables)
            {
                .type = TSCRIPT_TOKEN_INT,
                .optional = true
            },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_ENTITY,
    },
    // Set entity move type
    [ATOM_MOVETYPE] =
    {
        .name = "MOVETYPE",
        .token_count = 2,
        .tokens =
        {
            // #1: movement style flag
            {
                .type = TSCRIPT_TOKEN_LOOKUP,
                .lookup_func = lookup_tagap_movetype
            },
            // #2: movement speed
            { .type = TSCRIPT_TOKEN_FLOAT, },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_ENTITY,
    },
    // Set entity AI routine
    [ATOM_THINK] =
    {
        .name = "THINK",
        .token_count = 4,
        .tokens =
        {
            // #1: think mode
            {
                .type = TSCRIPT_TOKEN_LOOKUP,
                .lookup_func = lookup_tagap_think
            },
            // #2: movement speed modifier
            { .type = TSCRIPT_TOKEN_FLOAT, },
            // #3: attack reference
            {
                .type = TSCRIPT_TOKEN_LOOKUP,
                .lookup_func = lookup_tagap_think_attack
            },
            // #4: attack speed
            {
                .type = TSCRIPT_TOKEN_FLOAT,
                // Sometimes not specified (defaults to 0)
                .optional = true,
            },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_ENTITY,
    },
    // Set entity offset
    [ATOM_OFFSET] =
    {
        .name = "OFFSET",
        .token_count = 3,
        .tokens =
        {
            // #1: variable name
            {
                .type = TSCRIPT_TOKEN_LOOKUP,
                .lookup_func = lookup_tagap_offset
            },
            // #2: X offset
            { .type = TSCRIPT_TOKEN_INT, },
            // #3: Y offset
            {
                .type = TSCRIPT_TOKEN_INT,

                // Some scripts don't include the second value for some reason
                .optional = true,
            },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_ENTITY,
    },
    // Sets a value on an entity
    [ATOM_STAT] =
    {
        .name = "STAT",
        .token_count = 2,
        .tokens =
        {
            // #1: variable name
            { .type = TSCRIPT_TOKEN_LOOKUP, .lookup_func = lookup_tagap_stat },
            // #2: value (not used with [toggle] variables)
            {
                .type = TSCRIPT_TOKEN_INT,
                .optional = true,
            },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_ENTITY,
    },
    // Sets entity as weapon model
    [ATOM_GUNENTITY] =
    {
        .name = "GUNENTITY",
        .token_count = 1,
        .tokens =
        {
            // #1: entity to use as weapon model
            { .type = TSCRIPT_TOKEN_ENTITY },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_ENTITY,
    },
    // Light info for an entity
    [ATOM_LIGHT] =
    {
        .name = "LIGHT",
        .token_count = 5,
        .tokens =
        {
            // #1: light radius, as percentage (e.g. 0-100)
            { .type = TSCRIPT_TOKEN_INT },
            // #2: light intensity, as percentage (e.g. 0-100)
            { .type = TSCRIPT_TOKEN_INT },
            // #3: light colour (R)
            { .type = TSCRIPT_TOKEN_INT },
            // #4: light colour (G)
            { .type = TSCRIPT_TOKEN_INT },
            // #5: light colour (B)
            { .type = TSCRIPT_TOKEN_INT },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_ENTITY,
    },
    // Adds flashllight to entity
    [ATOM_FLASHLIGHT] =
    {
        .name = "FLASHLIGHT",
        .token_count = 7,
        .tokens =
        {
            // #1: flashlight origin X-axis offset
            { .type = TSCRIPT_TOKEN_INT },
            // #2: flashlight origin Y-axis offset
            { .type = TSCRIPT_TOKEN_INT },
            // #3: light halo radius (%)
            { .type = TSCRIPT_TOKEN_INT },
            // #4: light beam length (%)
            { .type = TSCRIPT_TOKEN_INT },
            // #5: flashlight colour (R)
            { .type = TSCRIPT_TOKEN_INT },
            // #6: flashlight colour (G)
            { .type = TSCRIPT_TOKEN_INT },
            // #7: flashlight colour (B)
            { .type = TSCRIPT_TOKEN_INT },
        },
        .requires_mode = true,
        .required_mode = TAGAP_PARSE_ENTITY,
    },
    // Defines a weapon slot
    [ATOM_WEAPON] =
    {
        .name = "WEAPON",
        .token_count = 4,
        .tokens =
        {
            // #1: weapon slot index
            { .type = TSCRIPT_TOKEN_INT },
            // #2: primary weapon entity
            { .type = TSCRIPT_TOKEN_ENTITY },
            // #3: secondary weapon entity
            { .type = TSCRIPT_TOKEN_ENTITY },
            // #4: weapon display nmae
            { .type = TSCRIPT_TOKEN_STRING, .length = WEAPON_DISPLAY_NAME_MAX },
        },
    },
    // Defines a layer
    [ATOM_LAYER] =
    {
        .name = "LAYER",
        .token_count = 6,
        .tokens =
        {
            // #1: scroll speed multiplier
            { .type = TSCRIPT_TOKEN_FLOAT },
            // #2: Y-axis offset from top of screen
            { .type = TSCRIPT_TOKEN_FLOAT },
            // #3: X-wise movement speed
            { .type = TSCRIPT_TOKEN_FLOAT },
            // #4: Y-wise movement speed
            { .type = TSCRIPT_TOKEN_FLOAT },
            // #5: texture name (hidden data/art/layers prefix)
            { .type = TSCRIPT_TOKEN_STRING, .length = LAYER_TEX_NAME_MAX },
            // #6: in-game rendering flag (???)
            { .type = TSCRIPT_TOKEN_INT },
        },
    },
    // Defines a trigger region
    [ATOM_TRIGGER] =
    {
        .name = "TRIGGER",
        .token_count = 9,
        .tokens =
        {
            // #1/2: region top left X and Y coordinates
            { .type = TSCRIPT_TOKEN_FLOAT },
            { .type = TSCRIPT_TOKEN_FLOAT },
            // #3/4: region bottom right X and Y coordinates
            { .type = TSCRIPT_TOKEN_FLOAT },
            { .type = TSCRIPT_TOKEN_FLOAT },
            // #5: target index reference or modifer value
            { .type = TSCRIPT_TOKEN_INT },
            // #6: trigger name
            {
                .type = TSCRIPT_TOKEN_LOOKUP,
                .lookup_func = lookup_tagap_trigger
            },
            // #7: trigger string
            { .type = TSCRIPT_TOKEN_STRING, .length = 32 },
            // #8: link class
            {
                .type = TSCRIPT_TOKEN_STRING, .length = 32,
                //.type = TSCRIPT_TOKEN_LOOKUP,
                //.lookup_func = lookup_tagap_link_class
            },
            // #9: additional variable value
            { .type = TSCRIPT_TOKEN_INT, },
        },
    },
    // Defines a texture that is a clone of another
    [ATOM_TEXCLONE] =
    {
        .name = "TEXCLONE",
        .token_count = 3,
        .tokens =
        {
            // #1: name of clone
            { .type = TSCRIPT_TOKEN_STRING, .length = TEXCLONE_NAME_MAX },
            // #2: name of texture to clone
            { .type = TSCRIPT_TOKEN_STRING, .length = TEXCLONE_NAME_MAX },
            // #3: bright flag toggle
            { .type = TSCRIPT_TOKEN_BOOL },
        },
    },
};

/*
 * Lookup a command by name
 */
static inline struct tagap_script_command
tagap_script_lookup_command(
    const char *a,
    enum tagap_script_atom_id *atom)
{
    for (enum tagap_script_atom_id i = ATOM_UNKNOWN + 1;
        i < _ATOM_COUNT;
        ++i)
    {
        if (strcmp(TAGAP_SCRIPT_COMMANDS[i].name, a) == 0)
        {
            if (atom) *atom = i;
            return TAGAP_SCRIPT_COMMANDS[i];
        }
    }

    // Could not find command
    if (atom) *atom = 0;
    return TAGAP_SCRIPT_COMMANDS[0];
}

#endif
