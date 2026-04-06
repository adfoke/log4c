#include <log4c/log4c.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(LOG4C_WITH_PTHREAD)
#include <pthread.h>
#endif

typedef struct memory_sink {
    char buffer[2048];
    size_t used;
} memory_sink;

static int contains_text(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}

static void remove_log_files(const char *path, size_t max_files) {
    char backup[256];
    size_t index = 0U;

    (void)remove(path);
    for (index = 1U; index <= max_files; ++index) {
        if (snprintf(backup, sizeof(backup), "%s.%zu", path, index) > 0) {
            (void)remove(backup);
        }
    }
}

static void remove_path_chain(const char *path) {
    (void)remove(path);
}

static int read_file_text(const char *path, char *buffer, size_t capacity) {
    FILE *stream = NULL;
    size_t used = 0U;
    size_t nread = 0U;

    if (path == NULL || buffer == NULL || capacity == 0U) {
        return 1;
    }

    stream = fopen(path, "r");
    if (stream == NULL) {
        return 1;
    }

    buffer[0] = '\0';
    while (used + 1U < capacity) {
        nread = fread(buffer + used, 1U, capacity - used - 1U, stream);
        used += nread;
        if (nread == 0U) {
            break;
        }
    }

    buffer[used] = '\0';
    fclose(stream);
    return 0;
}

static int memory_sink_write(void *userdata, const char *record) {
    memory_sink *sink = (memory_sink *)userdata;
    size_t remaining = 0U;
    int written = 0;

    if (sink == NULL || record == NULL || sink->used >= sizeof(sink->buffer)) {
        return -1;
    }

    remaining = sizeof(sink->buffer) - sink->used;
    written = snprintf(sink->buffer + sink->used, remaining, "%s", record);
    if (written < 0 || (size_t)written >= remaining) {
        return -1;
    }

    sink->used += (size_t)written;
    return written;
}

static int test_config_load_and_init(void) {
    const char *config_path = "log4c_test.conf";
    const char *log_path = "log4c_cfg/logs/app.log";
    char buffer[1024];
    FILE *stream = NULL;
    log4c_logger logger;
    log4c_config config;

    remove_path_chain(config_path);
    remove_path_chain("log4c_cfg/logs/app.log");
    remove_path_chain("log4c_cfg/logs");
    remove_path_chain("log4c_cfg");

    stream = fopen(config_path, "w");
    if (stream == NULL) {
        return 1;
    }

    if (fprintf(
            stream,
            "log_dir=log4c_cfg/logs\n"
            "file_name=app.log\n"
            "level=error\n"
            "rotation_max_bytes=64\n"
            "rotation_max_files=2\n"
            "console=false\n"
            "file=true\n"
            "color=false\n"
            "timestamp=false\n"
            "location=false\n"
            "auto_flush=true\n"
            "console_stream=stderr\n"
        ) < 0) {
        fclose(stream);
        remove_path_chain(config_path);
        return 1;
    }

    fclose(stream);

    log4c_config_init(&config);
    if (!log4c_config_load(&config, config_path)) {
        remove_path_chain(config_path);
        return 1;
    }

    if (config.level != LOG4C_LEVEL_ERROR ||
        config.rotation_max_bytes != 64U ||
        config.rotation_max_files != 2U ||
        config.enable_console ||
        !config.enable_file) {
        remove_path_chain(config_path);
        return 1;
    }

    if (!log4c_logger_init_from_config(&logger, &config)) {
        remove_path_chain(config_path);
        return 1;
    }

    if (LOG4C_INFO(&logger, "cfg", "hidden") != 0) {
        log4c_logger_destroy(&logger);
        remove_path_chain(config_path);
        return 1;
    }

    if (LOG4C_ERROR(&logger, "cfg", "visible") <= 0) {
        log4c_logger_destroy(&logger);
        remove_path_chain(config_path);
        return 1;
    }

    log4c_logger_destroy(&logger);

    if (read_file_text(log_path, buffer, sizeof(buffer)) != 0) {
        remove_path_chain(config_path);
        remove_path_chain("log4c_cfg/logs/app.log");
        remove_path_chain("log4c_cfg/logs");
        remove_path_chain("log4c_cfg");
        return 1;
    }

    remove_path_chain(config_path);
    remove_path_chain("log4c_cfg/logs/app.log");
    remove_path_chain("log4c_cfg/logs");
    remove_path_chain("log4c_cfg");
    return contains_text(buffer, "visible") && !contains_text(buffer, "hidden") ? 0 : 1;
}

static int test_config_load_resets_defaults(void) {
    const char *config_a_path = "log4c_test_a.conf";
    const char *config_b_path = "log4c_test_b.conf";
    FILE *stream = NULL;
    log4c_config config;

    stream = fopen(config_a_path, "w");
    if (stream == NULL) {
        return 1;
    }

    if (fprintf(stream, "file=true\nrotation_max_bytes=123\nfile_name=custom.log\n") < 0) {
        fclose(stream);
        remove_path_chain(config_a_path);
        return 1;
    }
    fclose(stream);

    stream = fopen(config_b_path, "w");
    if (stream == NULL) {
        remove_path_chain(config_a_path);
        return 1;
    }

    if (fprintf(stream, "level=warn\n") < 0) {
        fclose(stream);
        remove_path_chain(config_a_path);
        remove_path_chain(config_b_path);
        return 1;
    }
    fclose(stream);

    log4c_config_init(&config);
    if (!log4c_config_load(&config, config_a_path)) {
        remove_path_chain(config_a_path);
        remove_path_chain(config_b_path);
        return 1;
    }

    if (!log4c_config_load(&config, config_b_path)) {
        remove_path_chain(config_a_path);
        remove_path_chain(config_b_path);
        return 1;
    }

    remove_path_chain(config_a_path);
    remove_path_chain(config_b_path);

    return config.level == LOG4C_LEVEL_WARN &&
           config.rotation_max_bytes == 0U &&
           config.rotation_max_files == 0U &&
           !config.enable_file &&
           contains_text(config.file_name, "app.log") ? 0 : 1;
}

static int test_config_file_sink_does_not_use_color(void) {
    const char *path = "log4c_color_test.log";
    char buffer[1024];
    log4c_logger logger;
    log4c_config config;

    remove_path_chain(path);

    log4c_config_init(&config);
    config.enable_console = false;
    config.enable_file = true;
    config.color = true;
    config.timestamp = false;
    config.location = false;
    config.level = LOG4C_LEVEL_TRACE;
    if (snprintf(config.log_dir, sizeof(config.log_dir), ".") < 0) {
        return 1;
    }
    if (snprintf(config.file_name, sizeof(config.file_name), "%s", path) < 0) {
        return 1;
    }

    if (!log4c_logger_init_from_config(&logger, &config)) {
        remove_path_chain(path);
        return 1;
    }

    if (LOG4C_ERROR(&logger, "cfg", "plain file") <= 0) {
        log4c_logger_destroy(&logger);
        remove_path_chain(path);
        return 1;
    }

    log4c_logger_destroy(&logger);

    if (read_file_text(path, buffer, sizeof(buffer)) != 0) {
        remove_path_chain(path);
        return 1;
    }

    remove_path_chain(path);
    return contains_text(buffer, "plain file") && !contains_text(buffer, "\x1b[") ? 0 : 1;
}

static int test_basic_log_output(void) {
    char buffer[1024];
    FILE *stream = tmpfile();
    log4c_logger logger;

    if (stream == NULL) {
        return 1;
    }

    log4c_logger_init(&logger, stream);
    log4c_logger_set_level(&logger, LOG4C_LEVEL_TRACE);
    log4c_logger_set_options(&logger, LOG4C_OPTION_LOCATION | LOG4C_OPTION_AUTO_FLUSH);

    if (LOG4C_INFO(&logger, "core", "hello %d", 7) <= 0) {
        fclose(stream);
        return 1;
    }

    rewind(stream);
    if (fgets(buffer, (int)sizeof(buffer), stream) == NULL) {
        fclose(stream);
        return 1;
    }

    fclose(stream);

    if (!contains_text(buffer, "[INFO]")) {
        return 1;
    }

    if (!contains_text(buffer, "[core]")) {
        return 1;
    }

    if (!contains_text(buffer, "hello 7")) {
        return 1;
    }

    if (!contains_text(buffer, "test_basic_log_output")) {
        return 1;
    }

    return 0;
}

static int test_multi_sink_output(void) {
    char buffer[1024];
    FILE *stream = tmpfile();
    log4c_logger logger;
    memory_sink sink = {{0}, 0U};

    if (stream == NULL) {
        return 1;
    }

    log4c_logger_init(&logger, stream);
    log4c_logger_set_level(&logger, LOG4C_LEVEL_TRACE);
    log4c_logger_clear_sinks(&logger);

    if (!log4c_logger_add_stream_sink(&logger, stream, LOG4C_OPTION_AUTO_FLUSH)) {
        fclose(stream);
        return 1;
    }

    if (!log4c_logger_add_callback_sink(&logger, memory_sink_write, NULL, &sink, LOG4C_OPTION_AUTO_FLUSH)) {
        fclose(stream);
        return 1;
    }

    if (log4c_logger_sink_count(&logger) != 2U) {
        fclose(stream);
        return 1;
    }

    if (LOG4C_WARN(&logger, "fanout", "broadcast") <= 0) {
        fclose(stream);
        return 1;
    }

    rewind(stream);
    if (fgets(buffer, (int)sizeof(buffer), stream) == NULL) {
        fclose(stream);
        return 1;
    }

    fclose(stream);

    if (!contains_text(buffer, "[WARN]") || !contains_text(buffer, "broadcast")) {
        return 1;
    }

    if (!contains_text(sink.buffer, "[WARN]") || !contains_text(sink.buffer, "broadcast")) {
        return 1;
    }

    return 0;
}

static int test_file_sink_output(void) {
    const char *path = "log4c_test_file_sink.log";
    char buffer[1024];
    FILE *stream = NULL;
    log4c_logger logger;

    (void)remove(path);

    log4c_logger_init(&logger, stderr);
    log4c_logger_clear_sinks(&logger);
    log4c_logger_set_level(&logger, LOG4C_LEVEL_TRACE);

    if (!log4c_logger_add_file_sink(&logger, path, LOG4C_OPTION_AUTO_FLUSH)) {
        log4c_logger_destroy(&logger);
        return 1;
    }

    if (LOG4C_ERROR(&logger, "file", "persist me") <= 0) {
        log4c_logger_destroy(&logger);
        (void)remove(path);
        return 1;
    }

    log4c_logger_destroy(&logger);

    stream = fopen(path, "r");
    if (stream == NULL) {
        (void)remove(path);
        return 1;
    }

    if (fgets(buffer, (int)sizeof(buffer), stream) == NULL) {
        fclose(stream);
        (void)remove(path);
        return 1;
    }

    fclose(stream);
    (void)remove(path);

    if (!contains_text(buffer, "[ERROR]")) {
        return 1;
    }

    if (!contains_text(buffer, "persist me")) {
        return 1;
    }

    return 0;
}

static int test_rotating_file_sink_output(void) {
    const char *path = "log4c_test_rotate.log";
    char base[256];
    char backup1[256];
    char backup2[256];
    log4c_logger logger;

    remove_log_files(path, 3U);

    log4c_logger_init(&logger, stderr);
    log4c_logger_clear_sinks(&logger);
    log4c_logger_set_level(&logger, LOG4C_LEVEL_TRACE);

    if (!log4c_logger_add_rotating_file_sink(&logger, path, 24U, 2U, LOG4C_OPTION_AUTO_FLUSH)) {
        log4c_logger_destroy(&logger);
        remove_log_files(path, 3U);
        return 1;
    }

    if (LOG4C_INFO(&logger, "r", "alpha") <= 0 ||
        LOG4C_INFO(&logger, "r", "bravo") <= 0 ||
        LOG4C_INFO(&logger, "r", "charlie") <= 0) {
        log4c_logger_destroy(&logger);
        remove_log_files(path, 3U);
        return 1;
    }

    log4c_logger_destroy(&logger);

    if (read_file_text(path, base, sizeof(base)) != 0) {
        remove_log_files(path, 3U);
        return 1;
    }
    if (!contains_text(base, "charlie") || contains_text(base, "alpha") || contains_text(base, "bravo")) {
        remove_log_files(path, 3U);
        return 1;
    }

    if (read_file_text("log4c_test_rotate.log.1", backup1, sizeof(backup1)) != 0) {
        remove_log_files(path, 3U);
        return 1;
    }
    if (!contains_text(backup1, "bravo") || contains_text(backup1, "alpha") || contains_text(backup1, "charlie")) {
        remove_log_files(path, 3U);
        return 1;
    }

    if (read_file_text("log4c_test_rotate.log.2", backup2, sizeof(backup2)) != 0) {
        remove_log_files(path, 3U);
        return 1;
    }
    if (!contains_text(backup2, "alpha") || contains_text(backup2, "bravo") || contains_text(backup2, "charlie")) {
        remove_log_files(path, 3U);
        return 1;
    }

    remove_log_files(path, 3U);
    return 0;
}

static int test_file_sink_creates_parent_dirs(void) {
    const char *path = "log4c_test_dir/nested/app.log";
    char buffer[1024];
    log4c_logger logger;

    remove_path_chain("log4c_test_dir/nested/app.log");
    remove_path_chain("log4c_test_dir/nested");
    remove_path_chain("log4c_test_dir");

    log4c_logger_init(&logger, stderr);
    log4c_logger_clear_sinks(&logger);
    log4c_logger_set_level(&logger, LOG4C_LEVEL_TRACE);

    if (!log4c_logger_add_file_sink(&logger, path, LOG4C_OPTION_AUTO_FLUSH)) {
        log4c_logger_destroy(&logger);
        remove_path_chain("log4c_test_dir/nested/app.log");
        remove_path_chain("log4c_test_dir/nested");
        remove_path_chain("log4c_test_dir");
        return 1;
    }

    if (LOG4C_INFO(&logger, "dir", "created") <= 0) {
        log4c_logger_destroy(&logger);
        remove_path_chain("log4c_test_dir/nested/app.log");
        remove_path_chain("log4c_test_dir/nested");
        remove_path_chain("log4c_test_dir");
        return 1;
    }

    log4c_logger_destroy(&logger);

    if (read_file_text(path, buffer, sizeof(buffer)) != 0) {
        remove_path_chain("log4c_test_dir/nested/app.log");
        remove_path_chain("log4c_test_dir/nested");
        remove_path_chain("log4c_test_dir");
        return 1;
    }

    remove_path_chain("log4c_test_dir/nested/app.log");
    remove_path_chain("log4c_test_dir/nested");
    remove_path_chain("log4c_test_dir");
    return contains_text(buffer, "created") ? 0 : 1;
}

static int test_level_filtering(void) {
    FILE *stream = tmpfile();
    log4c_logger logger;
    long size = 0;

    if (stream == NULL) {
        return 1;
    }

    log4c_logger_init(&logger, stream);
    log4c_logger_set_level(&logger, LOG4C_LEVEL_ERROR);
    log4c_logger_set_options(&logger, 0U);

    if (LOG4C_INFO(&logger, "core", "hidden message") != 0) {
        fclose(stream);
        return 1;
    }

    if (fflush(stream) != 0) {
        fclose(stream);
        return 1;
    }

    if (fseek(stream, 0L, SEEK_END) != 0) {
        fclose(stream);
        return 1;
    }

    size = ftell(stream);
    fclose(stream);

    return size == 0L ? 0 : 1;
}

#if defined(LOG4C_WITH_PTHREAD)
typedef struct thread_ctx {
    log4c_threadsafe_logger *wrapper;
    int index;
} thread_ctx;

typedef struct async_thread_ctx {
    log4c_async_logger *wrapper;
    int index;
} async_thread_ctx;

static void *thread_writer(void *userdata) {
    thread_ctx *ctx = (thread_ctx *)userdata;
    int iteration = 0;

    for (iteration = 0; iteration < 50; ++iteration) {
        (void)LOG4C_INFO(
            log4c_threadsafe_logger_get(ctx->wrapper),
            "thread",
            "worker %d round %d",
            ctx->index,
            iteration
        );
    }

    return NULL;
}

static int test_threadsafe_wrapper(void) {
    FILE *stream = tmpfile();
    log4c_threadsafe_logger wrapper;
    pthread_t threads[4];
    thread_ctx ctx[4];
    char line[256];
    int index = 0;
    int line_count = 0;

    if (stream == NULL) {
        return 1;
    }

    if (!log4c_threadsafe_logger_init(&wrapper, stream)) {
        fclose(stream);
        return 1;
    }

    log4c_logger_set_level(log4c_threadsafe_logger_get(&wrapper), LOG4C_LEVEL_TRACE);
    log4c_logger_set_options(log4c_threadsafe_logger_get(&wrapper), LOG4C_OPTION_AUTO_FLUSH);

    for (index = 0; index < 4; ++index) {
        ctx[index].wrapper = &wrapper;
        ctx[index].index = index;
        if (pthread_create(&threads[index], NULL, thread_writer, &ctx[index]) != 0) {
            log4c_threadsafe_logger_destroy(&wrapper);
            fclose(stream);
            return 1;
        }
    }

    for (index = 0; index < 4; ++index) {
        if (pthread_join(threads[index], NULL) != 0) {
            log4c_threadsafe_logger_destroy(&wrapper);
            fclose(stream);
            return 1;
        }
    }

    rewind(stream);
    while (fgets(line, (int)sizeof(line), stream) != NULL) {
        line_count += 1;
    }

    log4c_threadsafe_logger_destroy(&wrapper);
    fclose(stream);
    return line_count == 200 ? 0 : 1;
}

static void *async_thread_writer(void *userdata) {
    async_thread_ctx *ctx = (async_thread_ctx *)userdata;
    int iteration = 0;

    for (iteration = 0; iteration < 50; ++iteration) {
        (void)LOG4C_ASYNC_INFO(
            ctx->wrapper,
            "async",
            "worker %d round %d",
            ctx->index,
            iteration
        );
    }

    return NULL;
}

static int test_async_logger_wrapper(void) {
    FILE *stream = tmpfile();
    log4c_async_logger wrapper;
    pthread_t threads[4];
    async_thread_ctx ctx[4];
    char line[256];
    int index = 0;
    int line_count = 0;

    if (stream == NULL) {
        return 1;
    }

    if (!log4c_async_logger_init(&wrapper, stream, 32U)) {
        fclose(stream);
        return 1;
    }

    log4c_logger_set_level(&wrapper.logger, LOG4C_LEVEL_TRACE);
    log4c_logger_set_options(&wrapper.logger, LOG4C_OPTION_AUTO_FLUSH);

    for (index = 0; index < 4; ++index) {
        ctx[index].wrapper = &wrapper;
        ctx[index].index = index;
        if (pthread_create(&threads[index], NULL, async_thread_writer, &ctx[index]) != 0) {
            log4c_async_logger_destroy(&wrapper);
            fclose(stream);
            return 1;
        }
    }

    for (index = 0; index < 4; ++index) {
        if (pthread_join(threads[index], NULL) != 0) {
            log4c_async_logger_destroy(&wrapper);
            fclose(stream);
            return 1;
        }
    }

    if (!log4c_async_logger_flush(&wrapper)) {
        log4c_async_logger_destroy(&wrapper);
        fclose(stream);
        return 1;
    }

    rewind(stream);
    while (fgets(line, (int)sizeof(line), stream) != NULL) {
        line_count += 1;
    }

    log4c_async_logger_destroy(&wrapper);
    fclose(stream);
    return line_count == 200 ? 0 : 1;
}

static int test_async_logger_destroy_drains_queue(void) {
    const char *path = "log4c_async.log";
    char buffer[2048];
    log4c_config config;
    log4c_async_logger wrapper;
    int index = 0;

    remove_path_chain(path);

    log4c_config_init(&config);
    config.enable_console = false;
    config.enable_file = true;
    config.level = LOG4C_LEVEL_TRACE;
    config.timestamp = false;
    config.location = false;
    if (snprintf(config.log_dir, sizeof(config.log_dir), ".") < 0) {
        return 1;
    }
    if (snprintf(config.file_name, sizeof(config.file_name), "%s", path) < 0) {
        return 1;
    }

    if (!log4c_async_logger_init_from_config(&wrapper, &config, 8U)) {
        remove_path_chain(path);
        return 1;
    }

    for (index = 0; index < 20; ++index) {
        if (LOG4C_ASYNC_INFO(&wrapper, "async", "queued %d", index) < 0) {
            log4c_async_logger_destroy(&wrapper);
            remove_path_chain(path);
            return 1;
        }
    }

    log4c_async_logger_destroy(&wrapper);

    if (read_file_text(path, buffer, sizeof(buffer)) != 0) {
        remove_path_chain(path);
        return 1;
    }

    remove_path_chain(path);
    return contains_text(buffer, "queued 0") && contains_text(buffer, "queued 19") ? 0 : 1;
}
#endif

int main(void) {
    if (test_basic_log_output() != 0) {
        return EXIT_FAILURE;
    }

    if (test_multi_sink_output() != 0) {
        return EXIT_FAILURE;
    }

    if (test_config_load_and_init() != 0) {
        return EXIT_FAILURE;
    }

    if (test_config_load_resets_defaults() != 0) {
        return EXIT_FAILURE;
    }

    if (test_config_file_sink_does_not_use_color() != 0) {
        return EXIT_FAILURE;
    }

    if (test_file_sink_output() != 0) {
        return EXIT_FAILURE;
    }

    if (test_rotating_file_sink_output() != 0) {
        return EXIT_FAILURE;
    }

    if (test_file_sink_creates_parent_dirs() != 0) {
        return EXIT_FAILURE;
    }

    if (test_level_filtering() != 0) {
        return EXIT_FAILURE;
    }

#if defined(LOG4C_WITH_PTHREAD)
    if (test_threadsafe_wrapper() != 0) {
        return EXIT_FAILURE;
    }

    if (test_async_logger_wrapper() != 0) {
        return EXIT_FAILURE;
    }

    if (test_async_logger_destroy_drains_queue() != 0) {
        return EXIT_FAILURE;
    }
#endif

    return EXIT_SUCCESS;
}
