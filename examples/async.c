#include <log4c/log4c.h>

#include <stdio.h>

int main(void) {
#if defined(LOG4C_WITH_PTHREAD)
    log4c_async_logger logger;

    if (!log4c_async_logger_init(&logger, stderr, 128U)) {
        fprintf(stderr, "failed to init async logger\n");
        return 1;
    }

    log4c_logger_set_level(&logger.logger, LOG4C_LEVEL_DEBUG);
    LOG4C_ASYNC_INFO(&logger, "app", "async logger started");
    LOG4C_ASYNC_DEBUG(&logger, "worker", "message was queued");
    LOG4C_ASYNC_ERROR(&logger, "demo", "background thread writes this");

    if (!log4c_async_logger_flush(&logger)) {
        fprintf(stderr, "async logger flush failed\n");
        log4c_async_logger_destroy(&logger);
        return 1;
    }

    log4c_async_logger_destroy(&logger);
    return 0;
#else
    fprintf(stderr, "pthread support is not available\n");
    return 1;
#endif
}
