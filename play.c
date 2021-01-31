/* Copyright (C) 2018  C. McEnroe <june@causal.agency>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <curses.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#ifdef __FreeBSD__
#include <sys/capsicum.h>
#endif

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

typedef unsigned uint;

enum { ScoresLen = 1000 };
static struct Score {
	time_t date;
	uint score;
	char name[32];
} scores[ScoresLen];

static FILE *scoresOpen(const char *path) {
	int fd = open(path, O_RDWR | O_CREAT, 0644);
	if (fd < 0) err(EX_CANTCREAT, "%s", path);
	FILE *file = fdopen(fd, "r+");
	if (!file) err(EX_CANTCREAT, "%s", path);
	return file;
}

static void scoresLock(FILE *file) {
	int error = flock(fileno(file), LOCK_EX);
	if (error) err(EX_IOERR, "flock");
}

static void scoresRead(FILE *file) {
	memset(scores, 0, sizeof(scores));
	rewind(file);
	fread(scores, sizeof(struct Score), ScoresLen, file);
	if (ferror(file)) err(EX_IOERR, "fread");
}

static void scoresWrite(FILE *file) {
	rewind(file);
	fwrite(scores, sizeof(struct Score), ScoresLen, file);
	if (ferror(file)) err(EX_IOERR, "fwrite");
}

static size_t scoresInsert(struct Score new) {
	if (!new.score) return ScoresLen;
	for (size_t i = 0; i < ScoresLen; ++i) {
		if (scores[i].score > new.score) continue;
		memmove(
			&scores[i + 1], &scores[i],
			sizeof(struct Score) * (ScoresLen - i - 1)
		);
		scores[i] = new;
		return i;
	}
	return ScoresLen;
}

static void curse(void) {
	initscr();
	cbreak();
	echo();
	curs_set(1);
	keypad(stdscr, true);
	leaveok(stdscr, false);
	start_color();
	use_default_colors();
	attr_set(A_NORMAL, 0, NULL);
	erase();
}

enum {
	RankWidth = 4,
	ScoreWidth = 10,
	NameWidth = 31,
	DateWidth = 10,
	BoardWidth = RankWidth + 2 + ScoreWidth + 2 + NameWidth + 2 + DateWidth,
	BoardY = 0,
	BoardX = 2,
	NameX = BoardX + RankWidth + 2 + ScoreWidth + 2,
	BoardLen = 15,
};

static char board[BoardWidth + 1];

static char *boardTitle(const char *title) {
	snprintf(
		board, sizeof(board),
		"%*s",
		(int)(BoardWidth + strlen(title)) / 2, title
	);
	return board;
}

static char *boardLine(void) {
	for (uint i = 0; i < BoardWidth; ++i) {
		board[i] = '=';
	}
	board[BoardWidth] = '\0';
	return board;
}

static char *boardScore(size_t i) {
	struct tm *time = localtime(&scores[i].date);
	if (!time) err(EX_SOFTWARE, "localtime");
	char date[DateWidth + 1];
	strftime(date, sizeof(date), "%F", time);
	snprintf(
		board, sizeof(board),
		"%*zu. %*u  %-*s  %*s",
		RankWidth, 1 + i,
		ScoreWidth, scores[i].score,
		NameWidth, scores[i].name,
		DateWidth, date
	);
	return board;
}

static void draw(const char *title, size_t new) {
	mvaddstr(BoardY + 0, BoardX, boardTitle(title));
	mvaddstr(BoardY + 1, BoardX, boardLine());

	int newY = -1;
	for (size_t i = 0; i < BoardLen; ++i) {
		if (!scores[i].score) break;
		if (i == new) newY = BoardY + 2 + i;
		attr_set(i == new ? A_BOLD : A_NORMAL, 0, NULL);
		mvaddstr(BoardY + 2 + i, BoardX, boardScore(i));
	}
	if (new == ScoresLen) return;

	if (new >= BoardLen) {
		newY = BoardY + BoardLen + 5;
		mvaddstr(newY - 3, BoardX, boardLine());
		mvaddstr(newY - 2, BoardX, boardScore(new - 2));
		mvaddstr(newY - 1, BoardX, boardScore(new - 1));
		attr_set(A_BOLD, 0, NULL);
		mvaddstr(newY, BoardX, boardScore(new));
		attr_set(A_NORMAL, 0, NULL);
		if (new + 1 < ScoresLen && scores[new + 1].score) {
			mvaddstr(newY + 1, BoardX, boardScore(new + 1));
		}
		if (new + 2 < ScoresLen && scores[new + 2].score) {
			mvaddstr(newY + 2, BoardX, boardScore(new + 2));
		}
	}
	move(newY, NameX);
}

typedef uint Play(void);
uint play2048(void);
uint playSnake(void);

static const struct Game {
	const char *name;
	const char *title;
	const char *desc;
	Play *play;
} Games[] = {
	{ "2048", "2048", "Slide and merge matching tiles", play2048 },
	{ "snake", "Snake", "Eat food before it spoils to become long", playSnake },
};

static const struct Game *menu(void) {
	const char *cmd = getenv("SSH_ORIGINAL_COMMAND");
	for (uint i = 0; cmd && i < ARRAY_LEN(Games); ++i) {
		if (!strcmp(Games[i].name, cmd)) return &Games[i];
	}
	uint game = 0;
	for (;;) {
		for (uint i = 0; i < ARRAY_LEN(Games); ++i) {
			attrset(i == game ? A_STANDOUT : A_NORMAL);
			char buf[256];
			snprintf(buf, sizeof(buf), "%u. %s", 1 + i, Games[i].title);
			mvaddstr(1 + 3 * i, 2, buf);
			attrset(A_NORMAL);
			mvaddstr(2 + 3 * i, 2, Games[i].desc);
		}
		move(1 + 3 * game, 2);
		int ch = getch();
		switch (ch) {
			break; case 'k': case KEY_UP: {
				if (game) game--;
			}
			break; case 'j': case KEY_DOWN: {
				if (game + 1 < ARRAY_LEN(Games)) game++;
			}
			break; case '1' ... '9': {
				if (ch - '1' < (int)ARRAY_LEN(Games)) game = ch - '1';
			}
			break; case '\r': case '\n': case KEY_ENTER: {
				return &Games[game];
			}
			break; case 'q': {
				return NULL;
			}
			break; case ERR: exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char *argv[]) {
	const char *path = NULL;

	int opt;
	while (0 < (opt = getopt(argc, argv, "t:"))) {
		switch (opt) {
			break; case 't': path = optarg;
			break; default:  return EX_USAGE;
		}
	}

	if (path) {
		FILE *file = fopen(path, "r");
		if (!file) err(EX_NOINPUT, "%s", path);
		scoresRead(file);
		printf("%s\n", boardTitle("TOP SCORES"));
		printf("%s\n", boardLine());
		for (size_t i = 0; i < ScoresLen; ++i) {
			if (!scores[i].score) break;
			printf("%s\n", boardScore(i));
		}
		return EX_OK;
	}

	if (!isatty(STDOUT_FILENO)) {
		errx(EX_USAGE, "not a tty; use ssh -t");
	}
	curse();

	const struct Game *game = menu();
	if (!game) goto done;
	erase();
#ifdef __FreeBSD__
	setproctitle("%s", game->name);
#endif

	char buf[256];
	snprintf(buf, sizeof(buf), "%s.scores", game->name);
	FILE *top = scoresOpen(buf);
	snprintf(buf, sizeof(buf), "%s.weekly", game->name);
	FILE *weekly = scoresOpen(buf);

#ifdef __FreeBSD__
	int error = cap_enter();
	if (error) err(EX_OSERR, "cap_enter");

	cap_rights_t rights;
	cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_FLOCK);

	error = cap_rights_limit(fileno(top), &rights);
	if (error) err(EX_OSERR, "cap_rights_limit");

	error = cap_rights_limit(fileno(weekly), &rights);
	if (error) err(EX_OSERR, "cap_rights_limit");
#endif

	struct Score new = {
		.date = time(NULL),
		.score = game->play(),
	};

	curse();

	scoresRead(weekly);
	size_t index = scoresInsert(new);
	draw("WEEKLY SCORES", index);

	if (index < ScoresLen) {
		attr_set(A_BOLD, 0, NULL);
		while (!new.name[0]) {
			int y, x;
			getyx(stdscr, y, x);
			getnstr(new.name, sizeof(new.name) - 1);
			move(y, x);
		}
		for (char *ch = new.name; *ch; ++ch) {
			if (*ch < ' ') *ch = ' ';
		}

		scoresLock(weekly);
		scoresRead(weekly);
		scoresInsert(new);
		scoresWrite(weekly);
		fclose(weekly);
	}

	noecho();
	curs_set(0);
	getch();
	erase();

	scoresRead(top);
	index = scoresInsert(new);
	draw("TOP SCORES", index);

	if (index < ScoresLen) {
		scoresLock(top);
		scoresRead(top);
		scoresInsert(new);
		scoresWrite(top);
		fclose(top);
	}

	getch();
done:
	endwin();
	printf(
		"This program is AGPLv3 Free Software!\n"
		"Code is available from <https://git.causal.agency/play>.\n"
	);
}
