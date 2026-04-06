#ifndef LOG4C_LOG4C_H
#define LOG4C_LOG4C_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#if defined(LOG4C_WITH_PTHREAD)
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LOG4C_MAX_SINKS 8u
#define LOG4C_CONFIG_PATH_CAPACITY 256u
#define LOG4C_CONFIG_NAME_CAPACITY 128u

typedef enum log4c_level {
    LOG4C_LEVEL_TRACE = 0,
    LOG4C_LEVEL_DEBUG = 1,
    LOG4C_LEVEL_INFO = 2,
    LOG4C_LEVEL_WARN = 3,
    LOG4C_LEVEL_ERROR = 4,
    LOG4C_LEVEL_FATAL = 5,
    LOG4C_LEVEL_OFF = 6
} log4c_level;

typedef enum log4c_console_stream {
    LOG4C_CONSOLE_STDERR = 0,
    LOG4C_CONSOLE_STDOUT = 1
} log4c_console_stream;

typedef enum log4c_option {
    LOG4C_OPTION_TIMESTAMP = 1u << 0,
    LOG4C_OPTION_LOCATION = 1u << 1,
    LOG4C_OPTION_COLOR = 1u << 2,
    LOG4C_OPTION_AUTO_FLUSH = 1u << 3
} log4c_option;

typedef void (*log4c_lock_fn)(void *userdata);
typedef int (*log4c_sink_write_fn)(void *userdata, const char *record);
typedef int (*log4c_sink_flush_fn)(void *userdata);
typedef int (*log4c_sink_close_fn)(void *userdata);

typedef struct log4c_sink {
    log4c_sink_write_fn write;
    log4c_sink_flush_fn flush;
    log4c_sink_close_fn close;
    void *userdata;
    unsigned int options;
    bool enabled;
} log4c_sink;

typedef struct log4c_logger {
    log4c_level min_level;
    unsigned int default_options;
    log4c_lock_fn lock;
    log4c_lock_fn unlock;
    void *lock_userdata;
    log4c_sink sinks[LOG4C_MAX_SINKS];
    size_t sink_count;
} log4c_logger;

typedef struct log4c_config {
    char log_dir[LOG4C_CONFIG_PATH_CAPACITY];
    char file_name[LOG4C_CONFIG_NAME_CAPACITY];
    log4c_level level;
    size_t rotation_max_bytes;
    size_t rotation_max_files;
    bool enable_console;
    bool enable_file;
    bool color;
    bool timestamp;
    bool location;
    bool auto_flush;
    log4c_console_stream console_stream;
} log4c_config;

void log4c_logger_init(log4c_logger *logger, FILE *stream);
bool log4c_logger_init_from_config(log4c_logger *logger, const log4c_config *config);
void log4c_logger_destroy(log4c_logger *logger);
void log4c_logger_clear_sinks(log4c_logger *logger);
void log4c_logger_set_level(log4c_logger *logger, log4c_level level);
void log4c_logger_set_stream(log4c_logger *logger, FILE *stream);
void log4c_logger_set_options(log4c_logger *logger, unsigned int options);
void log4c_logger_set_lock(
    log4c_logger *logger,
    log4c_lock_fn lock,
    log4c_lock_fn unlock,
    void *userdata
);
bool log4c_logger_add_stream_sink(log4c_logger *logger, FILE *stream, unsigned int options);
bool log4c_logger_add_callback_sink(
    log4c_logger *logger,
    log4c_sink_write_fn write,
    log4c_sink_flush_fn flush,
    void *userdata,
    unsigned int options
);
bool log4c_logger_add_file_sink(log4c_logger *logger, const char *path, unsigned int options);
bool log4c_logger_add_rotating_file_sink(
    log4c_logger *logger,
    const char *path,
    size_t max_bytes,
    size_t max_files,
    unsigned int options
);
size_t log4c_logger_sink_count(const log4c_logger *logger);

bool log4c_logger_should_log(const log4c_logger *logger, log4c_level level);
const char *log4c_level_name(log4c_level level);
void log4c_config_init(log4c_config *config);
bool log4c_config_load(log4c_config *config, const char *path);

int log4c_logger_log(
    log4c_logger *logger,
    log4c_level level,
    const char *tag,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...
);

int log4c_logger_vlog(
    log4c_logger *logger,
    log4c_level level,
    const char *tag,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    va_list args
);

#if defined(LOG4C_WITH_PTHREAD)
typedef struct log4c_async_message log4c_async_message;

typedef struct log4c_threadsafe_logger {
    log4c_logger logger;
    pthread_mutex_t mutex;
} log4c_threadsafe_logger;

typedef enum log4c_async_queue_policy {
    LOG4C_ASYNC_QUEUE_BLOCK = 0,
    LOG4C_ASYNC_QUEUE_DROP_NEWEST = 1,
    LOG4C_ASYNC_QUEUE_DROP_OLDEST = 2
} log4c_async_queue_policy;

typedef enum log4c_async_error {
    LOG4C_ASYNC_ERROR_ALLOC = 0,
    LOG4C_ASYNC_ERROR_QUEUE_FULL = 1,
    LOG4C_ASYNC_ERROR_SINK_WRITE = 2
} log4c_async_error;

typedef void (*log4c_async_error_fn)(void *userdata, log4c_async_error error, const char *detail);

typedef struct log4c_async_stats {
    size_t current_queue;
    size_t max_queue;
    size_t enqueued;
    size_t written;
    size_t dropped;
    size_t queue_full;
    size_t alloc_failures;
    size_t write_failures;
} log4c_async_stats;

typedef struct log4c_async_logger {
    log4c_logger logger;
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    pthread_cond_t drained;
    log4c_async_message *head;
    log4c_async_message *tail;
    size_t queued;
    size_t max_queue;
    bool stop;
    bool worker_active;
    bool worker_failed;
    log4c_async_queue_policy queue_policy;
    log4c_async_error_fn error_callback;
    void *error_userdata;
    log4c_async_stats stats;
} log4c_async_logger;

bool log4c_threadsafe_logger_init(log4c_threadsafe_logger *wrapper, FILE *stream);
void log4c_threadsafe_logger_destroy(log4c_threadsafe_logger *wrapper);
log4c_logger *log4c_threadsafe_logger_get(log4c_threadsafe_logger *wrapper);
const log4c_logger *log4c_threadsafe_logger_cget(const log4c_threadsafe_logger *wrapper);

bool log4c_async_logger_init(log4c_async_logger *wrapper, FILE *stream, size_t max_queue);
bool log4c_async_logger_init_from_config(
    log4c_async_logger *wrapper,
    const log4c_config *config,
    size_t max_queue
);
void log4c_async_logger_set_queue_policy(
    log4c_async_logger *wrapper,
    log4c_async_queue_policy policy
);
void log4c_async_logger_set_error_callback(
    log4c_async_logger *wrapper,
    log4c_async_error_fn callback,
    void *userdata
);
bool log4c_async_logger_get_stats(
    log4c_async_logger *wrapper,
    log4c_async_stats *stats
);
bool log4c_async_logger_flush(log4c_async_logger *wrapper);
void log4c_async_logger_destroy(log4c_async_logger *wrapper);
int log4c_async_logger_log(
    log4c_async_logger *wrapper,
    log4c_level level,
    const char *tag,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...
);
int log4c_async_logger_vlog(
    log4c_async_logger *wrapper,
    log4c_level level,
    const char *tag,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    va_list args
);
#endif

#define LOG4C_TRACE(logger, tag, ...) \
    log4c_logger_log((logger), LOG4C_LEVEL_TRACE, (tag), __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG4C_DEBUG(logger, tag, ...) \
    log4c_logger_log((logger), LOG4C_LEVEL_DEBUG, (tag), __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG4C_INFO(logger, tag, ...) \
    log4c_logger_log((logger), LOG4C_LEVEL_INFO, (tag), __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG4C_WARN(logger, tag, ...) \
    log4c_logger_log((logger), LOG4C_LEVEL_WARN, (tag), __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG4C_ERROR(logger, tag, ...) \
    log4c_logger_log((logger), LOG4C_LEVEL_ERROR, (tag), __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG4C_FATAL(logger, tag, ...) \
    log4c_logger_log((logger), LOG4C_LEVEL_FATAL, (tag), __FILE__, __LINE__, __func__, __VA_ARGS__)

#if defined(LOG4C_WITH_PTHREAD)
#define LOG4C_ASYNC_TRACE(logger, tag, ...) \
    log4c_async_logger_log((logger), LOG4C_LEVEL_TRACE, (tag), __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG4C_ASYNC_DEBUG(logger, tag, ...) \
    log4c_async_logger_log((logger), LOG4C_LEVEL_DEBUG, (tag), __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG4C_ASYNC_INFO(logger, tag, ...) \
    log4c_async_logger_log((logger), LOG4C_LEVEL_INFO, (tag), __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG4C_ASYNC_WARN(logger, tag, ...) \
    log4c_async_logger_log((logger), LOG4C_LEVEL_WARN, (tag), __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG4C_ASYNC_ERROR(logger, tag, ...) \
    log4c_async_logger_log((logger), LOG4C_LEVEL_ERROR, (tag), __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG4C_ASYNC_FATAL(logger, tag, ...) \
    log4c_async_logger_log((logger), LOG4C_LEVEL_FATAL, (tag), __FILE__, __LINE__, __func__, __VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif
