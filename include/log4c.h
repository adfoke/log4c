#ifndef LOG4C_H
#define LOG4C_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Log level definitions */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_COUNT
} log_level_t;

/* Output targets */
typedef enum {
    LOG_OUTPUT_CONSOLE = 1 << 0,
    LOG_OUTPUT_FILE = 1 << 1
} log_output_t;

/* Logger configuration */
typedef struct {
    log_level_t level;           /* Current log level */
    int outputs;                 /* Output targets (bitmask) */
    char filename[256];          /* Log filename */
    bool colors_enabled;         /* Whether to enable colors */
    bool thread_safe;            /* Whether thread-safe */
} log_config_t;

/* Global functions */

/* Initialize logger */
int log_init(const log_config_t* config);

/* Cleanup logger */
void log_cleanup(void);

/* Set log level */
void log_set_level(log_level_t level);

/* Set output targets */
void log_set_outputs(int outputs);

/* Set log file */
int log_set_file(const char* filename);

/* Enable/disable colors */
void log_enable_colors(bool enable);

/* Core log function */
void log_log(log_level_t level, const char* file, int line, const char* func, const char* format, ...);

/* Convenience macros */
#define LOG_DEBUG(format, ...) log_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  log_log(LOG_LEVEL_INFO,  __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)  log_log(LOG_LEVEL_WARN,  __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) log_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_FATAL(format, ...) log_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

/* Conditional log macros (compiled only when corresponding level is enabled) */
#if defined(LOG_COMPILE_DEBUG) || !defined(NDEBUG)
#define LOG_DEBUG_COND(format, ...) LOG_DEBUG(format, ##__VA_ARGS__)
#else
#define LOG_DEBUG_COND(format, ...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* LOG4C_H */
