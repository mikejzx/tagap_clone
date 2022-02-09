#include "pch.h"
#include "tagap.h"
#include "tagap_anim.h"
#include "tagap_entity.h"
#include "tagap_linedef.h"
#include "tagap_polygon.h"
#include "tagap_script.h"

// Top-level token
enum atom_id
{
    ATOM_UNKNOWN = 0,

    /*
     * Definition commands
     */

    // Used for general variables (e.g. map title)
    ATOM_CVAR,

    // Line definitions
    // + INT: start X point
    // + INT: start Y point
    // + INT: end X point
    // + INT: end Y point
    // + INT: style flag
    ATOM_LINEDEF,

    // Used to state beginning of a texture polygon definition
    // + STR: texture name (hidden prefix of data/art/textures/)
    // + INT: texture offset
    // + BOOL: texture shading
    ATOM_POLYGON,

    // Defines point of current polygon
    // + INT: X position
    // + INT: Y position
    ATOM_POLYPOINT,

    // Indicates end of polygon definition
    ATOM_POLYGON_END,

    // Adds entity into the level
    // STR: entity name
    // INT: entity X coordinates
    // INT: entity Y coordinates
    // INT: entity angle (in case of AI_AIM) or facing
    // BOOL: activity state (0=active, 1=inactive)
    ATOM_ENTITY_SET,

    /*
     * Entity commands
     */

    // Start of entity definition
    // + STR: name of entity
    ATOM_ENTITY_START,

    // End of entity definition
    ATOM_ENTITY_END,

    // Clones all info of existing entity
    // STR: name of entity to clone
    ATOM_CLONE,

    // Defines a sprite for an entity
    // + STR: loading parameter (STATIC/DYNAMIC)
    // + BOOL: full bright flag
    // + STR: animation style
    // + INT: sprite X offset
    // + INT: sprite Y offset
    // + STR: sprite name
    ATOM_SPRITE,

    // Additional sprite variables
    // + INT: index of sprite
    // + STR: variable name
    // + INT: value (not used with [toggle] variables)
    ATOM_SPRITEVAR,

    // Defines AI routine used for entity
    // + STR: main routine reference
    // + FLOAT: speed modifier
    // + STR: attack AI reference
    // + FLOAT: delay between attacks
    ATOM_THINK,

    // Sets entity move type
    // + STR: movement style flag
    // + FLOAT: movement speed
    ATOM_MOVETYPE,

    // Defines entity sound
    // + STR: sound event
    // + STR: sample name
    ATOM_SOUND,

    // Defines an entity value
    // + STR: variable name
    // + INT: value (not used with [toggle] variables)
    ATOM_STAT,

    // (internal) Number of atoms we implement
    ATOM_COUNT
};

static const char *ATOM_NAMES[] =
{
    [ATOM_UNKNOWN]      = "",
    [ATOM_CVAR]         = "CVAR",
    [ATOM_LINEDEF]      = "LINEDEF",
    [ATOM_POLYGON]      = "POLYGON",
    [ATOM_POLYPOINT]    = "POLYPOINT",
    [ATOM_POLYGON_END]  = "POLYGON_END",
    [ATOM_ENTITY_SET]   = "ENTITY_SET",
    [ATOM_ENTITY_START] = "ENTITY_START",
    [ATOM_ENTITY_END]   = "ENTITY_END",
    [ATOM_CLONE]        = "CLONE",
    [ATOM_SPRITE]       = "SPRITE",
    [ATOM_SPRITEVAR]    = "SPRITEVAR",
    [ATOM_THINK]        = "THINK",
    [ATOM_MOVETYPE]     = "MOVETYPE",
    [ATOM_SOUND]        = "SOUND",
    [ATOM_STAT]         = "STAT",
};

i32
tagap_script_run(const char *fpath)
{
#if DEBUG
    assert(sizeof(ATOM_NAMES) / sizeof(const char *) == ATOM_COUNT);
#endif

    FILE *fp = fopen(fpath, "r");
    if (!fp)
    {
        LOG_ERROR("[tagap_script] failed to read script '%s' (%d)",
            fpath, errno);
        return -1;
    }

    LOG_INFO("running script '%s'", fpath);

    // Handy shorthands
    struct level *lvl = &g_state.l.map;

    enum parsing_mode
    {
        TAGAP_PARSE_NORMAL = 0,
        TAGAP_PARSE_POLYGON = 1,
        TAGAP_PARSE_ENTITY = 2,
    } cur_parse_mode = TAGAP_PARSE_NORMAL;

    // Read the script file, line by line
    size_t len = 0, ltmp, line_num = 0;
    char *line_tmp = malloc(256 * sizeof(char));
    for (char *line = NULL; (len = getline(&line, &ltmp, fp)) != -1; ++line_num)
    {
        if (len < 1 || line[0] == '/') continue;

        // Strip newline
        if (line[len - 1] == '\n') line[len - 2] = '\0';

        // Hacky fix to make sure strtok doesn't ruin the original line
        strcpy(line_tmp, line);

        enum atom_id atom = ATOM_UNKNOWN;

        // Parse all the tokens in the line
        static const char *DELIM = " ";
    #define TOK_NEXT strtok(NULL, DELIM)
    #define TOK_IS(t) (strcmp(token, (t)) == 0)
        for (char *token = strtok(line_tmp, DELIM);
            token != NULL;
            token = TOK_NEXT)
        {
            if (!atom)
            {
                // First token is the atom ID
                for (enum atom_id a = 0;
                    a < ATOM_COUNT;
                    ++a)
                {
                    // Lookup atom ID from string
                    if (!TOK_IS(ATOM_NAMES[a])) continue;
                    atom = a;

                    // Make sure that the atom is allowed in current parse mode
                    switch (cur_parse_mode)
                    {
                    // Parsing POLYGONs
                    case TAGAP_PARSE_POLYGON:
                    {
                        if (atom == ATOM_POLYGON_END) goto polygon_end;
                        if (atom != ATOM_POLYPOINT)
                        {
                            LOG_ERROR(
                                "[tagap_script] expected POLYPOINT "
                                "or POLYGON_END (got %s) at %s:%d",
                                ATOM_NAMES[a], fpath, line_num);
                            goto next_line;
                        }
                    } break;

                    // Parsing an ENTITY
                    case TAGAP_PARSE_ENTITY:
                    {
                        if (atom == ATOM_ENTITY_END) goto entity_end;
                    } break;

                    // Normal parsing mode
                    case TAGAP_PARSE_NORMAL:
                    default: break;
                    }

                    break;
                }
                continue;
            }

            switch(atom)
            {
            case ATOM_CVAR:
            {
                if (TOK_IS("map_title"))
                {
                    token = TOK_NEXT;
                    LOG_INFO("[tagap_script] map: title '%s'", token);

                    if (strlen(token) >= LEVEL_TITLE_MAX)
                    {
                        LOG_ERROR(
                            "[tagap_script] map: title is too long: '%s'.  "
                            "Length must not exceed %d chars",
                            token, LEVEL_TITLE_MAX);
                        strcpy(lvl->title, "(null)");
                        break;
                    }
                    strcpy(lvl->title, token);

                    break;
                }
                if (TOK_IS("snd_song"))
                {
                    token = TOK_NEXT;
                    LOG_INFO("[tagap_script] map: song is '%s'", token);

                    // TODO soundtrack
                    break;
                }

            } break;

            // Line definitions
            case ATOM_LINEDEF:
            {
                if (lvl->linedef_count + 1 >= LEVEL_MAX_LINEDEFS)
                {
                    LOG_ERROR("[tagap_script] linedef limit (%d) exceeded",
                        LEVEL_MAX_LINEDEFS);
                    goto next_line;
                }

                struct tagap_linedef linedef;
                if (sscanf(line, "LINEDEF %f %f %f %f %d",
                    &linedef.start.x, &linedef.start.y,
                    &linedef.end.x, &linedef.end.y,
                    (i32 *)&linedef.style) != 5)
                {
                    LOG_ERROR("[tagap_script] invalid linedef (%s:%d)",
                        fpath, line_num);
                    goto next_line;
                }
                lvl->linedefs[lvl->linedef_count++] = linedef;

            #if 0
                LOG_DBUG("[tagap_script] new linedef: "
                    "[%.0f, %.0f] to [%.0f %.0f], style %d",
                    linedef.start.x, linedef.start.y,
                    linedef.end.x, linedef.end.y,
                    linedef.style);
            #endif

            } goto next_line;

            // Begin textured polygon definition
            case ATOM_POLYGON:
            {
                cur_parse_mode = TAGAP_PARSE_POLYGON;

                if (strlen(token) >= POLYGON_TEX_NAME_MAX)
                {
                    LOG_ERROR(
                        "[tagap_script] POLYGON title is too long: '%s'.  "
                        "Length must not exceed %d chars",
                        token, POLYGON_TEX_NAME_MAX);
                    cur_parse_mode = TAGAP_PARSE_NORMAL;
                    goto next_line;
                }

                // Copy texture name
                struct tagap_polygon *cur_poly =
                    &lvl->polygons[lvl->polygon_count++];
                strcpy(cur_poly->tex_name, token);
                token = TOK_NEXT;
                if (!token)
                {
                    LOG_ERROR("[tagap_script] POLYGON: missing arguments "
                        "on %s:%d",
                        fpath, line_num);
                    goto next_line;
                }
                cur_poly->tex_offset_point = atoi(token);
                token = TOK_NEXT;
                if (!token)
                {
                    LOG_ERROR("[tagap_script] POLYGON: missing arguments "
                        "on %s:%d",
                        fpath, line_num);
                    goto next_line;
                }
                cur_poly->tex_is_shaded = !!atoi(token);
            } goto next_line;

            // Define point on current polygon
            case ATOM_POLYPOINT:
            {
                // Don't exceed maximum polygons
                if (lvl->polygon_count + 1 >= LEVEL_MAX_POLYGONS)
                {
                    LOG_ERROR("[tagap_script] polygon limit (%d) exceeded",
                        LEVEL_MAX_POLYGONS);
                    goto next_line;
                }

                struct tagap_polygon *cur_poly =
                    &lvl->polygons[lvl->polygon_count - 1];
                vec2s *point = &cur_poly->points[cur_poly->point_count++];
                if (sscanf(line, "POLYPOINT %f %f",
                    &point->x, &point->y) != 2)
                {
                    LOG_ERROR("[tagap_script] invalid polypoint (%s:%d)",
                        fpath, line_num);
                    goto next_line;
                }
            } goto next_line;

            // End of textured polygon definition
            case ATOM_POLYGON_END:
            polygon_end:
            {
                cur_parse_mode = TAGAP_PARSE_NORMAL;
                struct tagap_polygon *cur_poly =
                    &lvl->polygons[lvl->polygon_count - 1];

                if (cur_poly->point_count < 3)
                {
                    LOG_ERROR("[tagap_script] invalid polygon "
                        "(needs at least 3 points) (%s:%d)",
                        fpath, line_num);
                    goto next_line;
                }

            #if 0
                LOG_DBUG("END POLYGON: %d points", cur_poly->point_count);
                for (i32 i = 0; i < cur_poly->point_count; ++i)
                {
                    LOG_DBUG("  PT: [%.2f %.2f]",
                        cur_poly->points[i].x, cur_poly->points[i].y);
                }
            #endif
            } goto next_line;

            // Begin entity info definition
            case ATOM_ENTITY_START:
            {
                // Make sure we don't exceed entity list
                if (g_state.l.entity_info_count + 1 >= GAME_ENTITY_INFO_LIMIT)
                {
                    LOG_ERROR("[tagap_script] entity limit (%d) exceeded",
                        GAME_ENTITY_INFO_LIMIT);
                    goto next_line;
                }

                i32 index = g_state.l.entity_info_count;
                struct tagap_entity_info *e =
                    &g_state.l.entity_infos[index];
                memset(e, 0, sizeof(struct tagap_entity_info));

                if (strlen(token) >= ENTITY_NAME_MAX)
                {
                    LOG_ERROR("[tagap_script] entity name is too "
                        "long (max %d) in (%s:%d)",
                        ENTITY_NAME_MAX, fpath, line_num);
                    goto next_line;
                }

                cur_parse_mode = TAGAP_PARSE_ENTITY;
                ++g_state.l.entity_info_count;

                // Set the entity name
                strcpy(e->name, token);

                //LOG_DBUG("NEW ENTITY '%s'", cur_entity->name);
            } goto next_line;

            // End entity definition
            case ATOM_ENTITY_END:
            entity_end:
            {
                // Reset to normal parsing mode
                cur_parse_mode = TAGAP_PARSE_NORMAL;
            } goto next_line;

            // Add entity into level
            case ATOM_ENTITY_SET:
            {
                // Don't exceed limit
                if (lvl->entity_count + 1 >= LEVEL_MAX_ENTITIES)
                {
                    LOG_ERROR("[tagap_script] level entity limit (%d) exceeded",
                        LEVEL_MAX_ENTITIES);
                    goto next_line;
                }

                // Get entity info from given name
                struct tagap_entity_info *ei = NULL;
                for (u32 i = 0; i < g_state.l.entity_info_count; ++i)
                {
                    // Find the entity with specified name
                    if (strcmp(g_state.l.entity_infos[i].name, token) != 0)
                        continue;

                    ei = &g_state.l.entity_infos[i];
                    break;
                }
                if (!ei)
                {
                    LOG_ERROR("[tagap_script] ENTITY_SET: "
                        "no entity with name '%s'", token);
                    goto next_line;
                }
                //LOG_DBUG("[tagap_script] adding entity (%s)", token);

                struct tagap_entity e;
                e.info = ei;

                // X coordinate
                token = TOK_NEXT;
                e.position.x = (f32)atoi(token);

                // Y coordinate
                token = TOK_NEXT;
                e.position.y = (f32)atoi(token);

                // Entity angle or facing
                token = TOK_NEXT;
                e.aim_angle = (int)atoi(token);

                lvl->entities[lvl->entity_count++] = e;
            } goto next_line;

            // Copy entity info
            case ATOM_CLONE:
            {
                if (cur_parse_mode != TAGAP_PARSE_ENTITY)
                {
                    LOG_ERROR("[tagap_script] CLONE: not on entity");
                    goto next_line;
                }

                struct tagap_entity_info *to_copy = NULL;
                for (u32 i = 0; i < g_state.l.entity_info_count; ++i)
                {
                    // Find the entity with specified name
                    if (strcmp(g_state.l.entity_infos[i].name, token) != 0)
                        continue;

                    to_copy = &g_state.l.entity_infos[i];
                    break;
                }
                if (!to_copy)
                {
                    LOG_WARN("[tagap_script] CLONE: "
                        "no entity with name '%s'", token);
                    goto next_line;
                }

                // Get current entity
                struct tagap_entity_info *e =
                    &g_state.l.entity_infos[g_state.l.entity_info_count - 1];
                entity_info_clone(e, to_copy, true);
            } goto next_line;

            // Define sprite on entity
            case ATOM_SPRITE:
            {
                if (cur_parse_mode != TAGAP_PARSE_ENTITY)
                {
                    LOG_ERROR("[tagap_script] SPRITE: not on entity");
                    goto next_line;
                }

                // Get current entity
                struct tagap_entity_info *e =
                    &g_state.l.entity_infos[g_state.l.entity_info_count - 1];

                // Check bounds
                i32 sprite_index = e->sprite_info_count;
                if (sprite_index + 1 >= ENTITY_MAX_SPRITES)
                {
                    goto next_line;
                }

                struct sprite_info info;

                // Loading parameter (ignored)
                token = TOK_NEXT;

                // Bright flag
                info.bright = !!atoi(token);
                token = TOK_NEXT;

                // Animation
                info.anim = lookup_tagap_anim(token);
                token = TOK_NEXT;

                // Offset
                info.offset.x = (f32)atoi(token);
                token = TOK_NEXT;
                info.offset.y = (f32)atoi(token);
                token = TOK_NEXT;

                // Name
                if (strlen(token) > SPRITE_NAME_MAX)
                {
                    LOG_ERROR("[tagap_script] sprite name is too "
                        "long (max %d) in (%s:%d)",
                        SPRITE_NAME_MAX, fpath, line_num);
                    goto next_line;
                }
                strcpy(info.name, token);

                // Set the sprite info
                e->sprite_infos[sprite_index] = info;
                ++e->sprite_info_count;
                //LOG_DBUG("%d", e->sprite_info_count);
            } goto next_line;

            // Defines entity AI routine
            case ATOM_THINK:
            {
                if (cur_parse_mode != TAGAP_PARSE_ENTITY)
                {
                    LOG_ERROR("[tagap_script] THINK: not on entity");
                    goto next_line;
                }

                // Get current entity
                struct tagap_entity_info *e =
                    &g_state.l.entity_infos[g_state.l.entity_info_count - 1];

                // Get mode and speed modifier
                e->think.mode = lookup_tagap_think(token);
                token = TOK_NEXT;
                e->think.speed_mod = (f32)atof(token);

                // Skip other parameters as we don't care about them right now
            } goto next_line;

            // Skip unimplemented atoms
            default:
                break;
            }
        }

    next_line:
        continue;
    }
    free(line_tmp);

    fclose(fp);

    return 0;
}
