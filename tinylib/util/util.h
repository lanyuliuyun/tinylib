
/* 基本的工具相关 
 */

#ifndef TINYLIB_UTIL_H
#define TINYLIB_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

/* 获取当前的时间戳，以ms为单位 */
unsigned long long now_ms(void);

/* 获取一个固定单调递增的时戳，以ms为单位。
 * 不受系统时间配置影响
 */
unsigned long long ts_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* !TINYLIB_UTIL_H */
