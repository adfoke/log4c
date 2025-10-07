# log4c

一个轻量级、易用的C语言日志库，支持多级别日志、彩色输出、文件输出和线程安全。

## 特性

- **多日志级别**：DEBUG、INFO、WARN、ERROR、FATAL
- **彩色输出**：不同级别使用不同颜色显示（可选）
- **多输出目标**：控制台、文件，或同时输出
- **线程安全**：可选的线程安全支持
- **易于集成**：单头文件包含，简单API
- **条件编译**：可选择性编译DEBUG级别日志
- **跨平台**：支持Linux、macOS、Windows

## 快速开始

### 1. 包含头文件

```c
#include "log4c.h"
```

### 2. 初始化日志器

```c
// 使用默认配置
log_init(NULL);

// 或使用自定义配置
log_config_t config = {
    .level = LOG_LEVEL_DEBUG,
    .outputs = LOG_OUTPUT_CONSOLE | LOG_OUTPUT_FILE,
    .filename = "app.log",
    .colors_enabled = true,
    .thread_safe = true
};
log_init(&config);
```

### 3. 使用日志

```c
LOG_DEBUG("调试信息: %d", value);
LOG_INFO("应用程序启动");
LOG_WARN("警告信息");
LOG_ERROR("错误: %s", error_msg);
LOG_FATAL("致命错误");
```

### 4. 清理资源

```c
log_cleanup();
```

## 构建

### 使用CMake

```bash
mkdir build
cd build
cmake ..
make
```

### 编译选项

- `LOG_COMPILE_DEBUG=ON`：编译时包含DEBUG级别日志（默认关闭，可减少生产环境开销）

## API参考

### 初始化和清理

- `int log_init(const log_config_t* config)`：初始化日志器
- `void log_cleanup(void)`：清理日志器资源

### 配置设置

- `void log_set_level(log_level_t level)`：设置日志级别
- `void log_set_outputs(int outputs)`：设置输出目标
- `int log_set_file(const char* filename)`：设置日志文件
- `void log_enable_colors(bool enable)`：启用/禁用颜色

### 日志输出

- `LOG_DEBUG(format, ...)`：调试级别日志
- `LOG_INFO(format, ...)`：信息级别日志
- `LOG_WARN(format, ...)`：警告级别日志
- `LOG_ERROR(format, ...)`：错误级别日志
- `LOG_FATAL(format, ...)`：致命级别日志

### 条件编译宏

- `LOG_DEBUG_COND(format, ...)`：仅在定义了`LOG_COMPILE_DEBUG`或未定义`NDEBUG`时编译

## 配置选项

```c
typedef struct {
    log_level_t level;           // 日志级别 (LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, etc.)
    int outputs;                 // 输出目标 (LOG_OUTPUT_CONSOLE | LOG_OUTPUT_FILE)
    char filename[256];          // 日志文件名
    bool colors_enabled;         // 是否启用颜色
    bool thread_safe;            // 是否线程安全
} log_config_t;
```

## 示例输出

```
[2025-01-07 10:43:14] [INFO] [main.c:25 main] 应用程序启动
[2025-01-07 10:43:14] [WARN] [main.c:30 main] 警告信息
[2025-01-07 10:43:14] [ERROR] [main.c:35 main] 错误: connection failed
```

## 集成到项目

1. 将`include/log4c.h`和`src/log4c.c`复制到你的项目中
2. 在构建时链接`pthread`库
3. 包含头文件并使用API

## 许可证

MIT License
