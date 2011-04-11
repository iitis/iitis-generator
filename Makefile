CFLAGS =
LDFLAGS = -lpjf -lpcre -levent

ME=generator
C_OBJECTS=interface.o generator.o
TARGETS=generator

include rules.mk

generator: $(C_OBJECTS)
	$(CC) $(C_OBJECTS) $(LDFLAGS) -o generator

install: install-std
