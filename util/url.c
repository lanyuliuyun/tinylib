
#include "url.h"
#include "log.h"

#include <string.h>
#include <stdlib.h>

/* rtsp://user:pass@10.0.0.1:554/demo.mp4/track1 */

static inline void extrac_query_hash(url_t *u)
{
	char *q_pos;
	char *h_pos;
	
	q_pos = strchr(u->path, '?');
	h_pos = strchr(u->path, '#');

	if (NULL != q_pos)
	{
		*q_pos = '\0';
		q_pos++;
		u->query = q_pos;
	}

	if (NULL != h_pos)
	{
		*h_pos = '\0';
		h_pos++;
		u->hash = h_pos;
	}

	return;
}

url_t* url_parse(const char* url, unsigned len)
{
    char *u_raw;
    url_t *u;
    char *part;
    char *pos;
    char *pos1;

    if (NULL == url || 4 > len)
    {
        return NULL;
    }

	/* 此处额外加3除了为\0包括空间之外，
	 * 在下面的两种情况中，需要额外空间
	 * 1, 存在path时，需要将path部分往后移动一个字符位置, 额外需要一个byte
	 * 2, 不存在path时，需要手动指定path为"/", 额外需要2个byte
	 * 
	 * 故此处为+3
	 */	
    u = (url_t*)malloc(sizeof(url_t)+len+3);   
	
    memset(u, 0, sizeof(url_t)+len+3);
    u_raw = (char*)&u[1];
    strcpy(u_raw, url);

    u->schema = NULL;
    u->user = NULL;
    u->pass = NULL;
    u->host = NULL;
    u->port = 0;
    u->path = NULL;
	u->query = NULL;

    part = u_raw;
    pos = strstr(part, "://");
    if (NULL == pos)
    {
        log_error("url_parse: bad url(%s)", url);
        free(u);
        return NULL;
    }

    pos++;
    *pos = '\0';	/* 截取schema部分完成 */
    u->schema = part;

    pos += 2;		/* 指针移动到 user:pwd@host:port/path?query#hash 部分 */
    part = pos;
	
	pos = strchr(part, '/');
	if (NULL == pos)
	{
		/* 没有path部分，手动将path确定为"/" */
		pos = part + strlen(part) + 1;
		*pos = '/';
		u->path = pos;
	}
	else
	{
		/* 存在path，将/开始部分向后移动一个字符，额外的空间分配时候已经提供了 */
		memmove(pos+1, pos, strlen(pos));
		*pos = '\0';	/* path之前的部分截取完毕 */

		pos++;
		u->path = pos;
		extrac_query_hash(u);
	}

	/* part -> user:pwd@host:port */

    pos = strrchr(part, '@');
    if (NULL != pos)
    {
		/* 存在 user@pwd 部分 */
        *pos = '\0';	/* 截取 user:pwd 部分完成 */
        u->user = part;
        
        pos1 = strchr(part, ':');
        if (NULL != pos1)
        {
            *pos1 = '\0';	/* 截取user部分完成 */
            pos1++;
            if (pos1 == pos)
            {
				/* user:@host 情景认为没有密码 */
                pos1 = NULL;
            }
        }
        u->pass = pos1;

        pos++;
        part = pos;
    }

    u->host = part;

    pos = strchr(u->host, ':');
    if (NULL != pos)
    {
        /* url中有端口 */
        *pos = '\0';	/* 截取host部分完成 */

		/* atoi("123/path")结果为123，atoi("/path")结果为0，所以可以直接获取端口 */
        pos++;
		part = pos;
		u->port = atoi(part);
	}

    return u;
}

void url_release(url_t* url)
{
    free(url);

    return;
}

