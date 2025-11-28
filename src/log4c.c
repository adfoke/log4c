#include "log4c.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* ANSI color codes */
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"

/* Level names */
static const char* level_names[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

/* Level colors */
static const char* level_colors[] = {
    COLOR_CYAN,    /* DEBUG */
    COLOR_GREEN,   /* INFO */
    COLOR_YELLOW,  /* WARN */
    COLOR_RED,     /* ERROR */
    COLOR_MAGENTA  /* FATAL */
};

/* Global logger state */
static struct {
    log_config_t config;
    FILE* file;
    pthread_mutex_t mutex;
    bool initialized;
} logger = {
    .config = {
        .level = LOG_LEVEL_INFO,
        .outputs = LOG_OUTPUT_CONSOLE,
        .filename = "",
        .colors_enabled = true,
        .thread_safe = true
    },
    .file = NULL,
    .initialized = false
};

/* Internal functions */

/* Get current time string */
static void get_time_string(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* Check if terminal supports colors */
static bool is_color_supported(void) {
    return isatty(STDOUT_FILENO) != 0;
}

/* Thread safety lock */
static void lock(void) {
    if (logger.config.thread_safe) {
        pthread_mutex_lock(&logger.mutex);
    }
}

static void unlock(void) {
    if (logger.config.thread_safe) {
        pthread_mutex_unlock(&logger.mutex);
    }
}

/* Output to console */
static void output_to_console(log_level_t level, const char* message) {
    FILE* stream = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;

    if (logger.config.colors_enabled && is_color_supported()) {
        fprintf(stream, "%s%s%s", level_colors[level], message, COLOR_RESET);
    } else {
        fprintf(stream, "%s", message);
    }
}

/* Output to file */
static void output_to_file(const char* message) {
    if (logger.file) {
        fprintf(logger.file, "%s", message);
        fflush(logger.file);
    }
}

/* Public API implementation */

int log_init(const log_config_t* config) {
    if (logger.initialized) {
        return -1; /* Already initialized */
    }

    if (config) {
        logger.config = *config;
    }

    /* Initialize mutex */
    if (logger.config.thread_safe) {
        if (pthread_mutex_init(&logger.mutex, NULL) != 0) {
            return -2; /* Mutex initialization failed */
        }
    }

    /* Open log file */
    if (logger.config.outputs & LOG_OUTPUT_FILE) {
        if (strlen(logger.config.filename) > 0) {
            logger.file = fopen(logger.config.filename, "a");
            if (!logger.file) {
                if (logger.config.thread_safe) {
                    pthread_mutex_destroy(&logger.mutex);
                }
                return -3; /* File open failed */
            }
        }
    }

    logger.initialized = true;
    return 0;
}

void log_cleanup(void) {
    if (!logger.initialized) {
        return;
    }

    lock();

    if (logger.file) {
        fclose(logger.file);
        logger.file = NULL;
    }

    unlock();

    if (logger.config.thread_safe) {
        pthread_mutex_destroy(&logger.mutex);
    }

    logger.initialized = false;
}

void log_set_level(log_level_t level) {
    if (level >= LOG_LEVEL_COUNT) {
        return;
    }
    lock();
    logger.config.level = level;
    unlock();
}

void log_set_outputs(int outputs) {
    lock();
    logger.config.outputs = outputs;
    unlock();
}

int log_set_file(const char* filename) {
    if (!filename || strlen(filename) >= sizeof(logger.config.filename)) {
        return -1;
    }

    lock();

    /* Close existing file */
    if (logger.file) {
        fclose(logger.file);
        logger.file = NULL;
    }

    /* Copy filename */
    strcpy(logger.config.filename, filename);

    /* Open new file */
    if (logger.config.outputs & LOG_OUTPUT_FILE) {
        logger.file = fopen(filename, "a");
        if (!logger.file) {
            unlock();
            return -2;
        }
    }

    unlock();
    return 0;
}

void log_enable_colors(bool enable) {
    lock();
    logger.config.colors_enabled = enable;
    unlock();
}

void log_log(log_level_t level, const char* file, int line, const char* func, const char* format, ...) {
    if (!logger.initialized || level < logger.config.level) {
        return;
    }

    lock();

    /* Format time */
    char time_str[20];
    get_time_string(time_str, sizeof(time_str));

    /* Extract filename */
    const char* filename = strrchr(file, '/');
    if (!filename) {
        filename = strrchr(file, '\\');
    }
    filename = filename ? filename + 1 : file;

    /* Build message */
    char message[4096];
    int offset = snprintf(message, sizeof(message), "[%s] [%s] [%s:%d %s] ",
                         time_str, level_names[level], filename, line, func);

    if (offset < 0 || offset >= sizeof(message)) {
        unlock();
        return;
    }

    /* Add user message */
    va_list args;
    va_start(args, format);
    vsnprintf(message + offset, sizeof(message) - offset, format, args);
    va_end(args);

    /* Add newline */
    strncat(message, "\n", sizeof(message) - strlen(message) - 1);

    /* Output */
    if (logger.config.outputs & LOG_OUTPUT_CONSOLE) {
        output_to_console(level, message);
    }

    if (logger.config.outputs & LOG_OUTPUT_FILE) {
        output_to_file(message);
    }

    unlock();
}
