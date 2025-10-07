#ifndef LOG4C_H
#define LOG4C_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 日志级别定义 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_COUNT
} log_level_t;

/* 输出目标 */
typedef enum {
    LOG_OUTPUT_CONSOLE = 1 << 0,
    LOG_OUTPUT_FILE = 1 << 1
} log_output_t;

/* 日志器配置 */
typedef struct {
    log_level_t level;           /* 当前日志级别 */
    int outputs;                 /* 输出目标 (bitmask) */
    char filename[256];          /* 日志文件名 */
    bool colors_enabled;         /* 是否启用颜色 */
    bool thread_safe;            /* 是否线程安全 */
} log_config_t;

/* 全局函数 */

/* 初始化日志器 */
int log_init(const log_config_t* config);

/* 清理日志器 */
void log_cleanup(void);

/* 设置日志级别 */
void log_set_level(log_level_t level);

/* 设置输出目标 */
void log_set_outputs(int outputs);

/* 设置日志文件 */
int log_set_file(const char* filename);

/* 启用/禁用颜色 */
void log_enable_colors(bool enable);

/* 核心日志函数 */
void log_log(log_level_t level, const char* file, int line, const char* func, const char* format, ...);

/* 便捷宏 */
#define LOG_DEBUG(format, ...) log_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  log_log(LOG_LEVEL_INFO,  __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)  log_log(LOG_LEVEL_WARN,  __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) log_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_FATAL(format, ...) log_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

/* 条件日志宏 (仅在对应级别时编译) */
#if defined(LOG_COMPILE_DEBUG) || !defined(NDEBUG)
#define LOG_DEBUG_COND(format, ...) LOG_DEBUG(format, ##__VA_ARGS__)
#else
#define LOG_DEBUG_COND(format, ...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* LOG4C_H */
