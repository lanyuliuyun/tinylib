
#include "util/md5.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
	char msg[] = "admin:8ce748cedf4c:12345";
	unsigned char digest[MD5_DIGEST_LENGTH] = {0};
	char digest_text[] = "46486c93b7b6d3826f738968061c7e27";
	
	MD5((unsigned char*)msg, strlen(msg), digest);
	
/*	openssl/md5的计算结果如下
	(gdb) p ha1buf
	$1 = "admin:8ce748cedf4c:12345", '\000' <repeats 487 times>
	(gdb) n
	67 ha2len = 0;
	(gdb) x /4x ha1digest
	0x22faf0:       0x3db21add      0x0fe706b7      0x22b02cd3      0x1757c2bd
	
	实际计算结果	
	(gdb) p msg
	$1 = "admin:8ce748cedf4c:12345"
	(gdb) x /4x digest
	0x22ff27:       0x3db21add      0x0fe706b7      0x22b02cd3      0x1757c2bd
	(gdb)
 */	
	printf("Desired digest_text: %s\n", digest_text);

	return 0;
}
