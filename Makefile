CFLAGS = -Ilib/
LDFLAGS = -lpjf -lpcre -levent lib/radiotap.o

ME=generator
C_OBJECTS=interface.o generator.o cmd-packet.o schedule.o sync.o stats.o
TARGETS=generator

include rules.mk

generator: $(C_OBJECTS)
	$(MAKE) -C lib
	$(CC) $(C_OBJECTS) $(LDFLAGS) -o generator

clean: clean-std
	$(MAKE) -C lib clean

install: install-std
