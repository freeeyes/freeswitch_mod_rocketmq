FREESWITCH_MOD_PATH=/usr/local/freeswitch-1.10.2/lib/freeswitch/mod
FREESWITCH_INCLUDE=/oncon/jiekou/freeswitch-1.10.2/include/freeswitch
ROCKET_MQ_INCLUDE=/oncon/tools/rocketmq-client-cpp/include
ROCKET_MQ_LIB_PATH=/oncon/tools/rocketmq-client-cpp/bin/

MODNAME = mod_rocketmq.so
MODOBJ = mod_rocketmq.o
MODCFLAGS = -Wall -Werror -I$(FREESWITCH_INCLUDE) -I$(ROCKET_MQ_INCLUDE)
MODLDFLAGS = -L$(ROCKET_MQ_LIB_PATH) -lfreeswitch -lrocketmq
CC = g++
CFLAGS = -fPIC -g -ggdb  $(MODCFLAGS)
CPPFLAGS = -fPIC -std=c++11 -g -ggdb  $(MODCFLAGS)
LDFLAGS = $(MODLDFLAGS)

.PHONY: all
all: $(MODNAME)

$(MODNAME): $(MODOBJ)
	@$(CC) -shared $(CPPFLAGS) -o $@ $(MODOBJ) $(LDFLAGS)

.c.o: $<
	@$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	rm -f $(MODNAME) $(MODOBJ)

install: $(MODNAME)
	install -d $(FREESWITCH_MOD_PATH)
	install $(MODNAME) $(FREESWITCH_MOD_PATH)