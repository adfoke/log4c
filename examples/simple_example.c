#include "../include/log4c.h"
#include <unistd.h>  /* for sleep */

int main(void) {
    /* 示例1: 默认配置初始化 */
    printf("=== 示例1: 默认配置 ===\n");
    if (log_init(NULL) != 0) {
        fprintf(stderr, "日志初始化失败\n");
        return 1;
    }

    LOG_INFO("应用程序启动");
    LOG_DEBUG("调试信息: value = %d", 42);
    LOG_WARN("警告信息");
    LOG_ERROR("错误信息: %s", "something went wrong");
    LOG_INFO("应用程序运行中...");

    log_cleanup();

    /* 示例2: 自定义配置 */
    printf("\n=== 示例2: 自定义配置 ===\n");
    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,
        .outputs = LOG_OUTPUT_CONSOLE | LOG_OUTPUT_FILE,
        .filename = "app.log",
        .colors_enabled = true,
        .thread_safe = true
    };

    if (log_init(&config) != 0) {
        fprintf(stderr, "日志初始化失败\n");
        return 1;
    }

    LOG_INFO("使用自定义配置启动");
    LOG_DEBUG("调试模式已启用");
    LOG_WARN("这是一个警告");
    LOG_ERROR("这是一个错误");

    /* 动态改变配置 */
    printf("\n=== 示例3: 动态配置 ===\n");
    log_set_level(LOG_LEVEL_ERROR);
    LOG_DEBUG("这个调试信息不会显示");  /* 低于当前级别 */
    LOG_INFO("这个信息也不会显示");    /* 低于当前级别 */
    LOG_ERROR("只有错误级别会显示");

    log_enable_colors(false);
    LOG_ERROR("颜色已禁用");

    log_cleanup();

    printf("\n日志示例完成，请查看 app.log 文件\n");
    return 0;
}
