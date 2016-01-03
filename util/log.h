
#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <stdarg.h>  /* for va_list */

/** 消息级别定义*/
typedef enum log_level{
    LOG_LEVEL_NONE,     /** 无任何打印 */
    LOG_LEVEL_LOG,      /** 一般运行日志 */
    LOG_LEVEL_ERROR,    /** 错误信息 */
    LOG_LEVEL_WARN,     /** 告警性的消息，背后可能存在错误 */
	LOG_LEVEL_INFO,		/** 提示性的信息输出 */
	LOG_LEVEL_DEBUG,	/** 调试信息输出，用于功能流程诊断 */
}log_level_e;

#ifdef __cplusplus
extern "C" {
#endif

/* 注意下面两种日志输出的方式 log_init() 指定输出函数的方式优先级更高
 * 此时级别控制由所指定的输出函数完成， log_file()/log_setlevel() 调用无实际效果！
 *
 * 不做任何设置时，全部输出到 stderr ，级别为 LOG_LEVEL_INFO
 */

/* 设置日志的输出函数，并发环境中的重入问题由所指定的函数自行解决 */
typedef void (*print_f)(log_level_e level, const char *file, int line, const char *fmt, va_list ap);
void log_init(print_f printcb);

/* 设置日志输出文件和输出级别 */
void log_file(const char *file);
void log_setlevel(log_level_e level);

void log_print(log_level_e level, const char *file, int line, const char *fmt, ...);

/* 实际打印可使用下面的宏 */
#ifdef _MSC_VER

#define log_debug(arg, ...) log_print(LOG_LEVEL_DEBUG, __FILE__, __LINE__, arg, __VA_ARGS__)

#define log_info(arg, ...) log_print(LOG_LEVEL_INFO, __FILE__, __LINE__, arg, __VA_ARGS__)

#define log_warn(arg, ...) log_print(LOG_LEVEL_WARN, __FILE__, __LINE__, arg, __VA_ARGS__)

#define log_error(arg, ...) log_print(LOG_LEVEL_ERROR, __FILE__, __LINE__, arg, __VA_ARGS__)

#define log_log(arg, ...) log_print(LOG_LEVEL_LOG, __FILE__, __LINE__, arg, __VA_ARGS__)

#else

#define log_debug(arg...) log_print(LOG_LEVEL_DEBUG, __FILE__, __LINE__, ##arg)

#define log_info(arg...) log_print(LOG_LEVEL_INFO, __FILE__, __LINE__, ##arg)

#define log_warn(arg...) log_print(LOG_LEVEL_WARN, __FILE__, __LINE__, ##arg)

#define log_error(arg...) log_print(LOG_LEVEL_ERROR, __FILE__, __LINE__, ##arg)

#define log_log(arg...) log_print(LOG_LEVEL_LOG, __FILE__, __LINE__, ##arg)

#endif

#ifdef __cplusplus
}
#endif

#endif /* !UTIL_LOG_H */
