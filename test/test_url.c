
#include "util/url.h"

#include <stdio.h>
#include <string.h>

int main()
{
    const char *url;
    url_t *u;

    url = "rtsp://10.10.8.33";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path
    );
    url_release(u);
    printf("=====================\n");

    url = "rtsp://10.10.8.33:554";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path        
    );
    url_release(u);
    printf("=====================\n");    

    url = "rtsp://10.10.8.33/path";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path        
    );
    url_release(u);
    printf("=====================\n");    

    url = "rtsp://10.10.8.33:554/path";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path        
    );    
    url_release(u);
    printf("=====================\n");

    url = "rtsp://admin@10.10.8.33:554/path";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path        
    );    
    url_release(u);
    printf("=====================\n");
	
    url = "rtsp://:@10.10.8.33:554/path";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path        
    );    
    url_release(u);
    printf("=====================\n");

    url = "rtsp://admin:@10.10.8.33:554";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path        
    );    
    url_release(u);
    printf("=====================\n");

    url = "rtsp://admin:12345@10.10.8.33:554";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path        
    );    
    url_release(u);
    printf("=====================\n");

    url = "rtsp://admin:12345@10.10.8.33:554/path";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path        
    );    
    url_release(u);
    printf("=====================\n");
	
    url = "rtsp://admin:12345@10.10.8.33:554/path?query=value#hash";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n"
		"query => %s\n"
		"hash => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path,
		u->query,
		u->hash
    );    
    url_release(u);
    printf("=====================\n");	
	
	
    url = "rtsp://127.0.0.1/tel:99100";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n"
		"query => %s\n"
		"hash => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path,
		u->query,
		u->hash
    );    
    url_release(u);
    printf("=====================\n");	
	
    url = "rtsp://127.0.0.1:554/tel:99100";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n"
		"query => %s\n"
		"hash => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path,
		u->query,
		u->hash
    );    
    url_release(u);
    printf("=====================\n");	
	
    url = "rtsp://user:pwd@127.0.0.1:554/hikplat://user:pwd@10.10.8.11/tel:99100";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n"
		"query => %s\n"
		"hash => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path,
		u->query,
		u->hash
    );
    url_release(u);
    printf("=====================\n");
	
	url = "hikplat://admin:1qaz2wsx!@@10.99.11.165:80/1001140901043135019";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n"
		"query => %s\n"
		"hash => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path,
		u->query,
		u->hash
    );    
    url_release(u);
    printf("=====================\n");
	
	url = "rtsp://10.20.2.157/rtsp://10.10.8.53";
    u = url_parse(url, strlen(url));
    printf(
        "raw: %s\n"
        "schema => %s\n"
        "user => %s\n"
        "pass => %s\n"
        "host => %s\n"
        "port => %u\n"
        "path => %s\n"
		"query => %s\n"
		"hash => %s\n",
        url,
        u->schema,
        u->user,
        u->pass,
        u->host,
        u->port,
        u->path,
		u->query,
		u->hash
    );    
    url_release(u);
    printf("=====================\n");	
	
    return 0;
}
