#include "../include/log4c.h"
#include <unistd.h>  /* for sleep */

int main(void) {
    /* Example 1: Default configuration initialization */
    printf("=== Example 1: Default Configuration ===\n");
    if (log_init(NULL) != 0) {
        fprintf(stderr, "Log initialization failed\n");
        return 1;
    }

    LOG_INFO("Application started");
    LOG_DEBUG("Debug info: value = %d", 42);
    LOG_WARN("Warning message");
    LOG_ERROR("Error message: %s", "something went wrong");
    LOG_INFO("Application running...");

    log_cleanup();

    /* Example 2: Custom configuration */
    printf("\n=== Example 2: Custom Configuration ===\n");
    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,
        .outputs = LOG_OUTPUT_CONSOLE | LOG_OUTPUT_FILE,
        .filename = "app.log",
        .colors_enabled = true,
        .thread_safe = true
    };

    if (log_init(&config) != 0) {
        fprintf(stderr, "Log initialization failed\n");
        return 1;
    }

    LOG_INFO("Started with custom configuration");
    LOG_DEBUG("Debug mode enabled");
    LOG_WARN("This is a warning");
    LOG_ERROR("This is an error");

    /* Dynamic configuration changes */
    printf("\n=== Example 3: Dynamic Configuration ===\n");
    log_set_level(LOG_LEVEL_ERROR);
    LOG_DEBUG("This debug message will not be shown");  /* Below current level */
    LOG_INFO("This info message will not be shown");    /* Below current level */
    LOG_ERROR("Only error level will be shown");

    log_enable_colors(false);
    LOG_ERROR("Colors disabled");

    log_cleanup();

    printf("\nLog example completed, please check app.log file\n");
    return 0;
}
