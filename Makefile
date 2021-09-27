CHROOT_USER = play
CHROOT_GROUP = ${CHROOT_USER}

CFLAGS += -std=c11 -Wall -Wextra
LDFLAGS = -static
LDLIBS = -lncursesw

-include config.mk

OBJS += 2048.o
OBJS += freecell.o
OBJS += play.o
OBJS += snake.o

all: tags play

play: ${OBJS}
	${CC} ${LDFLAGS} ${OBJS} ${LDLIBS} -o $@

tags: *.c
	ctags -w *.c

chroot.tar: play
	install -d -o root -g wheel \
		root \
		root/bin \
		root/home \
		root/usr/share/locale \
		root/usr/share/misc
	install -d -o ${CHROOT_USER} -g ${CHROOT_GROUP} root/home/${CHROOT_USER}
	if test -e /usr/share/locale/UTF-8; then \
		cp -af /usr/share/locale/UTF-8 root/usr/share/locale; fi
	if test -e /usr/share/locale/en_US.UTF-8; then \
		cp -LRfp /usr/share/locale/en_US.UTF-8 root/usr/share/locale; fi
	if test -e /usr/share/terminfo; then \
		cp -af /usr/share/terminfo root/usr/share; fi
	if test -e /usr/share/misc/termcap.db; then \
		cp -fp /usr/share/misc/termcap.db root/usr/share/misc; fi
	if test -e /rescue/sh; then \
		cp -fp /rescue/sh root/bin; else cp -fp /bin/sh root/bin; fi
	install play root/bin
	tar -c -f chroot.tar -C root bin home usr

clean:
	rm -fr play ${OBJS} tags chroot.tar root

install: chroot.tar
	tar -px -f chroot.tar -C /home/${CHROOT_USER}
