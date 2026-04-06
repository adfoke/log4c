#include "log4c/log4c.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#endif

enum {
    LOG4C_BUFFER_CAPACITY = 512,
    LOG4C_CONFIG_LINE_CAPACITY = 512,
    LOG4C_TIMESTAMP_CAPACITY = 32
};

static const char *const LOG4C_LEVEL_NAMES[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
    "OFF"
};

typedef struct log4c_file_sink_data {
    FILE *stream;
    char *path;
    size_t current_size;
    size_t max_bytes;
    size_t max_files;
    bool rotating;
} log4c_file_sink_data;

#if defined(LOG4C_WITH_PTHREAD)
struct log4c_async_message {
    log4c_level level;
    int line;
    char *tag;
    char *file;
    char *func;
    char *message;
    struct log4c_async_message *next;
};
#endif

static bool log4c_is_dir_separator(char ch) {
    return ch == '/' || ch == '\\';
}

static char log4c_ascii_lower(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return (char)(ch - 'A' + 'a');
    }

    return ch;
}

static int log4c_mkdir_single(const char *path) {
#if defined(_WIN32)
    return _mkdir(path);
#else
    return mkdir(path, 0777);
#endif
}

static char *log4c_strdup(const char *text) {
    char *copy = NULL;
    size_t length = 0U;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1U);
    return copy;
}

static char *log4c_strdup_or_null(const char *text) {
    if (text == NULL) {
        return NULL;
    }

    return log4c_strdup(text);
}

static bool log4c_copy_string(char *dst, size_t capacity, const char *src) {
    size_t length = 0U;

    if (dst == NULL || src == NULL || capacity == 0U) {
        return false;
    }

    length = strlen(src);
    if (length >= capacity) {
        return false;
    }

    memcpy(dst, src, length + 1U);
    return true;
}

static bool log4c_str_ieq(const char *lhs, const char *rhs) {
    size_t index = 0U;

    if (lhs == NULL || rhs == NULL) {
        return false;
    }

    while (lhs[index] != '\0' && rhs[index] != '\0') {
        if (log4c_ascii_lower(lhs[index]) != log4c_ascii_lower(rhs[index])) {
            return false;
        }
        index += 1U;
    }

    return lhs[index] == rhs[index];
}

static void log4c_trim(char *text) {
    char *start = NULL;
    char *end = NULL;
    size_t length = 0U;

    if (text == NULL) {
        return;
    }

    start = text;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start += 1;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1U);
    }

    length = strlen(text);
    while (length > 0U) {
        end = text + length - 1U;
        if (*end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') {
            break;
        }
        *end = '\0';
        length -= 1U;
    }
}

static bool log4c_parse_bool(const char *text, bool *value) {
    if (text == NULL || value == NULL) {
        return false;
    }

    if (log4c_str_ieq(text, "1") ||
        log4c_str_ieq(text, "true") ||
        log4c_str_ieq(text, "yes") ||
        log4c_str_ieq(text, "on")) {
        *value = true;
        return true;
    }

    if (log4c_str_ieq(text, "0") ||
        log4c_str_ieq(text, "false") ||
        log4c_str_ieq(text, "no") ||
        log4c_str_ieq(text, "off")) {
        *value = false;
        return true;
    }

    return false;
}

static bool log4c_parse_size(const char *text, size_t *value) {
    unsigned long long parsed = 0ULL;
    char *end = NULL;

    if (text == NULL || value == NULL || text[0] == '\0') {
        return false;
    }

    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }

    if (parsed > (unsigned long long)SIZE_MAX) {
        return false;
    }

    *value = (size_t)parsed;
    return true;
}

static bool log4c_parse_level(const char *text, log4c_level *level) {
    if (text == NULL || level == NULL) {
        return false;
    }

    if (log4c_str_ieq(text, "trace")) {
        *level = LOG4C_LEVEL_TRACE;
        return true;
    }

    if (log4c_str_ieq(text, "debug")) {
        *level = LOG4C_LEVEL_DEBUG;
        return true;
    }

    if (log4c_str_ieq(text, "info")) {
        *level = LOG4C_LEVEL_INFO;
        return true;
    }

    if (log4c_str_ieq(text, "warn") || log4c_str_ieq(text, "warning")) {
        *level = LOG4C_LEVEL_WARN;
        return true;
    }

    if (log4c_str_ieq(text, "error")) {
        *level = LOG4C_LEVEL_ERROR;
        return true;
    }

    if (log4c_str_ieq(text, "fatal")) {
        *level = LOG4C_LEVEL_FATAL;
        return true;
    }

    if (log4c_str_ieq(text, "off")) {
        *level = LOG4C_LEVEL_OFF;
        return true;
    }

    return false;
}

static bool log4c_parse_console_stream(const char *text, log4c_console_stream *stream) {
    if (text == NULL || stream == NULL) {
        return false;
    }

    if (log4c_str_ieq(text, "stderr")) {
        *stream = LOG4C_CONSOLE_STDERR;
        return true;
    }

    if (log4c_str_ieq(text, "stdout")) {
        *stream = LOG4C_CONSOLE_STDOUT;
        return true;
    }

    return false;
}

static bool log4c_join_path(char *dst, size_t capacity, const char *dir, const char *name) {
    int written = 0;

    if (dst == NULL || capacity == 0U || name == NULL || name[0] == '\0') {
        return false;
    }

    if (dir == NULL || dir[0] == '\0') {
        return log4c_copy_string(dst, capacity, name);
    }

    if (log4c_is_dir_separator(dir[strlen(dir) - 1U])) {
        written = snprintf(dst, capacity, "%s%s", dir, name);
    } else {
        written = snprintf(dst, capacity, "%s/%s", dir, name);
    }

    return written >= 0 && (size_t)written < capacity;
}

static unsigned int log4c_options_from_config(const log4c_config *config) {
    unsigned int options = 0U;

    if (config == NULL) {
        return 0U;
    }

    if (config->timestamp) {
        options |= LOG4C_OPTION_TIMESTAMP;
    }

    if (config->location) {
        options |= LOG4C_OPTION_LOCATION;
    }

    if (config->color) {
        options |= LOG4C_OPTION_COLOR;
    }

    if (config->auto_flush) {
        options |= LOG4C_OPTION_AUTO_FLUSH;
    }

    return options;
}

static unsigned int log4c_console_options_from_config(const log4c_config *config) {
    return log4c_options_from_config(config);
}

static unsigned int log4c_file_options_from_config(const log4c_config *config) {
    return log4c_options_from_config(config) & (unsigned int)~((unsigned int)LOG4C_OPTION_COLOR);
}

static bool log4c_ensure_parent_dirs(const char *path) {
    char *copy = NULL;
    size_t index = 0U;
    size_t start = 0U;

    if (path == NULL || path[0] == '\0') {
        return false;
    }

    copy = log4c_strdup(path);
    if (copy == NULL) {
        return false;
    }

    if (copy[0] != '\0' && log4c_is_dir_separator(copy[0])) {
        start = 1U;
    } else if (copy[0] != '\0' && copy[1] == ':') {
        start = 2U;
    }

    for (index = start; copy[index] != '\0'; ++index) {
        if (!log4c_is_dir_separator(copy[index])) {
            continue;
        }

        copy[index] = '\0';
        if (copy[0] != '\0' &&
            !(copy[0] != '\0' && copy[1] == '\0' && copy[0] == '.')) {
            if (log4c_mkdir_single(copy) != 0 && errno != EEXIST) {
                free(copy);
                return false;
            }
        }
        copy[index] = '/';
    }

    free(copy);
    return true;
}

static bool log4c_file_size(FILE *stream, size_t *size_out) {
    long size = 0L;

    if (stream == NULL || size_out == NULL) {
        return false;
    }

    if (fseek(stream, 0L, SEEK_END) != 0) {
        return false;
    }

    size = ftell(stream);
    if (size < 0L) {
        return false;
    }

    *size_out = (size_t)size;
    return true;
}

static char *log4c_make_backup_path(const char *path, size_t index) {
    char *result = NULL;
    int written = 0;
    size_t capacity = 0U;

    if (path == NULL || index == 0U) {
        return NULL;
    }

    capacity = strlen(path) + 32U;
    result = (char *)malloc(capacity);
    if (result == NULL) {
        return NULL;
    }

    written = snprintf(result, capacity, "%s.%zu", path, index);
    if (written < 0 || (size_t)written >= capacity) {
        free(result);
        return NULL;
    }

    return result;
}

static bool log4c_rotate_file_sink(log4c_file_sink_data *data) {
    char *src = NULL;
    char *dst = NULL;
    size_t index = 0U;

    if (data == NULL || data->stream == NULL || data->path == NULL || !data->rotating) {
        return false;
    }

    (void)fflush(data->stream);
    if (fclose(data->stream) != 0) {
        data->stream = NULL;
        return false;
    }
    data->stream = NULL;

    if (data->max_files > 0U) {
        dst = log4c_make_backup_path(data->path, data->max_files);
        if (dst == NULL) {
            return false;
        }
        (void)remove(dst);
        free(dst);

        for (index = data->max_files; index > 1U; --index) {
            src = log4c_make_backup_path(data->path, index - 1U);
            dst = log4c_make_backup_path(data->path, index);
            if (src == NULL || dst == NULL) {
                free(src);
                free(dst);
                return false;
            }

            (void)remove(dst);
            (void)rename(src, dst);
            free(src);
            free(dst);
        }

        dst = log4c_make_backup_path(data->path, 1U);
        if (dst == NULL) {
            return false;
        }
        (void)remove(dst);
        (void)rename(data->path, dst);
        free(dst);
    }

    data->stream = fopen(data->path, "a");
    if (data->stream == NULL) {
        return false;
    }

    data->current_size = 0U;
    return true;
}

static bool log4c_open_file_sink(
    log4c_file_sink_data *data,
    const char *path,
    size_t max_bytes,
    size_t max_files
) {
    if (data == NULL || path == NULL || path[0] == '\0') {
        return false;
    }

    memset(data, 0, sizeof(*data));
    data->path = log4c_strdup(path);
    if (data->path == NULL) {
        return false;
    }

    if (!log4c_ensure_parent_dirs(path)) {
        free(data->path);
        data->path = NULL;
        return false;
    }

    data->stream = fopen(path, "a");
    if (data->stream == NULL) {
        free(data->path);
        data->path = NULL;
        return false;
    }

    data->max_bytes = max_bytes;
    data->max_files = max_files;
    data->rotating = max_bytes > 0U;
    if (!log4c_file_size(data->stream, &data->current_size)) {
        data->current_size = 0U;
    }

    return true;
}

static int log4c_stream_sink_write(void *userdata, const char *record) {
    FILE *stream = (FILE *)userdata;
    size_t length = 0U;

    if (stream == NULL || record == NULL) {
        return -1;
    }

    if (fputs(record, stream) == EOF) {
        return -1;
    }

    length = strlen(record);
    if (length > (size_t)INT_MAX) {
        return -1;
    }

    return (int)length;
}

static int log4c_stream_sink_flush(void *userdata) {
    FILE *stream = (FILE *)userdata;

    if (stream == NULL) {
        return -1;
    }

    return fflush(stream);
}

static int log4c_file_sink_write(void *userdata, const char *record) {
    log4c_file_sink_data *data = (log4c_file_sink_data *)userdata;
    int result = 0;
    size_t record_length = 0U;

    if (data == NULL || record == NULL) {
        return -1;
    }

    record_length = strlen(record);
    if (data->rotating &&
        data->max_bytes > 0U &&
        data->current_size > 0U &&
        (record_length > data->max_bytes || data->current_size > data->max_bytes - record_length) &&
        !log4c_rotate_file_sink(data)) {
        return -1;
    }

    result = log4c_stream_sink_write(data->stream, record);
    if (result >= 0) {
        data->current_size += record_length;
    }

    return result;
}

static int log4c_file_sink_flush(void *userdata) {
    log4c_file_sink_data *data = (log4c_file_sink_data *)userdata;

    if (data == NULL) {
        return -1;
    }

    return log4c_stream_sink_flush(data->stream);
}

static int log4c_file_sink_close(void *userdata) {
    log4c_file_sink_data *data = (log4c_file_sink_data *)userdata;
    int result = 0;

    if (data == NULL) {
        return -1;
    }

    if (data->stream != NULL) {
        result = fclose(data->stream);
    }

    free(data->path);
    free(data);
    return result;
}

static bool log4c_logger_add_sink_entry(
    log4c_logger *logger,
    log4c_sink_write_fn write,
    log4c_sink_flush_fn flush,
    log4c_sink_close_fn close,
    void *userdata,
    unsigned int options
) {
    if (logger == NULL || write == NULL || logger->sink_count >= LOG4C_MAX_SINKS) {
        return false;
    }

    logger->sinks[logger->sink_count].write = write;
    logger->sinks[logger->sink_count].flush = flush;
    logger->sinks[logger->sink_count].close = close;
    logger->sinks[logger->sink_count].userdata = userdata;
    logger->sinks[logger->sink_count].options = options;
    logger->sinks[logger->sink_count].enabled = true;
    logger->sink_count += 1U;
    return true;
}

static const char *log4c_level_color(log4c_level level) {
    switch (level) {
        case LOG4C_LEVEL_TRACE:
            return "\x1b[37m";
        case LOG4C_LEVEL_DEBUG:
            return "\x1b[36m";
        case LOG4C_LEVEL_INFO:
            return "\x1b[32m";
        case LOG4C_LEVEL_WARN:
            return "\x1b[33m";
        case LOG4C_LEVEL_ERROR:
            return "\x1b[31m";
        case LOG4C_LEVEL_FATAL:
            return "\x1b[35m";
        case LOG4C_LEVEL_OFF:
        default:
            return "";
    }
}

static bool log4c_is_valid_level(log4c_level level) {
    return level >= LOG4C_LEVEL_TRACE && level <= LOG4C_LEVEL_OFF;
}

static bool log4c_get_local_time(const time_t *now, struct tm *out_tm) {
#if defined(_WIN32)
    return localtime_s(out_tm, now) == 0;
#elif defined(_POSIX_VERSION)
    return localtime_r(now, out_tm) != NULL;
#else
    struct tm *tmp = localtime(now);
    if (tmp == NULL) {
        return false;
    }

    *out_tm = *tmp;
    return true;
#endif
}

static void log4c_append(
    char *buffer,
    size_t capacity,
    size_t *used,
    const char *fmt,
    ...
) {
    int written = 0;
    size_t remaining = 0;
    va_list args;

    if (*used >= capacity) {
        return;
    }

    remaining = capacity - *used;
    va_start(args, fmt);
    written = vsnprintf(buffer + *used, remaining, fmt, args);
    va_end(args);

    if (written < 0) {
        return;
    }

    if ((size_t)written >= remaining) {
        *used = capacity;
        return;
    }

    *used += (size_t)written;
}

static void log4c_format_timestamp(char *buffer, size_t capacity) {
    struct tm tm_value;
    time_t now = time(NULL);

    if (capacity == 0U) {
        return;
    }

    buffer[0] = '\0';

    if (now == (time_t)-1) {
        return;
    }

    if (!log4c_get_local_time(&now, &tm_value)) {
        return;
    }

    if (strftime(buffer, capacity, "%Y-%m-%d %H:%M:%S", &tm_value) == 0U) {
        buffer[0] = '\0';
    }
}

static int log4c_make_record(
    const log4c_sink *sink,
    log4c_level level,
    const char *tag,
    const char *file,
    int line,
    const char *func,
    const char *message,
    char **record
) {
    char prefix[LOG4C_BUFFER_CAPACITY];
    char timestamp[LOG4C_TIMESTAMP_CAPACITY];
    char *line_buffer = NULL;
    size_t message_length = 0U;
    size_t line_size = 0U;
    size_t used = 0U;
    const char *color = "";
    const char *reset = "";

    prefix[0] = '\0';
    timestamp[0] = '\0';
    *record = NULL;

    if ((sink->options & LOG4C_OPTION_TIMESTAMP) != 0U) {
        log4c_format_timestamp(timestamp, sizeof(timestamp));
        if (timestamp[0] != '\0') {
            log4c_append(prefix, sizeof(prefix), &used, "[%s] ", timestamp);
        }
    }

    if ((sink->options & LOG4C_OPTION_COLOR) != 0U) {
        color = log4c_level_color(level);
        reset = "\x1b[0m";
    }

    log4c_append(prefix, sizeof(prefix), &used, "%s[%s]%s ", color, log4c_level_name(level), reset);

    if (tag != NULL && tag[0] != '\0') {
        log4c_append(prefix, sizeof(prefix), &used, "[%s] ", tag);
    }

    if (((sink->options & LOG4C_OPTION_LOCATION) != 0U) &&
        file != NULL &&
        func != NULL) {
        log4c_append(prefix, sizeof(prefix), &used, "%s:%d %s: ", file, line, func);
    }

    message_length = strlen(message);
    line_size = strlen(prefix) + message_length + 2U;
    line_buffer = (char *)malloc(line_size);
    if (line_buffer == NULL) {
        return -1;
    }

    if (snprintf(line_buffer, line_size, "%s%s\n", prefix, message) < 0) {
        free(line_buffer);
        return -1;
    }

    *record = line_buffer;
    return 0;
}

static int log4c_emit_to_sink(
    const log4c_sink *sink,
    log4c_level level,
    const char *tag,
    const char *file,
    int line,
    const char *func,
    const char *message
) {
    char *record = NULL;
    int result = 0;

    if (sink == NULL || !sink->enabled || sink->write == NULL) {
        return 0;
    }

    if (log4c_make_record(sink, level, tag, file, line, func, message, &record) != 0) {
        return -1;
    }

    result = sink->write(sink->userdata, record);
    if (result >= 0 &&
        (sink->options & LOG4C_OPTION_AUTO_FLUSH) != 0U &&
        sink->flush != NULL &&
        sink->flush(sink->userdata) != 0) {
        result = -1;
    }

    free(record);
    return result;
}

static int log4c_logger_emit_message(
    log4c_logger *logger,
    log4c_level level,
    const char *tag,
    const char *file,
    int line,
    const char *func,
    const char *message
) {
    int emit_result = 0;
    int result = 0;
    size_t index = 0U;

    if (logger == NULL || message == NULL) {
        return -1;
    }

    if (logger->lock != NULL) {
        logger->lock(logger->lock_userdata);
    }

    for (index = 0U; index < logger->sink_count; ++index) {
        emit_result = log4c_emit_to_sink(
            &logger->sinks[index],
            level,
            tag,
            file,
            line,
            func,
            message
        );
        if (emit_result < 0) {
            result = -1;
            break;
        }

        result += emit_result;
    }

    if (logger->unlock != NULL) {
        logger->unlock(logger->lock_userdata);
    }

    return result;
}

void log4c_logger_init(log4c_logger *logger, FILE *stream) {
    if (logger == NULL) {
        return;
    }

    memset(logger, 0, sizeof(*logger));
    logger->min_level = LOG4C_LEVEL_INFO;
    logger->default_options = LOG4C_OPTION_TIMESTAMP |
                              LOG4C_OPTION_LOCATION |
                              LOG4C_OPTION_AUTO_FLUSH;
    (void)log4c_logger_add_stream_sink(
        logger,
        (stream != NULL) ? stream : stderr,
        logger->default_options
    );
}

bool log4c_logger_init_from_config(log4c_logger *logger, const log4c_config *config) {
    char path[LOG4C_CONFIG_PATH_CAPACITY + LOG4C_CONFIG_NAME_CAPACITY + 2U];
    unsigned int console_options = 0U;
    unsigned int file_options = 0U;
    bool has_sink = false;

    if (logger == NULL || config == NULL) {
        return false;
    }

    log4c_logger_init(logger, stderr);
    log4c_logger_clear_sinks(logger);
    log4c_logger_set_level(logger, config->level);

    console_options = log4c_console_options_from_config(config);
    file_options = log4c_file_options_from_config(config);
    log4c_logger_set_options(logger, file_options);

    if (config->enable_console) {
        has_sink = log4c_logger_add_stream_sink(
            logger,
            (config->console_stream == LOG4C_CONSOLE_STDOUT) ? stdout : stderr,
            console_options
        );
        if (!has_sink) {
            log4c_logger_destroy(logger);
            return false;
        }
    }

    if (config->enable_file) {
        if (!log4c_join_path(path, sizeof(path), config->log_dir, config->file_name)) {
            log4c_logger_destroy(logger);
            return false;
        }

        if (config->rotation_max_bytes > 0U && config->rotation_max_files > 0U) {
            has_sink = log4c_logger_add_rotating_file_sink(
                logger,
                path,
                config->rotation_max_bytes,
                config->rotation_max_files,
                file_options
            );
        } else {
            has_sink = log4c_logger_add_file_sink(logger, path, file_options);
        }

        if (!has_sink) {
            log4c_logger_destroy(logger);
            return false;
        }
    }

    return logger->sink_count > 0U;
}

void log4c_logger_destroy(log4c_logger *logger) {
    if (logger == NULL) {
        return;
    }

    log4c_logger_clear_sinks(logger);
    logger->lock = NULL;
    logger->unlock = NULL;
    logger->lock_userdata = NULL;
}

void log4c_logger_clear_sinks(log4c_logger *logger) {
    size_t index = 0U;

    if (logger == NULL) {
        return;
    }

    for (index = 0U; index < LOG4C_MAX_SINKS; ++index) {
        if (logger->sinks[index].enabled && logger->sinks[index].close != NULL) {
            (void)logger->sinks[index].close(logger->sinks[index].userdata);
        }

        logger->sinks[index].write = NULL;
        logger->sinks[index].flush = NULL;
        logger->sinks[index].close = NULL;
        logger->sinks[index].userdata = NULL;
        logger->sinks[index].options = 0U;
        logger->sinks[index].enabled = false;
    }

    logger->sink_count = 0U;
}

void log4c_logger_set_level(log4c_logger *logger, log4c_level level) {
    if (logger == NULL || !log4c_is_valid_level(level)) {
        return;
    }

    logger->min_level = level;
}

void log4c_logger_set_stream(log4c_logger *logger, FILE *stream) {
    if (logger == NULL || stream == NULL) {
        return;
    }

    log4c_logger_clear_sinks(logger);
    (void)log4c_logger_add_stream_sink(logger, stream, logger->default_options);
}

void log4c_logger_set_options(log4c_logger *logger, unsigned int options) {
    size_t index = 0U;

    if (logger == NULL) {
        return;
    }

    logger->default_options = options;
    for (index = 0U; index < logger->sink_count; ++index) {
        logger->sinks[index].options = options;
    }
}

void log4c_logger_set_lock(
    log4c_logger *logger,
    log4c_lock_fn lock,
    log4c_lock_fn unlock,
    void *userdata
) {
    if (logger == NULL) {
        return;
    }

    logger->lock = lock;
    logger->unlock = unlock;
    logger->lock_userdata = userdata;
}

bool log4c_logger_add_stream_sink(log4c_logger *logger, FILE *stream, unsigned int options) {
    if (stream == NULL) {
        return false;
    }

    return log4c_logger_add_sink_entry(
        logger,
        log4c_stream_sink_write,
        log4c_stream_sink_flush,
        NULL,
        stream,
        options
    );
}

bool log4c_logger_add_callback_sink(
    log4c_logger *logger,
    log4c_sink_write_fn write,
    log4c_sink_flush_fn flush,
    void *userdata,
    unsigned int options
) {
    return log4c_logger_add_sink_entry(logger, write, flush, NULL, userdata, options);
}

bool log4c_logger_add_file_sink(log4c_logger *logger, const char *path, unsigned int options) {
    log4c_file_sink_data *data = NULL;
    bool ok = false;

    if (logger == NULL || path == NULL || path[0] == '\0' || logger->sink_count >= LOG4C_MAX_SINKS) {
        return false;
    }

    data = (log4c_file_sink_data *)malloc(sizeof(*data));
    if (data == NULL) {
        return false;
    }

    if (!log4c_open_file_sink(data, path, 0U, 0U)) {
        free(data);
        return false;
    }

    ok = log4c_logger_add_sink_entry(
        logger,
        log4c_file_sink_write,
        log4c_file_sink_flush,
        log4c_file_sink_close,
        data,
        options
    );
    if (!ok) {
        (void)log4c_file_sink_close(data);
    }

    return ok;
}

bool log4c_logger_add_rotating_file_sink(
    log4c_logger *logger,
    const char *path,
    size_t max_bytes,
    size_t max_files,
    unsigned int options
) {
    log4c_file_sink_data *data = NULL;
    bool ok = false;

    if (logger == NULL ||
        path == NULL ||
        path[0] == '\0' ||
        max_bytes == 0U ||
        max_files == 0U ||
        logger->sink_count >= LOG4C_MAX_SINKS) {
        return false;
    }

    data = (log4c_file_sink_data *)malloc(sizeof(*data));
    if (data == NULL) {
        return false;
    }

    if (!log4c_open_file_sink(data, path, max_bytes, max_files)) {
        free(data);
        return false;
    }

    ok = log4c_logger_add_sink_entry(
        logger,
        log4c_file_sink_write,
        log4c_file_sink_flush,
        log4c_file_sink_close,
        data,
        options
    );
    if (!ok) {
        (void)log4c_file_sink_close(data);
    }

    return ok;
}

size_t log4c_logger_sink_count(const log4c_logger *logger) {
    if (logger == NULL) {
        return 0U;
    }

    return logger->sink_count;
}

bool log4c_logger_should_log(const log4c_logger *logger, log4c_level level) {
    if (logger == NULL || !log4c_is_valid_level(level)) {
        return false;
    }

    if (level == LOG4C_LEVEL_OFF || logger->min_level == LOG4C_LEVEL_OFF) {
        return false;
    }

    return level >= logger->min_level;
}

const char *log4c_level_name(log4c_level level) {
    if (!log4c_is_valid_level(level)) {
        return "UNKNOWN";
    }

    return LOG4C_LEVEL_NAMES[level];
}

void log4c_config_init(log4c_config *config) {
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    (void)log4c_copy_string(config->log_dir, sizeof(config->log_dir), "logs");
    (void)log4c_copy_string(config->file_name, sizeof(config->file_name), "app.log");
    config->level = LOG4C_LEVEL_INFO;
    config->rotation_max_bytes = 0U;
    config->rotation_max_files = 0U;
    config->enable_console = true;
    config->enable_file = false;
    config->color = false;
    config->timestamp = true;
    config->location = true;
    config->auto_flush = true;
    config->console_stream = LOG4C_CONSOLE_STDERR;
}

bool log4c_config_load(log4c_config *config, const char *path) {
    char line[LOG4C_CONFIG_LINE_CAPACITY];
    FILE *stream = NULL;

    if (config == NULL || path == NULL || path[0] == '\0') {
        return false;
    }

    log4c_config_init(config);

    stream = fopen(path, "r");
    if (stream == NULL) {
        return false;
    }

    while (fgets(line, (int)sizeof(line), stream) != NULL) {
        char *sep = NULL;
        char *key = line;
        char *value = NULL;

        log4c_trim(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
            continue;
        }

        sep = strchr(line, '=');
        if (sep == NULL) {
            fclose(stream);
            return false;
        }

        *sep = '\0';
        value = sep + 1;
        log4c_trim(key);
        log4c_trim(value);

        if (log4c_str_ieq(key, "log_dir")) {
            if (!log4c_copy_string(config->log_dir, sizeof(config->log_dir), value)) {
                fclose(stream);
                return false;
            }
            continue;
        }

        if (log4c_str_ieq(key, "file_name")) {
            if (!log4c_copy_string(config->file_name, sizeof(config->file_name), value)) {
                fclose(stream);
                return false;
            }
            continue;
        }

        if (log4c_str_ieq(key, "level")) {
            if (!log4c_parse_level(value, &config->level)) {
                fclose(stream);
                return false;
            }
            continue;
        }

        if (log4c_str_ieq(key, "rotation_max_bytes")) {
            if (!log4c_parse_size(value, &config->rotation_max_bytes)) {
                fclose(stream);
                return false;
            }
            continue;
        }

        if (log4c_str_ieq(key, "rotation_max_files")) {
            if (!log4c_parse_size(value, &config->rotation_max_files)) {
                fclose(stream);
                return false;
            }
            continue;
        }

        if (log4c_str_ieq(key, "console")) {
            if (!log4c_parse_bool(value, &config->enable_console)) {
                fclose(stream);
                return false;
            }
            continue;
        }

        if (log4c_str_ieq(key, "file")) {
            if (!log4c_parse_bool(value, &config->enable_file)) {
                fclose(stream);
                return false;
            }
            continue;
        }

        if (log4c_str_ieq(key, "color")) {
            if (!log4c_parse_bool(value, &config->color)) {
                fclose(stream);
                return false;
            }
            continue;
        }

        if (log4c_str_ieq(key, "timestamp")) {
            if (!log4c_parse_bool(value, &config->timestamp)) {
                fclose(stream);
                return false;
            }
            continue;
        }

        if (log4c_str_ieq(key, "location")) {
            if (!log4c_parse_bool(value, &config->location)) {
                fclose(stream);
                return false;
            }
            continue;
        }

        if (log4c_str_ieq(key, "auto_flush")) {
            if (!log4c_parse_bool(value, &config->auto_flush)) {
                fclose(stream);
                return false;
            }
            continue;
        }

        if (log4c_str_ieq(key, "console_stream")) {
            if (!log4c_parse_console_stream(value, &config->console_stream)) {
                fclose(stream);
                return false;
            }
            continue;
        }

        fclose(stream);
        return false;
    }

    fclose(stream);
    return true;
}

int log4c_logger_vlog(
    log4c_logger *logger,
    log4c_level level,
    const char *tag,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    va_list args
) {
    char *message = NULL;
    int message_size = 0;
    int result = 0;
    va_list measure_args;

    if (logger == NULL || fmt == NULL) {
        return -1;
    }

    if (!log4c_logger_should_log(logger, level)) {
        return 0;
    }

    va_copy(measure_args, args);
    message_size = vsnprintf(NULL, 0U, fmt, measure_args);
    va_end(measure_args);

    if (message_size < 0) {
        return -1;
    }

    message = (char *)malloc((size_t)message_size + 1U);
    if (message == NULL) {
        return -1;
    }

    result = vsnprintf(message, (size_t)message_size + 1U, fmt, args);
    if (result < 0) {
        free(message);
        return -1;
    }
    result = log4c_logger_emit_message(logger, level, tag, file, line, func, message);
    free(message);
    return result;
}

int log4c_logger_log(
    log4c_logger *logger,
    log4c_level level,
    const char *tag,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...
) {
    int result = 0;
    va_list args;

    va_start(args, fmt);
    result = log4c_logger_vlog(logger, level, tag, file, line, func, fmt, args);
    va_end(args);

    return result;
}

#if defined(LOG4C_WITH_PTHREAD)
static void log4c_async_message_free(log4c_async_message *message) {
    if (message == NULL) {
        return;
    }

    free(message->tag);
    free(message->file);
    free(message->func);
    free(message->message);
    free(message);
}

static log4c_async_message *log4c_async_message_create(
    log4c_level level,
    const char *tag,
    const char *file,
    int line,
    const char *func,
    const char *message
) {
    log4c_async_message *entry = NULL;

    entry = (log4c_async_message *)calloc(1U, sizeof(*entry));
    if (entry == NULL) {
        return NULL;
    }

    entry->level = level;
    entry->line = line;
    entry->tag = log4c_strdup_or_null(tag);
    entry->file = log4c_strdup_or_null(file);
    entry->func = log4c_strdup_or_null(func);
    entry->message = log4c_strdup_or_null(message);
    if (message != NULL && entry->message == NULL) {
        log4c_async_message_free(entry);
        return NULL;
    }

    if ((tag != NULL && entry->tag == NULL) ||
        (file != NULL && entry->file == NULL) ||
        (func != NULL && entry->func == NULL)) {
        log4c_async_message_free(entry);
        return NULL;
    }

    return entry;
}

static void log4c_async_report_error(
    log4c_async_logger *wrapper,
    log4c_async_error error,
    const char *detail
) {
    log4c_async_error_fn callback = NULL;
    void *userdata = NULL;

    if (wrapper == NULL) {
        return;
    }

    callback = wrapper->error_callback;
    userdata = wrapper->error_userdata;
    if (callback != NULL) {
        callback(userdata, error, detail);
    }
}

static void *log4c_async_worker_main(void *userdata) {
    log4c_async_logger *wrapper = (log4c_async_logger *)userdata;

    if (wrapper == NULL) {
        return NULL;
    }

    for (;;) {
        log4c_async_message *entry = NULL;

        (void)pthread_mutex_lock(&wrapper->mutex);
        while (!wrapper->stop && wrapper->head == NULL) {
            (void)pthread_cond_wait(&wrapper->not_empty, &wrapper->mutex);
        }

        if (wrapper->stop && wrapper->head == NULL) {
            (void)pthread_mutex_unlock(&wrapper->mutex);
            break;
        }

        entry = wrapper->head;
        wrapper->head = entry->next;
        if (wrapper->head == NULL) {
            wrapper->tail = NULL;
        }
        wrapper->queued -= 1U;
        wrapper->worker_active = true;
        (void)pthread_cond_signal(&wrapper->not_full);
        (void)pthread_mutex_unlock(&wrapper->mutex);

        if (log4c_logger_emit_message(
                &wrapper->logger,
                entry->level,
                entry->tag,
                entry->file,
                entry->line,
                entry->func,
                entry->message
            ) < 0) {
            (void)pthread_mutex_lock(&wrapper->mutex);
            wrapper->worker_failed = true;
            wrapper->stats.write_failures += 1U;
            (void)pthread_mutex_unlock(&wrapper->mutex);
            log4c_async_report_error(wrapper, LOG4C_ASYNC_ERROR_SINK_WRITE, entry->message);
        } else {
            (void)pthread_mutex_lock(&wrapper->mutex);
            wrapper->stats.written += 1U;
            (void)pthread_mutex_unlock(&wrapper->mutex);
        }

        log4c_async_message_free(entry);

        (void)pthread_mutex_lock(&wrapper->mutex);
        wrapper->worker_active = false;
        (void)pthread_cond_broadcast(&wrapper->drained);
        (void)pthread_mutex_unlock(&wrapper->mutex);
    }

    (void)pthread_mutex_lock(&wrapper->mutex);
    wrapper->worker_active = false;
    (void)pthread_cond_broadcast(&wrapper->drained);
    (void)pthread_mutex_unlock(&wrapper->mutex);
    return NULL;
}

static void log4c_pthread_lock(void *userdata) {
    pthread_mutex_t *mutex = (pthread_mutex_t *)userdata;

    if (mutex != NULL) {
        (void)pthread_mutex_lock(mutex);
    }
}

static void log4c_pthread_unlock(void *userdata) {
    pthread_mutex_t *mutex = (pthread_mutex_t *)userdata;

    if (mutex != NULL) {
        (void)pthread_mutex_unlock(mutex);
    }
}

bool log4c_threadsafe_logger_init(log4c_threadsafe_logger *wrapper, FILE *stream) {
    if (wrapper == NULL) {
        return false;
    }

    if (pthread_mutex_init(&wrapper->mutex, NULL) != 0) {
        return false;
    }

    log4c_logger_init(&wrapper->logger, stream);
    log4c_logger_set_lock(&wrapper->logger, log4c_pthread_lock, log4c_pthread_unlock, &wrapper->mutex);
    return true;
}

void log4c_threadsafe_logger_destroy(log4c_threadsafe_logger *wrapper) {
    if (wrapper == NULL) {
        return;
    }

    log4c_logger_destroy(&wrapper->logger);
    (void)pthread_mutex_destroy(&wrapper->mutex);
}

log4c_logger *log4c_threadsafe_logger_get(log4c_threadsafe_logger *wrapper) {
    if (wrapper == NULL) {
        return NULL;
    }

    return &wrapper->logger;
}

const log4c_logger *log4c_threadsafe_logger_cget(const log4c_threadsafe_logger *wrapper) {
    if (wrapper == NULL) {
        return NULL;
    }

    return &wrapper->logger;
}

bool log4c_async_logger_init(log4c_async_logger *wrapper, FILE *stream, size_t max_queue) {
    if (wrapper == NULL || max_queue == 0U) {
        return false;
    }

    memset(wrapper, 0, sizeof(*wrapper));
    if (pthread_mutex_init(&wrapper->mutex, NULL) != 0) {
        return false;
    }

    if (pthread_cond_init(&wrapper->not_empty, NULL) != 0) {
        (void)pthread_mutex_destroy(&wrapper->mutex);
        return false;
    }

    if (pthread_cond_init(&wrapper->not_full, NULL) != 0) {
        (void)pthread_cond_destroy(&wrapper->not_empty);
        (void)pthread_mutex_destroy(&wrapper->mutex);
        return false;
    }

    if (pthread_cond_init(&wrapper->drained, NULL) != 0) {
        (void)pthread_cond_destroy(&wrapper->not_full);
        (void)pthread_cond_destroy(&wrapper->not_empty);
        (void)pthread_mutex_destroy(&wrapper->mutex);
        return false;
    }

    log4c_logger_init(&wrapper->logger, stream);
    wrapper->max_queue = max_queue;
    wrapper->queue_policy = LOG4C_ASYNC_QUEUE_BLOCK;
    wrapper->stats.max_queue = max_queue;

    if (pthread_create(&wrapper->thread, NULL, log4c_async_worker_main, wrapper) != 0) {
        log4c_logger_destroy(&wrapper->logger);
        (void)pthread_cond_destroy(&wrapper->drained);
        (void)pthread_cond_destroy(&wrapper->not_full);
        (void)pthread_cond_destroy(&wrapper->not_empty);
        (void)pthread_mutex_destroy(&wrapper->mutex);
        memset(wrapper, 0, sizeof(*wrapper));
        return false;
    }

    return true;
}

bool log4c_async_logger_init_from_config(
    log4c_async_logger *wrapper,
    const log4c_config *config,
    size_t max_queue
) {
    if (wrapper == NULL || config == NULL || max_queue == 0U) {
        return false;
    }

    if (!log4c_async_logger_init(wrapper, stderr, max_queue)) {
        return false;
    }

    log4c_logger_destroy(&wrapper->logger);
    if (!log4c_logger_init_from_config(&wrapper->logger, config)) {
        log4c_async_logger_destroy(wrapper);
        return false;
    }

    return true;
}

void log4c_async_logger_set_queue_policy(
    log4c_async_logger *wrapper,
    log4c_async_queue_policy policy
) {
    if (wrapper == NULL) {
        return;
    }

    (void)pthread_mutex_lock(&wrapper->mutex);
    wrapper->queue_policy = policy;
    (void)pthread_mutex_unlock(&wrapper->mutex);
}

void log4c_async_logger_set_error_callback(
    log4c_async_logger *wrapper,
    log4c_async_error_fn callback,
    void *userdata
) {
    if (wrapper == NULL) {
        return;
    }

    (void)pthread_mutex_lock(&wrapper->mutex);
    wrapper->error_callback = callback;
    wrapper->error_userdata = userdata;
    (void)pthread_mutex_unlock(&wrapper->mutex);
}

bool log4c_async_logger_get_stats(
    log4c_async_logger *wrapper,
    log4c_async_stats *stats
) {
    if (wrapper == NULL || stats == NULL) {
        return false;
    }

    (void)pthread_mutex_lock(&wrapper->mutex);
    *stats = wrapper->stats;
    stats->current_queue = wrapper->queued;
    stats->max_queue = wrapper->max_queue;
    (void)pthread_mutex_unlock(&wrapper->mutex);
    return true;
}

bool log4c_async_logger_flush(log4c_async_logger *wrapper) {
    bool ok = true;

    if (wrapper == NULL) {
        return false;
    }

    (void)pthread_mutex_lock(&wrapper->mutex);
    while (wrapper->queued > 0U || wrapper->worker_active) {
        (void)pthread_cond_wait(&wrapper->drained, &wrapper->mutex);
    }
    ok = !wrapper->worker_failed;
    (void)pthread_mutex_unlock(&wrapper->mutex);
    return ok;
}

void log4c_async_logger_destroy(log4c_async_logger *wrapper) {
    log4c_async_message *entry = NULL;
    log4c_async_message *next = NULL;

    if (wrapper == NULL) {
        return;
    }

    (void)pthread_mutex_lock(&wrapper->mutex);
    wrapper->stop = true;
    (void)pthread_cond_broadcast(&wrapper->not_empty);
    (void)pthread_cond_broadcast(&wrapper->not_full);
    (void)pthread_mutex_unlock(&wrapper->mutex);

    if (wrapper->thread != 0U) {
        (void)pthread_join(wrapper->thread, NULL);
    }

    entry = wrapper->head;
    while (entry != NULL) {
        next = entry->next;
        log4c_async_message_free(entry);
        entry = next;
    }

    wrapper->head = NULL;
    wrapper->tail = NULL;
    wrapper->queued = 0U;
    log4c_logger_destroy(&wrapper->logger);
    (void)pthread_cond_destroy(&wrapper->drained);
    (void)pthread_cond_destroy(&wrapper->not_full);
    (void)pthread_cond_destroy(&wrapper->not_empty);
    (void)pthread_mutex_destroy(&wrapper->mutex);
    memset(wrapper, 0, sizeof(*wrapper));
}

int log4c_async_logger_vlog(
    log4c_async_logger *wrapper,
    log4c_level level,
    const char *tag,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    va_list args
) {
    log4c_async_message *entry = NULL;
    char *message = NULL;
    int message_size = 0;
    va_list measure_args;

    if (wrapper == NULL || fmt == NULL) {
        return -1;
    }

    if (!log4c_logger_should_log(&wrapper->logger, level)) {
        return 0;
    }

    va_copy(measure_args, args);
    message_size = vsnprintf(NULL, 0U, fmt, measure_args);
    va_end(measure_args);
    if (message_size < 0) {
        return -1;
    }

    message = (char *)malloc((size_t)message_size + 1U);
    if (message == NULL) {
        (void)pthread_mutex_lock(&wrapper->mutex);
        wrapper->stats.alloc_failures += 1U;
        (void)pthread_mutex_unlock(&wrapper->mutex);
        log4c_async_report_error(wrapper, LOG4C_ASYNC_ERROR_ALLOC, fmt);
        return -1;
    }

    if (vsnprintf(message, (size_t)message_size + 1U, fmt, args) < 0) {
        free(message);
        return -1;
    }

    entry = log4c_async_message_create(level, tag, file, line, func, message);
    free(message);
    if (entry == NULL) {
        (void)pthread_mutex_lock(&wrapper->mutex);
        wrapper->stats.alloc_failures += 1U;
        (void)pthread_mutex_unlock(&wrapper->mutex);
        log4c_async_report_error(wrapper, LOG4C_ASYNC_ERROR_ALLOC, fmt);
        return -1;
    }

    (void)pthread_mutex_lock(&wrapper->mutex);
    while (!wrapper->stop && wrapper->queued >= wrapper->max_queue) {
        log4c_async_message *dropped = NULL;

        if (wrapper->queue_policy == LOG4C_ASYNC_QUEUE_DROP_NEWEST) {
            wrapper->stats.queue_full += 1U;
            wrapper->stats.dropped += 1U;
            (void)pthread_mutex_unlock(&wrapper->mutex);
            log4c_async_report_error(wrapper, LOG4C_ASYNC_ERROR_QUEUE_FULL, entry->message);
            log4c_async_message_free(entry);
            return 0;
        }

        if (wrapper->queue_policy == LOG4C_ASYNC_QUEUE_DROP_OLDEST) {
            wrapper->stats.queue_full += 1U;
            dropped = wrapper->head;
            wrapper->head = dropped->next;
            if (wrapper->head == NULL) {
                wrapper->tail = NULL;
            }
            wrapper->queued -= 1U;
            wrapper->stats.dropped += 1U;
            (void)pthread_mutex_unlock(&wrapper->mutex);
            log4c_async_report_error(wrapper, LOG4C_ASYNC_ERROR_QUEUE_FULL, dropped->message);
            log4c_async_message_free(dropped);
            (void)pthread_mutex_lock(&wrapper->mutex);
            continue;
        }

        (void)pthread_cond_wait(&wrapper->not_full, &wrapper->mutex);
    }

    if (wrapper->stop) {
        (void)pthread_mutex_unlock(&wrapper->mutex);
        log4c_async_message_free(entry);
        return -1;
    }

    if (wrapper->tail != NULL) {
        wrapper->tail->next = entry;
    } else {
        wrapper->head = entry;
    }
    wrapper->tail = entry;
    wrapper->queued += 1U;
    wrapper->stats.enqueued += 1U;
    (void)pthread_cond_signal(&wrapper->not_empty);
    (void)pthread_mutex_unlock(&wrapper->mutex);
    return message_size;
}

int log4c_async_logger_log(
    log4c_async_logger *wrapper,
    log4c_level level,
    const char *tag,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...
) {
    int result = 0;
    va_list args;

    va_start(args, fmt);
    result = log4c_async_logger_vlog(wrapper, level, tag, file, line, func, fmt, args);
    va_end(args);
    return result;
}
#endif
