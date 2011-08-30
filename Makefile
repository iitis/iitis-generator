CFLAGS = -Ilib/
LDFLAGS = -export-dynamic -lpjf -lpcre -levent lib/radiotap.o

ME=iitis-generator
C_OBJECTS=interface.o generator.o cmd-packet.o schedule.o sync.o stats.o dump.o parser.o fun.o
TARGETS=iitis-generator

include rules.mk

iitis-generator: $(C_OBJECTS)
	$(MAKE) -C lib
	$(CC) $(C_OBJECTS) $(LDFLAGS) -o iitis-generator

clean: clean-std
	$(MAKE) -C lib clean

install: install-std
