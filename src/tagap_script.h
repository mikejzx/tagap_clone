#ifndef TAGAP_SCRIPT_H
#define TAGAP_SCRIPT_H

#include "types.h"

#define TAGAP_SCRIPT_MAX_TOKENS 10
#define TAGAP_SCRIPT_STRING_TOKEN_MAX 256

enum tagap_script_token_type
{
    TSCRIPT_TOKEN_INT,
    TSCRIPT_TOKEN_FLOAT,
    TSCRIPT_TOKEN_BOOL,
    TSCRIPT_TOKEN_STRING,
    TSCRIPT_TOKEN_LOOKUP,

    // Special lookups
    TSCRIPT_TOKEN_ENTITY,
    TSCRIPT_TOKEN_THEME,
};

// Storage for token values
union tagap_script_token_value
{
    i32 i;
    f32 f;
    bool b;
    char str[TAGAP_SCRIPT_STRING_TOKEN_MAX];

    struct tagap_entity_info *e;
    struct tagap_theme_info *t;
};

// Single token/parameter in a TAGAP_Script command
struct tagap_script_token
{
    // Type of the token
    enum tagap_script_token_type type;

    // STRING TYPES: max length of string
    u32 length;

    // LOOKUP TYPE:
    // lookup function for lookup type Returns the looked-up value, or 0 on
    // fail. Takes one string argument as the term to lookup
    i32(*lookup_func)(const char *);

    // Whether this token can be omitted
    // (technically should only appear as last parameter);
    bool optional;
};

enum tagap_script_parse_mode
{
    TAGAP_PARSE_NORMAL = 0,
    TAGAP_PARSE_POLYGON,
    TAGAP_PARSE_ENTITY,
    TAGAP_PARSE_THEME,
};

// Used to define the parameters, etc. used in a command.
struct tagap_script_command
{
    // Command name
    char name[32];

    // Number of parameters the command takes
    u32 token_count;

    // Defines what parameters the command expects
    struct tagap_script_token tokens[TAGAP_SCRIPT_MAX_TOKENS];

    // (optional) required parse mode.  Set to NORMAL by default
    bool requires_mode;
    enum tagap_script_parse_mode required_mode;

    // (optional) mode that the command sets parser to
    bool sets_mode;
    enum tagap_script_parse_mode sets_mode_to;
};

// Current parser state
struct tagap_script_state
{
    // Values of tokens that parser is currently using
    union tagap_script_token_value tok[TAGAP_SCRIPT_MAX_TOKENS];
    u32 tok_count;

    // Current parsing mode
    enum tagap_script_parse_mode mode;

    // Mode that will be set after next command is run
    bool has_next_mode;
    enum tagap_script_parse_mode next_mode;

    // Used for line storage
    char tmp[256];

    // For debugging.  Set to -1 for non-file commands
    i32 line_num;
    char fname[256];
};

i32 tagap_script_run(const char *fpath);
i32 tagap_script_run_cmd(
    struct tagap_script_state *, const char *, size_t);

static inline void
tagap_script_new_state(struct tagap_script_state *ss)
{
    //memset(ss, 0, sizeof(struct tagap_script_state));
    ss->mode = TAGAP_PARSE_NORMAL;
    ss->tok_count = 0;
    ss->has_next_mode = false;
    ss->line_num = -1;
}

#endif
