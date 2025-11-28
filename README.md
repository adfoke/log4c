# log4c

A lightweight, easy-to-use C logging library supporting multi-level logging, color output, file output, and thread safety.

## Features

- **Multi-level logging**: DEBUG, INFO, WARN, ERROR, FATAL
- **Color output**: Different levels displayed in different colors (optional)
- **Multiple output targets**: Console, file, or both
- **Thread safety**: Optional thread safety support
- **Easy integration**: Single header file inclusion, simple API
- **Conditional compilation**: Selectively compile DEBUG level logs
- **Cross-platform**: Supports Linux, macOS, Windows

## Quick Start

### 1. Include header file

```c
#include "log4c.h"
```

### 2. Initialize logger

```c
// Use default configuration
log_init(NULL);

// Or use custom configuration
log_config_t config = {
    .level = LOG_LEVEL_DEBUG,
    .outputs = LOG_OUTPUT_CONSOLE | LOG_OUTPUT_FILE,
    .filename = "app.log",
    .colors_enabled = true,
    .thread_safe = true
};
log_init(&config);
```

### 3. Usage

```c
LOG_DEBUG("Debug info: %d", value);
LOG_INFO("Application started");
LOG_WARN("Warning message");
LOG_ERROR("Error: %s", error_msg);
LOG_FATAL("Fatal error");
```

### 4. Cleanup resources

```c
log_cleanup();
```

## Build

### Using CMake

```bash
mkdir build
cd build
cmake ..
make
```

### Build Options

- `LOG_COMPILE_DEBUG=ON`: Include DEBUG level logs at compile time (default OFF, reduces overhead in production)

## API Reference

### Initialization and Cleanup

- `int log_init(const log_config_t* config)`: Initialize logger
- `void log_cleanup(void)`: Cleanup logger resources

### Configuration Settings

- `void log_set_level(log_level_t level)`: Set log level
- `void log_set_outputs(int outputs)`: Set output targets
- `int log_set_file(const char* filename)`: Set log file
- `void log_enable_colors(bool enable)`: Enable/disable colors

### Log Output

- `LOG_DEBUG(format, ...)`: Debug level log
- `LOG_INFO(format, ...)`: Info level log
- `LOG_WARN(format, ...)`: Warning level log
- `LOG_ERROR(format, ...)`: Error level log
- `LOG_FATAL(format, ...)`: Fatal level log

### Conditional Compilation Macros

- `LOG_DEBUG_COND(format, ...)`: Compiled only when `LOG_COMPILE_DEBUG` is defined or `NDEBUG` is not defined

## Configuration Options

```c
typedef struct {
    log_level_t level;           // Log level (LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, etc.)
    int outputs;                 // Output target (LOG_OUTPUT_CONSOLE | LOG_OUTPUT_FILE)
    char filename[256];          // Log filename
    bool colors_enabled;         // Whether to enable colors
    bool thread_safe;            // Whether thread-safe
} log_config_t;
```

## Example Output

```
[2025-01-07 10:43:14] [INFO] [main.c:25 main] Application started
[2025-01-07 10:43:14] [WARN] [main.c:30 main] Warning message
[2025-01-07 10:43:14] [ERROR] [main.c:35 main] Error: connection failed
```

## Integration

1. Copy `include/log4c.h` and `src/log4c.c` to your project
2. Link `pthread` library during build
3. Include header and use API

## License

MIT License
