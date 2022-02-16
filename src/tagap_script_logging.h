#ifndef TAGAP_SCRIPT_LOGGING_H
#define TAGAP_SCRIPT_LOGGING_H

// Regular logging
#define LOG_SCRIPT(...) LOG_INFO("[tagap_script] " __VA_ARGS__)

// For errors and warnings we print file path and line number
#ifdef DEBUG

void
debug_print_script_errwarn(
    bool err, 
    struct tagap_script_state *ss, 
    const char *x, 
    ...)
{
    va_list args;
    va_start(args, x);

    // Print original message into the buffer, then append a line with
    // file/line info
    char msg[4096];
    vsprintf(msg, x, args);
    if (ss->line_num > -1)
    {
        char msg2[2048];
        strcpy(msg2, msg);
        sprintf(msg, "%s\n       --> in %s:%d", msg2, ss->fname, ss->line_num);
    }

    if (err) LOG_ERROR(msg);
    else LOG_WARN(msg);

    va_end(args);
}

// Print error
#  define SCRIPT_ERROR(...) debug_print_script_errwarn( \
    true, ss, "[tagap_script] parse fail: " __VA_ARGS__)

// Print warning
#  define SCRIPT_WARN(...) debug_print_script_errwarn( \
    false, ss, "[tagap_script] " __VA_ARGS__)
#else
#  define SCRIPT_ERROR(...) LOG_ERROR("[tagap_script] parse fail: " __VA_ARGS__)
#  define SCRIPT_WARN(...) LOG_WARN("[tagap_script] " __VA_ARGS__)
#endif // DEBUG


#endif
