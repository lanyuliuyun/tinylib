
/* time wheel是用于实现在执行多少步(time_wheel_step)之后执行用户指定的函数
 * 可用于实现定时器，但其特点是可以在常数时间内监测多个单元的超时事件
 *  现统称为timer
 */

#ifndef UTIL_TIME_WHEEL_H
#define UTIL_TIME_WHEEL_H

struct time_wheel;
typedef struct time_wheel time_wheel_t;

typedef enum{
    TIME_WHEEL_EXPIRE_ONESHOT,
    TIME_WHEEL_EXPIRE_LOOP,
}time_wheel_expire_type_e;

#ifdef __cplusplus
extern "C" {
#endif 

/* 如果返回值是oneshot，则该timer是一次性的，超时之后不再活动
 * 反之返回值是其他值时时默认为loop，该timer是循环timer，直至其返回oneshot或被cancel为止
 */
typedef int(*onexpire_f)(void *userdata);

time_wheel_t* time_wheel_create(unsigned max_step);

void time_wheel_destroy(time_wheel_t* wheel);

/* 返回值为timer的handle，在time_wheel_refresh时使用 */
void* time_wheel_submit(time_wheel_t* wheel, onexpire_f func, void* userdata, unsigned steps);

void time_wheel_cancel(time_wheel_t* wheel, void *handle);

/* 重置handle指定的timer，使其可再执行一次 */
void time_wheel_refresh(time_wheel_t* wheel, void *handle);

void time_wheel_step(time_wheel_t* wheel);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_TIME_WHEEL_H */

