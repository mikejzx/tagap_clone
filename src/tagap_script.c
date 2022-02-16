#include "pch.h"
#include "tagap.h"
#include "tagap_anim.h"
#include "tagap_entity.h"
#include "tagap_linedef.h"
#include "tagap_sprite.h"
#include "tagap_polygon.h"
#include "tagap_theme.h"
#include "tagap_layer.h"
#include "tagap_trigger.h"
#include "tagap_script.h"

#include "tagap_script_def.h"

#include "tagap_script_logging.h"

static i32 tagap_script_run_cmd_in_state(
    enum tagap_script_atom_id, struct tagap_script_state *);
static enum tagap_script_atom_id tagap_script_parse_cmd(
    struct tagap_script_state *, const char *, size_t);

/* Set script parser mode */
static inline void
tagap_script_set_parse_mode(
    struct tagap_script_state *ss,
    enum tagap_script_parse_mode mode)
{
    if (ss->mode == mode) return;
    if (mode != TAGAP_PARSE_NORMAL &&
        ss->mode != TAGAP_PARSE_NORMAL)
    {
        SCRIPT_WARN("changing parser mode to '%d' when '%d' hadn't finished!",
            mode, ss->mode);
    }
    ss->mode = mode;
}

/*
 * Parse and run a command string
 */
i32
tagap_script_run_cmd(
    struct tagap_script_state *ss,
    const char *cmd,
    size_t cmd_len)
{
    // Parse the command
    enum tagap_script_atom_id atom = tagap_script_parse_cmd(ss, cmd, cmd_len);
    if (atom < 0) return -1;

    // Run parsed command
    i32 status = tagap_script_run_cmd_in_state(atom, ss);

    // Finally, adjust the parser mode
    if (status == 0 && ss->has_next_mode)
    {
        tagap_script_set_parse_mode(ss, ss->next_mode);
        ss->has_next_mode = false;
    }
    return status;
}

/*
 * Run script from file
 */
i32
tagap_script_run(const char *fpath)
{
    // Make sure that all defined atoms have strings
#if DEBUG
    assert(sizeof(TAGAP_SCRIPT_COMMANDS) /
        sizeof(struct tagap_script_command) == _ATOM_COUNT);
#endif

    FILE *fp = fopen(fpath, "r");
    if (!fp)
    {
        LOG_ERROR("[tagap_script] failed to read script '%s' (%d)",
            fpath, errno);
        return -1;
    }

    LOG_INFO("running script '%s'", fpath);

    // Create state
    struct tagap_script_state ss;
    tagap_script_new_state(&ss);

#ifdef DEBUG
    ss.line_num = 1;
    strcpy(ss.fname, fpath);
#endif

    // Read the script file line by line
    size_t len = 0, ltmp;
    for (char *line = NULL; 
        (len = getline(&line, &ltmp, fp)) != -1; 
        ++ss.line_num)
    {
        // Strip newline
        if (line[len - 1] == '\n') line[len - 2] = '\0';

        // Parse and run the command
        tagap_script_run_cmd(&ss, line, len);
    }

    fclose(fp);

    return 0;
}

/*
 * Parse a string of TAGAP_Script
 */
static enum tagap_script_atom_id
tagap_script_parse_cmd(
    struct tagap_script_state *ss,
    const char *cmd_str,
    size_t cmd_len)
{
    if (cmd_len < 1 || cmd_str[0] == '/') return 0;
    strcpy(ss->tmp, cmd_str);

    // Get the first token in the string, this is our command ID
    char *token = strtok(ss->tmp, " ");
    if (token == NULL) return -1;

    // Get info about the command we are parsing.  We store parsed token values
    // in here too
    enum tagap_script_atom_id atom;
    struct tagap_script_command cmd =
        tagap_script_lookup_command(token, &atom);
    if (atom == ATOM_UNKNOWN)
    {
        // Unknown/unimplemented command
        return -1;
    }

    // Make sure that we are in the right parsing mode
    if (cmd.requires_mode && ss->mode != cmd.required_mode)
    {
        SCRIPT_ERROR("cannot parse command '%s' "
            "because parse mode '%d' not satisfied (is %d)",
            cmd.name, cmd.required_mode, ss->mode);
        return -1;
    }

    {
        // Get minimum number of tokens we need to successfully parse
        u32 min_tok_count = cmd.token_count;
        if (min_tok_count > 0 && cmd.tokens[min_tok_count - 1].optional)
        {
            // Last parameter is optional.
            --min_tok_count;
        }

        // Count number of tokens in the string and make sure they are
        // sufficient
        u32 tok_count = 0;
        const char *tmpptr = cmd_str;
        for (;(tmpptr = strchr(tmpptr, ' ')) != NULL; ++tok_count, ++tmpptr);
        if (tok_count < min_tok_count)
        {
            SCRIPT_ERROR("parse fail: token count (%d) does not meet "
                "minimum of %d tokens (%s)",
                tok_count, min_tok_count, cmd.name);
            return -1;
        }
    }

    // Zero all the tokens so that unspecified optional parameters don't cause
    // issues
    memset(ss->tok, 0, sizeof(union tagap_script_token_value));
    ss->tok_count = 0;

    // Now iterate over the tokens/parameters in the command
    for (token = strtok(NULL, " ");
        token != NULL;
        token = strtok(NULL, " "))
    {
        i32 tok_index = ss->tok_count;
        union tagap_script_token_value *v = &ss->tok[tok_index];
        ++ss->tok_count;

        // Used for strtof, strtod, etc. error checking
        char *tmp_ptr;

        // Convert token to appropriate types and store in the parser for
        // proper parsing
        switch(cmd.tokens[tok_index].type)
        {
        // Token is an integer
        case TSCRIPT_TOKEN_INT:
        {
            v->i = strtol(token, &tmp_ptr, 10);
            if (v->i == 0 && tmp_ptr == token)
            {
                SCRIPT_ERROR("error parsing 'int' token: '%s'", token);
                return -1;
            }
        } break;

        // Token is floating-point
        case TSCRIPT_TOKEN_FLOAT:
        {
            v->f = strtof(token, &tmp_ptr);
            if (v->f == 0.0f && tmp_ptr == token)
            {
                SCRIPT_ERROR("error parsing 'float' token: '%s'", token);
                return -1;
            }
        } break;

        // Token is boolean
        case TSCRIPT_TOKEN_BOOL:
        {
            v->b = (bool)!!strtol(token, &tmp_ptr, 10);
            if (v->b == 0 && tmp_ptr == token)
            {
                SCRIPT_ERROR("error parsing 'bool' token: '%s'", token);
                return -1;
            }
        } break;

        // Token is a string
        case TSCRIPT_TOKEN_STRING:
        {
            size_t slen = strlen(token);
            if (slen >= TAGAP_SCRIPT_STRING_TOKEN_MAX ||
                slen >= cmd.tokens[tok_index].length)
            {
                SCRIPT_ERROR("'string' token '%s' is too long", token);
                return -1;
            }

            // Just copy the string
            strcpy(v->str, token);
        } break;

        // Special: token is an enum value that needs to be looked up
        case TSCRIPT_TOKEN_LOOKUP:
        {
            if (!cmd.tokens[tok_index].lookup_func)
            {
                SCRIPT_ERROR("lookup_func for '%s' not defined", cmd.name);
                return -1;
            }

            // Set the integer to the lookup-up value
            v->i = cmd.tokens[tok_index].lookup_func(token);
        } break;

        // Special: token is name of an entity
        case TSCRIPT_TOKEN_ENTITY:
        {
            // Search for the entity info in the entity list
            v->e = NULL;
            for (u32 i = 0; i < g_level->entity_info_count; ++i)
            {
                struct tagap_entity_info *e = &g_level->entity_infos[i];
                if (strcmp(e->name, token) == 0)
                {
                    v->e = e;
                    //LOG_SCRIPT("found info for entity %s", token);
                    break;
                }
            }
            if (!v->e)
            {
                SCRIPT_WARN("entity '%s' not found", token);
                return -1;
            }
        } break;

        // Special: token is name of a theme
        case TSCRIPT_TOKEN_THEME:
        {
            // Search for the theme info in the theme list
            v->t = NULL;
            for (u32 i = 0; i < g_level->theme_info_count; ++i)
            {
                struct tagap_theme_info *t = &g_level->theme_infos[i];
                if (strcmp(t->name, token) == 0)
                {
                    v->t = t;
                    LOG_SCRIPT("found info for theme %s", token);
                    break;
                }
            }
            if (!v->t)
            {
                SCRIPT_WARN("theme '%s' not found", token);
                return -1;
            }
        } break;
        }
    }

    if (cmd.sets_mode)
    {
        ss->has_next_mode = true;
        ss->next_mode = cmd.sets_mode_to;
    }

    return atom;
}

/*
 * Run the command currently in script state
 */
static i32
tagap_script_run_cmd_in_state(
    enum tagap_script_atom_id atom,
    struct tagap_script_state *ss)
{
    switch (atom)
    {
    // Internal game stuff
    case ATOM_CVAR:
    {
        enum tagap_cvar_id cvar = ss->tok[0].i;
        const char *value = ss->tok[1].str;

        // First argument is CVAR type
        switch (cvar)
        {
        case CVAR_MAP_TITLE:
            LOG_SCRIPT("CVAR: map title is %s", value);
            strcpy(g_map->title, value);
            break;
        case CVAR_MAP_SCHEME:
            LOG_SCRIPT("CVAR: map scheme is %s", value);

            // Find theme with given name
            g_map->theme = NULL;
            for (u32 i = 0; i < g_level->theme_info_count; ++i)
            {
                if (strcmp(g_level->theme_infos[i].name, value) == 0)
                {
                    g_map->theme = &g_level->theme_infos[i];
                    break;
                }
            }
            if (!g_map->theme)
            {
                SCRIPT_WARN("CVAR: no such theme '%s'", value);
            }
            break;
        case CVAR_SND_SONG:
            LOG_SCRIPT("CVAR: map song is %s", value);
            break;
        default: break;
        }
    } break;

    // Line definitions
    case ATOM_LINEDEF:
    {
        if (g_map->linedef_count + 1 >= LEVEL_MAX_LINEDEFS)
        {
            SCRIPT_ERROR("LINEDEF: limit (%d) exceeded", LEVEL_MAX_LINEDEFS);
            return -1;
        }
        // Copy linedef info
        g_map->linedefs[g_map->linedef_count++] = (struct tagap_linedef)
        {
            .start.x = (f32)ss->tok[0].i,
            .start.y = (f32)ss->tok[1].i,
            .end.x = (f32)ss->tok[2].i,
            .end.y = (f32)ss->tok[3].i,
            .style = ss->tok[4].i,
        };
    } break;

    // Polygon definition
    case ATOM_POLYGON:
    {
        if (g_map->polygon_count + 1 >= LEVEL_MAX_POLYGONS)
        {
            SCRIPT_ERROR("POLYGON: limit (%d) exceeded", LEVEL_MAX_POLYGONS);
            return -1;
        }

        struct tagap_polygon *p = &g_map->polygons[g_map->polygon_count++];
        *p = (struct tagap_polygon)
        {
            .tex_offset_point = ss->tok[1].i,
            .tex_is_shaded = ss->tok[2].b,
            .point_count = 0,
            .depth = g_map->current_depth++,
        };
        strcpy(p->tex_name, ss->tok[0].str);
    } break;

    // End of polygon definition
    case ATOM_POLYGON_END: break;

    // Polygon point definition
    case ATOM_POLYPOINT:
    {
        struct tagap_polygon *p = &g_map->polygons[g_map->polygon_count - 1];
        if (p->point_count + 1 >= POLYGON_MAX_POINTS)
        {
            SCRIPT_ERROR("POLYPOINT: too many points on polygon (%d, max:%d)",
                p->point_count + 1, POLYGON_MAX_POINTS);
            return -1;
        }
        p->points[p->point_count++] = (vec2s)
        {
            .x = ss->tok[0].f,
            .y = ss->tok[1].f,
        };
    } break;

    // Begin theme definition
    case ATOM_THEME:
    {
        if (g_level->theme_info_count + 1 >= GAME_THEME_INFO_LIMIT)
        {
            SCRIPT_ERROR("THEME: limit (%d) exceeded", GAME_THEME_INFO_LIMIT);
            return -1;
        }
        struct tagap_theme_info *t =
            &g_level->theme_infos[g_level->theme_info_count++];
        memset(t, 0, sizeof(struct tagap_theme_info));
        strcpy(t->name, ss->tok[0].str);
    } break;

    // End theme definition
    case ATOM_THEME_END: break;

    // Theme colour definition
    case ATOM_COLOUR:
    {
        g_level->theme_infos[g_level->theme_info_count - 1]
            .colours[ss->tok[0].b][ss->tok[1].b] = (vec3s)
        {
            .x = (f32)ss->tok[2].i / 255.0f,
            .y = (f32)ss->tok[3].i / 255.0f,
            .z = (f32)ss->tok[4].i / 255.0f,
        };
    } break;

    // Environment setting for theme
    case ATOM_ENVIRONMENT:
    {
        struct tagap_theme_info *t = 
            &g_level->theme_infos[g_level->theme_info_count - 1];
        t->env = ss->tok[0].i;
    };

    // Darkness setting for theme
    case ATOM_DARKNESS:
    {
        struct tagap_theme_info *t = 
            &g_level->theme_infos[g_level->theme_info_count - 1];
        t->darkness[THEME_STATE_BASE] = ss->tok[0].i;
        t->darkness[THEME_STATE_SHIFT] = ss->tok[1].i;
    };

    // Begin entity definition
    case ATOM_ENTITY_START:
    {
        if (g_level->entity_info_count + 1 >= GAME_ENTITY_INFO_LIMIT)
        {
            SCRIPT_ERROR("ENTITY_START: limit (%d) exceeded",
                GAME_ENTITY_INFO_LIMIT);
            return -1;
        }
        struct tagap_entity_info *e =
            &g_level->entity_infos[g_level->entity_info_count++];
        memset(e, 0, sizeof(struct tagap_entity_info));
        strcpy(e->name, ss->tok[0].str);
    } break;

    // End of entity definition
    case ATOM_ENTITY_END: break;

    // Spawn an entity into the world
    case ATOM_ENTITY_SET:
    {
        if (ss->tok[0].e == NULL)
        {
            SCRIPT_WARN("ENTITY_SET: parsed entity is null");
            return -1;
        }
        struct tagap_entity *e = level_add_entity(ss->tok[0].e);
        if (!e)
        {
            SCRIPT_ERROR("ENTITY_SET: failed to add entity");
            return -1;
        }
        e->position = (vec2s){ (f32)ss->tok[1].i, (f32)ss->tok[2].i };
        e->aim_angle = (f32)ss->tok[3].i;
        e->active = !ss->tok[4].b;
    } break;

    // Clone entity info
    case ATOM_CLONE:
    {
        entity_info_clone(
            &g_level->entity_infos[g_level->entity_info_count - 1],
            ss->tok[0].e,
            true);
    } break;

    // Set sprite on entity
    case ATOM_SPRITE:
    {
        struct tagap_entity_info *e =
            &g_level->entity_infos[g_level->entity_info_count - 1];

        // Check bounds of entity sprite list
        if (e->sprite_count + 1 >= ENTITY_MAX_SPRITES)
        {
            SCRIPT_ERROR("SPRITE: too many sprites (%d, max:%d)",
                e->sprite_count + 1, ENTITY_MAX_SPRITES);
            return -1;
        }

        // Check global list if sprite info has already been added , so we can
        // reuse it (and hence avoid re-reading it's frames, etc.)
        struct tagap_sprite_info *info = NULL;
        for (u32 i = 0; i < g_level->sprite_info_count; ++i)
        {
            if (strcmp(g_level->sprite_infos[i].name, ss->tok[5].str) == 0)
            {
                info = &g_level->sprite_infos[i];
                break;
            }
        }
        if (!info)
        {
            // Sprite has not been added to the list yet, so let's add it.
            i32 spr_index_global = g_level->sprite_info_count;

            // Check that the global sprite list isn't full
            if (spr_index_global + 1 >= GAME_SPRITE_INFO_LIMIT)
            {
                SCRIPT_ERROR("SPRITE: info limit (%d) reached",
                    GAME_SPRITE_INFO_LIMIT);
                return -1;
            }
            ++g_level->sprite_info_count;

            // Get the info, zero it out, and copy the name over
            info = &g_level->sprite_infos[spr_index_global];
            memset(info, 0, sizeof(struct tagap_sprite_info));
            strcpy(g_level->sprite_infos[spr_index_global].name,
                ss->tok[5].str);
        }

        // Get sprite in the entity info list
        struct tagap_entity_sprite *spr = &e->sprites[e->sprite_count++];
        *spr = (struct tagap_entity_sprite)
        {
            .info = info,
            .bright = ss->tok[1].b,
            .anim = (enum tagap_anim)ss->tok[2].i,
            .offset.x = (f32)ss->tok[3].i,
            .offset.y = (f32)ss->tok[4].i,
        };
    } break;

    // Defines a variable for a sprite
    case ATOM_SPRITEVAR:
    {
        // Check that integer is in range
        if (ss->tok[0].i < 0 || ss->tok[0].i >= ENTITY_MAX_SPRITES)
        {
            SCRIPT_ERROR("SPRITEVAR: invalid index %d", ss->tok[0].i);
            return -1;
        }

        struct tagap_entity_info *e =
            &g_level->entity_infos[g_level->entity_info_count - 1];

        // Check if optional argument was given
        if (ss->tok_count < TAGAP_SCRIPT_COMMANDS[atom].token_count)
        {
            // Toggle variable on
            e->sprites[ss->tok[0].i].vars[ss->tok[1].i] = 1;
        }
        else
        {
            // Set to value that was given
            e->sprites[ss->tok[0].i].vars[ss->tok[1].i] = ss->tok[2].i;
        }
    } break;

    // Sets entity movement type
    case ATOM_MOVETYPE:
    {
        struct tagap_entity_info *e =
            &g_level->entity_infos[g_level->entity_info_count - 1];

        e->move = (struct tagap_entity_movetype)
        {
            .type = ss->tok[0].i,
            .speed = ss->tok[1].f,
        };
    } break;

    // Sets entity AI routine mode
    case ATOM_THINK:
    {
        struct tagap_entity_info *e =
            &g_level->entity_infos[g_level->entity_info_count - 1];

        e->think = (struct tagap_entity_think)
        {
            .mode = ss->tok[0].i,
            .speed_mod = ss->tok[1].f,
            .attack = ss->tok[2].i,
            .attack_delay = ss->tok[3].f,
        };
    } break;

    // Sets an offset for an entity
    case ATOM_OFFSET:
    {
        struct tagap_entity_info *e =
            &g_level->entity_infos[g_level->entity_info_count - 1];

        // Check for 'optional' second value
        f32 y = 0.0f;
        if (ss->tok_count < TAGAP_SCRIPT_COMMANDS[atom].token_count)
        {
            y = (f32)ss->tok[1].i;
        }
        else
        {
            // Use normal y value
            y = (f32)ss->tok[2].i;
        }

        // Set offset values
        e->offsets[ss->tok[0].i] = (vec2s)
        {
            .x = (f32)ss->tok[1].i,
            .y = y
        };
    } break;

    // Sets a variable for an entity
    case ATOM_STAT:
    {
        struct tagap_entity_info *e =
            &g_level->entity_infos[g_level->entity_info_count - 1];

        // Check if optional parameter was passed
        if (ss->tok_count < TAGAP_SCRIPT_COMMANDS[atom].token_count)
        {
            // Toggle variable
            e->stats[ss->tok[0].i] = 1;
        }
        else
        {
            e->stats[ss->tok[0].i] = ss->tok[1].i;

            // Special case: for S_WEAPON we indicate that the entity indeed
            // has a weapon
            if (ss->tok[0].i == STAT_S_WEAPON)
            {
                e->has_weapon = true;
            }
        }
    } break;

    // Sets entity as weapon model
    case ATOM_GUNENTITY:
    {
        struct tagap_entity_info *e =
            &g_level->entity_infos[g_level->entity_info_count - 1];
        e->gun_entity = ss->tok[0].e;
    } break;

    // Sets a weapon slot
    case ATOM_WEAPON:
    {
        // Check that the slot is in range
        if (ss->tok[0].i < 0 || ss->tok[0].i >= WEAPON_SLOT_COUNT)
        {
            SCRIPT_ERROR("WEAPON: slot index '%d' out of range", ss->tok[0].i);
            return -1;
        }

        // Copy data into the weapon slot
        struct tagap_weapon *w = &g_level->weapons[ss->tok[0].i];
        strcpy(w->display_name, ss->tok[3].str);
        w->primary = ss->tok[1].e;
        w->secondary = ss->tok[2].e;
    } break;

    // Sets a static layer in the level
    case ATOM_LAYER:
    {
        if (g_map->layer_count + 1 >= LEVEL_MAX_LAYERS)
        {
            SCRIPT_ERROR("LAYER: limit (%d) exceeded", LEVEL_MAX_LAYERS);
            return -1;
        }

        struct tagap_layer *l = &g_map->layers[g_map->layer_count++];
        *l = (struct tagap_layer)
        {
            .scroll_speed_mul = ss->tok[0].f,
            .offset_y = ss->tok[1].f,
            .movement_speed = (vec2s) { ss->tok[2].f, ss->tok[3].f },
            .rendering_flag = ss->tok[5].i,
            .depth = g_map->current_depth++,
        };
        strcpy(l->tex_name, ss->tok[4].str);
    } break;

    // Sets up a trigger in the level
    case ATOM_TRIGGER:
    {
        if (g_map->trigger_count + 1 >= LEVEL_MAX_TRIGGERS)
        {
            SCRIPT_ERROR("TRIGGER: limit (%d) exceeded", LEVEL_MAX_TRIGGERS);
            return -1;
        }

        struct tagap_trigger *t = &g_map->triggers[g_map->trigger_count++];
        *t = (struct tagap_trigger)
        {
            .corner_tl.x = ss->tok[0].f,
            .corner_tl.y = ss->tok[1].f,
            .corner_br.x = ss->tok[2].f,
            .corner_br.y = ss->tok[3].f,
            .target_index = ss->tok[4].i,
            .id = ss->tok[5].i,
        };
    } break;

    default: break;
    }

    return 0;
}

#undef SCRIPT_ERROR
#undef SCRIPT_WARN
#undef LOG_SCRIPT

