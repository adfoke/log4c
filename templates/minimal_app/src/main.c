#include <log4c/log4c.h>

#include <stdio.h>

int main(void) {
    log4c_config config;
    log4c_logger logger;

    log4c_config_init(&config);
    if (!log4c_config_load(&config, "log4c.conf")) {
        fprintf(stderr, "failed to load log4c.conf\n");
        return 1;
    }

    if (!log4c_logger_init_from_config(&logger, &config)) {
        fprintf(stderr, "failed to init logger\n");
        return 1;
    }

    LOG4C_INFO(&logger, "app", "my_app started");
    LOG4C_WARN(&logger, "config", "template app is using log4c");
    LOG4C_ERROR(&logger, "demo", "sample error log");

    log4c_logger_destroy(&logger);
    return 0;
}
