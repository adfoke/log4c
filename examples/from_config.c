#include <log4c/log4c.h>

#include <stdio.h>

int main(void) {
    log4c_config config;
    log4c_logger logger;

    log4c_config_init(&config);
    if (!log4c_config_load(&config, "examples/log4c.conf")) {
        fprintf(stderr, "failed to load config\n");
        return 1;
    }

    if (!log4c_logger_init_from_config(&logger, &config)) {
        fprintf(stderr, "failed to init logger from config\n");
        return 1;
    }

    LOG4C_INFO(&logger, "app", "logger initialized from config");
    LOG4C_WARN(&logger, "app", "rotation and directory come from config");
    log4c_logger_destroy(&logger);
    return 0;
}
