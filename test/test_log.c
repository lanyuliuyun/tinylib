
#include "util/log.h"

#include <stdio.h>
#include <assert.h>

int main(int argc, char const *argv[])
{
	log_warn("log_warn: arg: %s, number: %d, ratio: %f", "info", 4, 0.53);
	log_error("log_error: arg: %s", "error");
	log_log("log_log: arg: %s", "log");
	log_warn("this text will not be displayed");
	
	return 0;
}
