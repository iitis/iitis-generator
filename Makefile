CFLAGS = -Ilib/
LDFLAGS = -lpjf -lpcre -levent lib/radiotap.o

ME=generator
C_OBJECTS=interface.o generator.o cmd-packet.o schedule.o sync.o
TARGETS=generator

include rules.mk

generator: $(C_OBJECTS)
	make -C lib
	$(CC) $(C_OBJECTS) $(LDFLAGS) -o generator

install: install-std
