BASE=../../../..


LOCAL_CFLAGS=-I/usr/include -std=c++11 -fpic -Werror=declaration-after-statement
LOCAL_LDFLAGS=-L/usr/lib64 -lpthread 

LOCAL_OBJS=

include $(BASE)/build/modmake.rules