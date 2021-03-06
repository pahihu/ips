CFLAGS	= -m32 -Wall -O3
# Add -DBIGENDIAN to the above if your machine is big-endian.
# Doing so is also correct on little-endian machines, but results
# in slightly less efficient code.

LDFLAGS	= -lm -lncurses
SRCS = ips.c emu.c
OBJS = ips.o emu.o

ips: $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

ips.o:	ips.c ips.h
emu.o:	emu.c ips.h

