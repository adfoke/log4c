# log4c

轻量 C17 日志库。

支持：

- 同步日志
- 异步日志
- 多 sink
- 文件输出
- 按大小滚动
- 自动建目录
- `key=value` 配置文件
- `CMake`

## 平台

- 支持 `Linux`
- 支持 `macOS`
- 不支持 `Windows`
- 异步日志依赖 `pthread`

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

```c
log4c_async_logger logger;
log4c_async_stats stats;

if (!log4c_async_logger_init(&logger, stderr, 1024U)) {
    return 1;
}

log4c_async_logger_set_queue_policy(&logger, LOG4C_ASYNC_QUEUE_DROP_NEWEST);
LOG4C_ASYNC_INFO(&logger, "app", "queued");
log4c_async_logger_flush(&logger);
log4c_async_logger_get_stats(&logger, &stats);
log4c_async_logger_destroy(&logger);
```

异步模式补充：

- 调用线程只入队
- 后台线程写 sink
- 满队策略：`BLOCK`、`DROP_NEWEST`、`DROP_OLDEST`
- 支持错误回调和统计

## 目录

- `include/log4c/log4c.h`
- `src/log4c.c`
- `examples/basic.c`
- `examples/from_config.c`
- `examples/async.c`
- `examples/log4c.conf`
- `templates/minimal_app`
- `tests/test_log4c.c`
