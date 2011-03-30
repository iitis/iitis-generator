CFLAGS =
LDFLAGS = -lpjf -lpcre

ME=generator
C_OBJECTS=inject.o generator.o
TARGETS=generator

include rules.mk

generator: $(C_OBJECTS)
	$(CC) $(C_OBJECTS) $(LDFLAGS) -o generator

nfs:
	cp -a generator /home/pjf/iitis/rbtools/data/nfs/all/usr/sbin/
