#include "pch.h"
#include "tagap.h"
#include "tagap_anim.h"
#include "tagap_entity.h"
#include "tagap_linedef.h"
#include "tagap_sprite.h"
#include "tagap_polygon.h"
#include "tagap_script.h"
#include "tagap_theme.h"

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

    // Defines a weapon slot
    // INT: weapon slot ID
    // STR: primary weapon entity name
    // STR: secondary weapon entity name
    // STR: weapon display name
    ATOM_WEAPON,

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

    // Sets an offset value
    // + STR: variable name
    // + INT x offset
    // + INT y offset
    ATOM_OFFSET,

    // Defines entity sound
    // + STR: sound event
    // + STR: sample name
    ATOM_SOUND,

    // Defines an entity value
    // + STR: variable name
    // + INT: value (not used with [toggle] variables)
    ATOM_STAT,

    /*
     * Theme commands
     */
    // Start theme
    // STR: theme name
    ATOM_THEME,
    ATOM_THEME_END,

    // Define theme colour
    // BOOL: part of world to affect (0=world, 1=backgrounds)
    // BOOL: target of colour shift (0=base, 1=shifted)
    // BYTE x3: colours in RGB format
    ATOM_COLOUR,

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
    [ATOM_WEAPON]       = "WEAPON",
    [ATOM_ENTITY_SET]   = "ENTITY_SET",
    [ATOM_ENTITY_START] = "ENTITY_START",
    [ATOM_ENTITY_END]   = "ENTITY_END",
    [ATOM_CLONE]        = "CLONE",
    [ATOM_SPRITE]       = "SPRITE",
    [ATOM_SPRITEVAR]    = "SPRITEVAR",
    [ATOM_THINK]        = "THINK",
    [ATOM_MOVETYPE]     = "MOVETYPE",
    [ATOM_OFFSET]       = "OFFSET",
    [ATOM_SOUND]        = "SOUND",
    [ATOM_STAT]         = "STAT",
    [ATOM_THEME]        = "THEME",
    [ATOM_THEME_END]    = "THEME_END",
    [ATOM_COLOUR]       = "COLOR",
};

/*
 * TODO: seperate command parsing to another function and allow running
 *       scripts via strings.  store parser state in some static struct or
 *       something (make sure to only allow 1 script to run at a time)
 */
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
        TAGAP_PARSE_POLYGON,
        TAGAP_PARSE_ENTITY,
        TAGAP_PARSE_THEME,
    } cur_parse_mode = TAGAP_PARSE_NORMAL;

    // Read the script file, line by line
    size_t len = 0, ltmp, line_num = 1;
    char *line_tmp = malloc(256 * sizeof(char));
    for (char *line = NULL; (len = getline(&line, &ltmp, fp)) != -1; ++line_num)
    {
        if (len < 1 || line[0] == '/') continue;

        // Strip newline
        if (line[len - 1] == '\n') line[len - 2] = '\0';

        // Hacky fix to make sure strtok doesn't ruin the original line
        strcpy(line_tmp, line);

        enum atom_id atom = ATOM_UNKNOWN;

    #define CUR_ENTITY_INFO \
        &g_state.l.entity_infos[g_state.l.entity_info_count - 1]

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

                    // Parsing a THEME
                    case TAGAP_PARSE_THEME:
                    {
                        if (atom == ATOM_THEME_END) goto theme_end;
                    } break;

                    // Normal parsing mode
                    case TAGAP_PARSE_NORMAL:
                    default: break;
                    }

                    break;
                }

                // Unidentified atom, skip to next line
                if (!atom) goto next_line;

                // We got an atom, continue parsing this line
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
                if (TOK_IS("map_scheme"))
                {
                    token = TOK_NEXT;
                    LOG_INFO("[tagap_script] map: scheme is '%s'", token);

                    // Find theme with this name
                    for (u32 i = 0; i < g_state.l.theme_info_count; ++i)
                    {
                        if (strcmp(token, g_state.l.theme_infos[i].name) == 0)
                        {
                            lvl->theme = &g_state.l.theme_infos[i];
                            break;
                        }
                    }
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

            // Define weapon slot
            case ATOM_WEAPON:
            {
                if (!token)
                {
                    LOG_ERROR("[tagap_script] WEAPON: missing slot (%s:%d)",
                        fpath, line_num);
                    goto next_line;
                }
                i32 slot = (i32)atoi(token);
                if (slot < 0 || slot >= WEAPON_DISPLAY_NAME_MAX)
                {
                    LOG_ERROR("[tagap_script] WEAPON: invalid slot %d (%s:%d)",
                        slot, fpath, line_num);
                    goto next_line;
                }
                struct tagap_weapon *weap = &g_state.l.weapons[slot];

                // Get primary entity
                token = TOK_NEXT;
                if (!token)
                {
                    LOG_ERROR("[tagap_script] WEAPON: missing primary (%s:%d)",
                        fpath, line_num);
                    goto next_line;
                }
                weap->primary = NULL;
                for (u32 i = 0; i < g_state.l.entity_info_count; ++i)
                {
                    struct tagap_entity_info *e = &g_state.l.entity_infos[i];
                    if (strcmp(e->name, token) == 0)
                    {
                        weap->primary  = e;
                        break;
                    }
                }
                if (!weap->primary)
                {
                    LOG_WARN("[tagap_script] WEAPON: can't find "
                        "primary entity '%s'", token);
                }

                // Get secondary entity
                token = TOK_NEXT;
                if (!token)
                {
                    LOG_ERROR("[tagap_script] WEAPON: missing "
                        "secondary (%s:%d)", fpath, line_num);
                    goto next_line;
                }
                weap->secondary = NULL;
                for (u32 i = 0; i < g_state.l.entity_info_count; ++i)
                {
                    struct tagap_entity_info *e = &g_state.l.entity_infos[i];
                    if (strcmp(e->name, token) == 0)
                    {
                        weap->secondary  = e;
                        break;
                    }
                }
                if (!weap->secondary)
                {
                    LOG_WARN("[tagap_script] WEAPON: can't find "
                        "secondary entity '%s'", token);
                }

                // Get display name
                token = TOK_NEXT;
                if (!token)
                {
                    LOG_WARN("[tagap_script] WEAPON: missing "
                        "display name (%s:%d)", fpath, line_num);
                    strcpy(g_state.l.weapons[slot].display_name, "(null)");
                }
                else if (strlen(token) >= WEAPON_DISPLAY_NAME_MAX)
                {
                    LOG_ERROR("[tagap_script] WEAPON: name '%s' too long",
                        token);
                    strcpy(g_state.l.weapons[slot].display_name, "(null)");
                }
                else
                {
                    // Copy display name
                    strcpy(g_state.l.weapons[slot].display_name, token);
                }
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

                u32 index = g_state.l.entity_info_count;
                struct tagap_entity_info *e = &g_state.l.entity_infos[index];
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
                memset(&e, 0, sizeof(struct tagap_entity));
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

                // Activity state (unimplemented)
                //token = TOK_NEXT;
                //e.active = !!atoi(token);

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
                struct tagap_entity_info *e = CUR_ENTITY_INFO;
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

                // Get current entity to set sprite on
                struct tagap_entity_info *e = CUR_ENTITY_INFO;

                // Check bounds of entity sprite list
                i32 spr_entity_index = e->sprite_count;
                if (spr_entity_index + 1 >= ENTITY_MAX_SPRITES)
                {
                    LOG_ERROR("[tagap_script] SPRITE: too many sprites (%s:%d)",
                        fpath, line_num);
                    goto next_line;
                }

                // Temporary sprite for us to work with here
                struct tagap_entity_sprite spr_entity;
                memset(&spr_entity, 0, sizeof(struct tagap_entity_sprite));

                // Loading parameter (ignored)
                token = TOK_NEXT;

                // Bright flag
                spr_entity.bright = !!atoi(token);
                token = TOK_NEXT;

                // Animation
                spr_entity.anim = lookup_tagap_anim(token);
                token = TOK_NEXT;

                // Offset
                spr_entity.offset.x = (f32)atoi(token);
                token = TOK_NEXT;
                spr_entity.offset.y = (f32)atoi(token);
                token = TOK_NEXT;

                // Sprite name
                if (token == NULL)
                {
                    LOG_ERROR("[tagap_script] SPRITE: missing name in (%s:%d)",
                        fpath, line_num);
                    goto next_line;
                }
                if (strlen(token) > SPRITE_NAME_MAX)
                {
                    LOG_ERROR("[tagap_script] SPRITE: name is too "
                        "long (max %d) in (%s:%d)",
                        SPRITE_NAME_MAX, fpath, line_num);
                    goto next_line;
                }

                // Try find the sprite in global list
                struct tagap_sprite_info *info = NULL;
                for (u32 i = 0; i < g_state.l.sprite_info_count; ++i)
                {
                    if (strcmp(g_state.l.sprite_infos[i].name, token) == 0)
                    {
                        info = &g_state.l.sprite_infos[i];
                        //LOG_DBUG("reuse %s at %d", token, spr_global_index);
                        break;
                    }
                }
                if (info == NULL)
                {
                    // Sprite has not been added to the list yet.  Add it now
                    i32 spr_global_index = g_state.l.sprite_info_count;

                    // Check that the global sprite list isn't full
                    if (spr_global_index + 1 >= GAME_SPRITE_INFO_LIMIT)
                    {
                        LOG_ERROR("[tagap_script] SPRITE: info limit (%d) "
                            "reached", GAME_SPRITE_INFO_LIMIT);
                        goto next_line;
                    }
                    ++g_state.l.sprite_info_count;
                    //LOG_DBUG("added %s to %d", token, spr_global_index);

                    // Get the info and zero it out
                    info = &g_state.l.sprite_infos[spr_global_index];
                    memset(info, 0, sizeof(struct tagap_sprite_info));
                    strcpy(g_state.l.sprite_infos[spr_global_index].name,
                        token);
                }

                // And finally copy the info over to the entity
                spr_entity.info = info;
                e->sprites[spr_entity_index] = spr_entity;
                ++e->sprite_count;
            } goto next_line;

            // Defines sprite variable for entity sprite
            case ATOM_SPRITEVAR:
            {
                if (cur_parse_mode != TAGAP_PARSE_ENTITY)
                {
                    LOG_ERROR("[tagap_script] SPRITE: not on entity");
                    goto next_line;
                }

                // Get current entity
                struct tagap_entity_info *e = CUR_ENTITY_INFO;

                // 1st arg: sprite index
                i32 spr_index = (i32)atoi(token);
                token = TOK_NEXT;

                // Get sprite
                if (spr_index < 0 || spr_index >= ENTITY_MAX_SPRITES)
                {
                    LOG_ERROR("[tagap_script] SPRITEVAR: invalid index");
                    goto next_line;
                }

                // 2nd arg: variable name
                enum tagap_spritevar_id id = lookup_tagap_spritevar(token);

                // Optional 3rd arg: variable value
                token = TOK_NEXT;
                if (token != NULL)
                {
                    // 3rd arg: variable value
                    e->sprites[spr_index].vars[id] = (i32)atoi(token);
                }
                else
                {
                    // Set value to an initial true value for toggled variables
                    e->sprites[spr_index].vars[id] = 1;
                }
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
                struct tagap_entity_info *e = CUR_ENTITY_INFO;

                // Get mode, speed modifier, (skip attack reference), and
                // attack speed
                e->think.mode = lookup_tagap_think(token);
                token = TOK_NEXT;
                e->think.speed_mod = (f32)atof(token);
                token = TOK_NEXT;
                // ... skip
                token = TOK_NEXT;
                if (token)
                {
                    e->think.attack_speed = (f32)atof(token);
                }
                else
                {
                    LOG_WARN("[tagap_script] THINK: missing attack speed %s:%d",
                        fpath, line_num);
                    e->think.attack_speed = 1.0f;
                }

                // Skip other parameters as we don't care about them right now
            } goto next_line;

            // Defines entity move type
            case ATOM_MOVETYPE:
            {
                if (cur_parse_mode != TAGAP_PARSE_ENTITY)
                {
                    LOG_ERROR("[tagap_script] MOVETYPE: not on entity");
                    goto next_line;
                }

                // Get current entity
                struct tagap_entity_info *e = CUR_ENTITY_INFO;

                // Get mode and speed modifier
                e->move.type = lookup_tagap_movetype(token);
                token = TOK_NEXT;
                e->move.speed = (f32)atof(token);

                // Skip other parameters as we don't care about them right now
            } goto next_line;

            // Defines an offset variable for an entity
            case ATOM_OFFSET:
            {
                if (cur_parse_mode != TAGAP_PARSE_ENTITY)
                {
                    LOG_ERROR("[tagap_script] OFFSET: not on entity");
                    goto next_line;
                }

                // Get current entity
                struct tagap_entity_info *e = CUR_ENTITY_INFO;

                // Get offset ID
                enum tagap_entity_offset_id id = lookup_tagap_offset(token);

                // Read X value
                token = TOK_NEXT;
                if (token == NULL)
                {
                    LOG_ERROR("[tagap_script] OFFSET: missing X token");
                    e->offsets[id].x = 0.0f;
                    goto next_line;
                }
                e->offsets[id].x = (f32)atoi(token);

                // Read Y value
                token = TOK_NEXT;
                if (token == NULL)
                {
                    LOG_WARN("[tagap_script] OFFSET: missing Y token");
                    e->offsets[id].y = 0.0f;
                    goto next_line;
                }
                e->offsets[id].y = (f32)atoi(token);
            } goto next_line;

            // Defines STAT value for entity
            case ATOM_STAT:
            {
                if (cur_parse_mode != TAGAP_PARSE_ENTITY)
                {
                    LOG_ERROR("[tagap_script] STAT: not on entity");
                    goto next_line;
                }

                // Get current entity
                struct tagap_entity_info *e = CUR_ENTITY_INFO;

                // Read stat ID and value
                enum tagap_entity_stat_id id = lookup_tagap_stat(token);
                token = TOK_NEXT;
                if (token) e->stats[id] = (i32)atoi(token);
            } goto next_line;

            // Begin theme definition
            case ATOM_THEME:
            {
                // Make sure we don't exceed theme limit
                if (g_state.l.theme_info_count + 1 >= GAME_THEME_INFO_LIMIT)
                {
                    LOG_ERROR("[tagap_script] THEME: limit (%d) exceeded",
                        GAME_THEME_INFO_LIMIT);
                    goto next_line;
                }

                u32 index = g_state.l.theme_info_count;
                struct tagap_theme_info *t = &g_state.l.theme_infos[index];
                memset(t, 0, sizeof(struct tagap_theme_info));

                if (strlen(token) >= THEME_NAME_MAX)
                {
                    LOG_ERROR("[tagap_script] THEME: name is too "
                        "long (max %d) in (%s:%d)",
                        THEME_NAME_MAX, fpath, line_num);
                    goto next_line;
                }

                cur_parse_mode = TAGAP_PARSE_THEME;
                ++g_state.l.theme_info_count;

                // Set the theme name
                strcpy(t->name, token);
            } goto next_line;

            // Theme colour definition
            case ATOM_COLOUR:
            {
                if (cur_parse_mode != TAGAP_PARSE_THEME)
                {
                    LOG_ERROR("[tagap_script] COLOR: not on theme");
                    goto next_line;
                }

                // Get current theme
                struct tagap_theme_info *t =
                    &g_state.l.theme_infos[g_state.l.theme_info_count - 1];

                // Get 'affect'
                enum tagap_theme_affect_id affect =
                    (enum tagap_theme_affect_id)atoi(token);
                token = TOK_NEXT;

                // Get 'state'
                enum tagap_theme_state state =
                    (enum tagap_theme_state)atoi(token);
                token = TOK_NEXT;

                // Get RGB colour
                i32 r = atoi(token);
                token = TOK_NEXT;
                i32 g = atoi(token);
                token = TOK_NEXT;
                i32 b = atoi(token);

                t->colours[affect][state] = (vec3s)
                {{
                     (f32)r / 255.0f,
                     (f32)g / 255.0f,
                     (f32)b / 255.0f,
                }};
            } goto next_line;

            // End theme definition
            case ATOM_THEME_END:
            theme_end:
            {
                // Reset to normal parsing mode
                cur_parse_mode = TAGAP_PARSE_NORMAL;
            } goto next_line;

            // Skip unimplemented atoms
            default: goto next_line;
            }
        }

    next_line:
        continue;
    }
    free(line_tmp);

    fclose(fp);

    return 0;
}
