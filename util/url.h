
/** 实现一个基本的URL解析的工具*/

#ifndef UTIL_URL_H
#define UTIL_URL_H

/* rtsp://user:pass@10.0.0.1:554/demo.mp4/track1?key1=value1&key2=value2#hash */
typedef struct url
{
    const char *schema;               /* rtsp: */
    const char *user;                 /* user */
    const char *pass;                 /* pass */
    const char *host;                 /* 10.0.0.1 */
    unsigned short port;        	  /* 554 */
    const char *path;                 /* /demo.mp4/track1 */
	const char *query;				  /* key1=value1&key2=value2 */
	const char *hash;				  /* hash */
}url_t;

#ifdef __cplusplus
extern "C" {
#endif

/** 给给定的url做解析处理，结果按照url_t的结构进行返回
 * 解析失败时返回NULL，表示不是一个合法的url
 * 对返回的url_t需要使用url_release()进行释放
 */
url_t* url_parse(const char* url, unsigned len);

void url_release(url_t* url);

#ifdef __cplusplus
}
#endif

#endif /* !UTIL_URL_H */
