# log4c

一个 C17 日志库。

支持：

- 同步日志
- 异步日志
- 多 sink
- 文件输出
- 按大小滚动
- 自动建目录
- `key=value` 配置文件
- `CMake`

## 构建

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## 集成

```cmake
add_subdirectory(path/to/log4c)

add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE log4c::log4c)
```

头文件：

```c
#include <log4c/log4c.h>
```

## 最小示例

```c
#include <log4c/log4c.h>

int main(void) {
    log4c_logger logger;

    log4c_logger_init(&logger, stderr);
    log4c_logger_set_level(&logger, LOG4C_LEVEL_INFO);

    LOG4C_INFO(&logger, "app", "started");

    log4c_logger_destroy(&logger);
    return 0;
}
```

## 文件日志

```c
log4c_logger logger;

log4c_logger_init(&logger, stderr);
log4c_logger_clear_sinks(&logger);
log4c_logger_add_file_sink(&logger, "logs/app.log", LOG4C_OPTION_AUTO_FLUSH);

LOG4C_INFO(&logger, "app", "written to file");
log4c_logger_destroy(&logger);
```

## 滚动日志

```c
log4c_logger logger;

log4c_logger_init(&logger, stderr);
log4c_logger_clear_sinks(&logger);
log4c_logger_add_rotating_file_sink(&logger, "logs/app.log", 1048576U, 3U, LOG4C_OPTION_AUTO_FLUSH);

LOG4C_INFO(&logger, "app", "rotate by size");
log4c_logger_destroy(&logger);
```

规则：

- 当前文件超 `max_bytes` 前先滚动
- 备份文件是 `app.log.1`、`app.log.2`
- 最多保留 `max_files` 个备份

## 配置文件

```ini
log_dir=logs
file_name=app.log
level=info
rotation_max_bytes=1048576
rotation_max_files=3
console=true
file=true
color=false
timestamp=true
location=true
auto_flush=true
console_stream=stderr
```

使用：

```c
log4c_config config;
log4c_logger logger;

log4c_config_init(&config);
if (!log4c_config_load(&config, "log4c.conf")) {
    return 1;
}

if (!log4c_logger_init_from_config(&logger, &config)) {
    return 1;
}

LOG4C_INFO(&logger, "app", "from config");
log4c_logger_destroy(&logger);
```

## 异步日志

需要 `pthread`。

```c
log4c_async_logger logger;

if (!log4c_async_logger_init(&logger, stderr, 1024U)) {
    return 1;
}

log4c_logger_set_level(&logger.logger, LOG4C_LEVEL_DEBUG);
LOG4C_ASYNC_INFO(&logger, "app", "queued");
LOG4C_ASYNC_WARN(&logger, "app", "written by background thread");

log4c_async_logger_flush(&logger);
log4c_async_logger_destroy(&logger);
```

说明：

- 调用线程只负责入队
- 后台线程负责写 sink
- `flush` 等待队列清空
- `destroy` 会 drain 剩余消息

## 目录

- `include/log4c/log4c.h` 公开接口
- `src/log4c.c` 实现
- `examples/basic.c` 同步示例
- `examples/from_config.c` 配置示例
- `examples/async.c` 异步示例
- `examples/log4c.conf` 配置示例
- `templates/minimal_app` 最小接入模板
- `tests/test_log4c.c` 测试
