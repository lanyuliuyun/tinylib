
/* 基本的工具相关 
 */

#ifndef TINYLIB_UTIL_H
#define TINYLIB_UTIL_H

#include <string.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 获取当前的时间戳，以ms为单位 */
void get_current_timestamp(unsigned long long *timestamp);

#ifdef __cplusplus
}
#endif

#endif /* !TINYLIB_UTIL_H */
