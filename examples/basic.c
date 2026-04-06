#include <log4c/log4c.h>

#include <stdio.h>

int main(void) {
    log4c_logger logger;

    log4c_logger_init(&logger, stderr);
    log4c_logger_set_level(&logger, LOG4C_LEVEL_DEBUG);
    log4c_logger_add_stream_sink(
        &logger,
        stdout,
        LOG4C_OPTION_TIMESTAMP | LOG4C_OPTION_AUTO_FLUSH
    );

    LOG4C_INFO(&logger, "app", "service started on port %d", 8080);
    LOG4C_DEBUG(&logger, "worker", "jobs in queue: %d", 3);
    LOG4C_WARN(&logger, "config", "using fallback config file");
    LOG4C_ERROR(&logger, "db", "connection failed: %s", "timeout");

    return 0;
}
