CFLAGS += -std=c11 -Wall -Wextra
LDLIBS = -lcurses

OBJS += play.o
OBJS += 2048.o

all: tags play

play: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

tags: *.c
	ctags -w *.c
