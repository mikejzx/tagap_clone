#include "pch.h"
#include "log.h"
#include <stdarg.h>

static inline void 
log_base(enum log_mode mode, const char *fmt, va_list args)
{
    fprintf(stdout, "%s", LOG_PREFICES[mode]);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, LOG_ESC_RESET "\n");
    fflush(stdout);
}

void 
log_infofln(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_base(LOG_MODE_INFO, fmt, args);
    va_end(args);
}

void 
log_warnfln(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_base(LOG_MODE_WARN, fmt, args);
    va_end(args);
}

void 
log_errfln(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_base(LOG_MODE_ERROR, fmt, args);
    va_end(args);
}

void 
log_dbugfln(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_base(LOG_MODE_DEBUG, fmt, args);
    va_end(args);
}
