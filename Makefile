
# makfile for a tiny net/rtsp library written by huangsanyi

TOOLCHAIN_PREFIX ?= arm-sunxiA20-linux-gnueabi-

CC = $(TOOLCHAIN_PREFIX)gcc
LD = $(TOOLCHAIN_PREFIX)gcc
AR = $(TOOLCHAIN_PREFIX)ar -r

CPP_FLAGS = -I.
C_FLAGS = -Wall -Werror -fno-omit-frame-pointer -O0 -ggdb
LD_FLAGS = -L./output -ltinylib -lpthread

TINYLIB = output/libtinylib.a

UTIL_OBJS = \
	tinylib/util/log.o \
	tinylib/util/md5.o \
	tinylib/util/time_wheel.o \
	tinylib/util/url.o \
	tinylib/util/util.o

NET_OBJS = \
	tinylib/linux/net/async_task_queue.o \
	tinylib/linux/net/buffer.o \
	tinylib/linux/net/channel.o \
	tinylib/linux/net/inetaddr.o \
	tinylib/linux/net/loop.o \
	tinylib/linux/net/socket.o \
	tinylib/linux/net/tcp_client.o \
	tinylib/linux/net/tcp_connection.o \
	tinylib/linux/net/tcp_server.o \
	tinylib/linux/net/timer_queue.o \
	tinylib/linux/net/udp_peer.o

# unit test

# async_task
TEST_ASYNC_TASK_BIN = output/test_async_task
TEST_ASYNC_TASK_OBJS = test/test_async_task.o

# async_task in multi-thread
TEST_MT_ASYNC_TASK_BIN = output/test_mt_async_task
TEST_MT_ASYNC_TASK_OBJS = test/test_mt_async_task.o

## atomic
TEST_ATOMIC_BIN = output/test_atomic
TEST_ATOMIC_OBJS = test/test_atomic.o

## log
TEST_LOG_BIN = output/test_log
TEST_LOG_OBJS = test/test_log.o

## loop_timer
TEST_LOOP_TIMER_BIN = output/test_loop_timer
TEST_LOOP_TIMER_OBJS = test/test_loop_timer.o

# timer in multi-thread
TEST_MT_TIMER_BIN = output/test_mt_timer
TEST_MT_TIMER_OBJS = test/test_mt_timer.o

## md5
TEST_MD5_BIN = output/test_md5
TEST_MD5_OBJS = test/test_md5.o

## tcp
TEST_TCP_SERVER_BIN = output/test_tcp_server
TEST_TCP_SERVER_OBJS = test/test_tcp_server.o
TEST_TCP_CLIENT_BIN = output/test_tcp_client
TEST_TCP_CLIENT_OBJS = test/test_tcp_client.o

## time_wheel
TEST_TIME_WHEEL_BIN = output/test_time_wheel
TEST_TIME_WHEEL_OBJS = test/test_time_wheel.o

## url
TEST_URL_BIN = output/test_url
TEST_URL_OBJS = test/test_url.o

## test_pingpong
TEST_PINGPONG_BIN = output/test_pingpong
TEST_PINGPONG_OBJS = test/test_pingpong.o

.PHONY: all clean test

all: $(TINYLIB)

$(TINYLIB): $(NET_OBJS) $(UTIL_OBJS)
	$(AR) $(TINYLIB) $^

test: $(TINYLIB) $(TEST_ASYNC_TASK_OBJS) \
	      $(TEST_MT_ASYNC_TASK_OBJS) \
	      $(TEST_ATOMIC_OBJS) \
	      $(TEST_LOG_OBJS) \
	      $(TEST_LOOP_TIMER_OBJS) \
	      $(TEST_MT_TIMER_OBJS) \
	      $(TEST_MD5_OBJS) \
	      $(TEST_TCP_CLIENT_OBJS) \
	      $(TEST_TCP_SERVER_OBJS) \
	      $(TEST_TIME_WHEEL_OBJS) \
	      $(TEST_URL_OBJS) \
		  $(TEST_PINGPONG_OBJS)
	$(LD) $(TEST_ASYNC_TASK_OBJS) -o $(TEST_ASYNC_TASK_BIN) $(LD_FLAGS)
	$(LD) $(TEST_MT_ASYNC_TASK_OBJS) -o $(TEST_MT_ASYNC_TASK_BIN) $(LD_FLAGS)
	$(LD) $(TEST_ATOMIC_OBJS) -o $(TEST_ATOMIC_BIN) $(LD_FLAGS)
	$(LD) $(TEST_LOG_OBJS) -o $(TEST_LOG_BIN) $(LD_FLAGS)
	$(LD) $(TEST_LOOP_TIMER_OBJS) -o $(TEST_LOOP_TIMER_BIN) $(LD_FLAGS)
	$(LD) $(TEST_MT_TIMER_OBJS) -o $(TEST_MT_TIMER_BIN) $(LD_FLAGS)
	$(LD) $(TEST_MD5_OBJS) -o $(TEST_MD5_BIN) $(LD_FLAGS)
	$(LD) $(TEST_TCP_CLIENT_OBJS) -o $(TEST_TCP_CLIENT_BIN) $(LD_FLAGS)
	$(LD) $(TEST_TCP_SERVER_OBJS) -o $(TEST_TCP_SERVER_BIN) $(LD_FLAGS)
	$(LD) $(TEST_TIME_WHEEL_OBJS) -o $(TEST_TIME_WHEEL_BIN) $(LD_FLAGS)
	$(LD) $(TEST_URL_OBJS) -o $(TEST_URL_BIN) $(LD_FLAGS)
	$(LD) $(TEST_PINGPONG_OBJS) -o $(TEST_PINGPONG_BIN) $(LD_FLAGS)

%.o: %.c
	$(CC) -c $^ -o $@ $(CPP_FLAGS) $(C_FLAGS)

clean:
	rm -f $(TINYLIB)
	rm -f $(NET_OBJS) $(RTP_OBJS) $(RTSP_OBJS) $(UTIL_OBJS)
	rm -f $(TEST_ASYNC_TASK_OBJS) $(TEST_ASYNC_TASK_BIN)
	rm -f $(TEST_MT_ASYNC_TASK_OBJS) $(TEST_MT_ASYNC_TASK_BIN)
	rm -f $(TEST_ATOMIC_OBJS) $(TEST_ATOMIC_BIN)
	rm -f $(TEST_LOG_OBJS) $(TEST_LOG_BIN)
	rm -f $(TEST_LOOP_TIMER_OBJS) $(TEST_LOOP_TIMER_BIN)
	rm -f $(TEST_MT_TIMER_OBJS) $(TEST_MT_TIMER_BIN)
	rm -f $(TEST_MD5_OBJS) $(TEST_MD5_BIN)
	rm -f $(TEST_TCP_CLIENT_OBJS) $(TEST_TCP_CLIENT_BIN)
	rm -f $(TEST_TCP_SERVER_OBJS) $(TEST_TCP_SERVER_BIN)
	rm -f $(TEST_TIME_WHEEL_OBJS) $(TEST_TIME_WHEEL_BIN)
	rm -f $(TEST_URL_OBJS) $(TEST_URL_BIN)
	rm -f $(TEST_PINGPONG_OBJS) $(TEST_PINGPONG_BIN)
