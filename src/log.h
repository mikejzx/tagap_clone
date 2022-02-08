#ifndef LOG_H
#define LOG_H

#define LOG_INFO(...) log_infofln(__VA_ARGS__)
#define LOG_WARN(...) log_warnfln(__VA_ARGS__)
#define LOG_ERROR(...) log_errfln(__VA_ARGS__)
#define LOG_DBUG(...) log_dbugfln(__VA_ARGS__)

enum log_mode
{
    LOG_MODE_INFO = 0,
    LOG_MODE_WARN,
    LOG_MODE_ERROR,
    LOG_MODE_DEBUG,
};

#define LOG_ESC_INFO  "\033[00;37m" 
#define LOG_ESC_WARN  "\033[00;93m"
#define LOG_ESC_ERROR "\033[00;91m"
#define LOG_ESC_DEBUG "\033[00;95m"
#define LOG_ESC_RESET "\033[00m"

static const char *const LOG_PREFICES[] = 
{
    [LOG_MODE_INFO]  = LOG_ESC_INFO  "[INFO] ",
    [LOG_MODE_WARN]  = LOG_ESC_WARN  "[WARN] ",
    [LOG_MODE_ERROR] = LOG_ESC_ERROR "[ERROR] ",
    [LOG_MODE_DEBUG] = LOG_ESC_DEBUG "[DEBUG] ",
};

void log_infofln(const char *, ...);
void log_warnfln(const char *, ...);
void log_errfln(const char *, ...);
void log_dbugfln(const char *, ...);

#endif
