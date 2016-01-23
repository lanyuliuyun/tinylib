
/** buffer对象接口 */

#ifndef NET_BUFFER_H
#define NET_BUFFER_H

struct buffer;
typedef struct buffer buffer_t;

#ifdef __cplusplus
extern "C" {
#endif

/** 按指定尺寸创建的buffer */
buffer_t* buffer_new(unsigned size);

/** 释放给定的对象  */
void buffer_destory(buffer_t* buffer);

/** 获取给定buffer中有效数据的起始地址  */
void* buffer_peek(buffer_t* buffer);

/** 获取给定buffer中有效数据的尺寸 */
unsigned buffer_readablebytes(buffer_t* buffer);

/** 向给定的buffer中追加数据  ，结果返回本次写入的数据尺寸*/
unsigned buffer_append(buffer_t* buffer, const void* data, unsigned size);

/** 从给定的fd中读取数据到buffer中 ，结果返回本次读取到的数据尺寸*/
unsigned buffer_readFd(buffer_t* buffer, int fd);

/**  标记在给定的buffer中已经获取到所指尺寸的数据，释放对应的空间 */
void buffer_retrieve(buffer_t *buffer, unsigned size);

void buffer_retrieveall(buffer_t *buffer);

#ifdef __cplusplus
}
#endif

#endif /* !NET_BUFFER_H */

