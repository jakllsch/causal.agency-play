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

#ifdef __FreeBSD__
#include <sys/capsicum.h>
#endif

typedef unsigned uint;

uint play2048(void);

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
	ScoresWidth = RankWidth + 2 + ScoreWidth + 2 + NameWidth + 2 + DateWidth,
	ScoresY = 0,
	ScoresX = 2,
	NameX = ScoresX + RankWidth + 2 + ScoreWidth + 2,
	ScoresTop = 15,
};
static const char ScoresTitle[] = "TOP SCORES";

static const char *formatScore(size_t i) {
	struct tm *time = localtime(&scores[i].date);
	if (!time) err(EX_SOFTWARE, "localtime");

	char date[DateWidth + 1];
	strftime(date, sizeof(date), "%F", time);

	static char buf[ScoresWidth + 1];
	snprintf(
		buf, sizeof(buf),
		"%*zu. %*u  %-*s  %*s",
		RankWidth, 1 + i,
		ScoreWidth, scores[i].score,
		NameWidth, scores[i].name,
		DateWidth, date
	);
	return buf;
}

static void draw(size_t new) {
	mvaddstr(
		ScoresY, ScoresX + (ScoresWidth - sizeof(ScoresTitle) + 2) / 2,
		ScoresTitle
	);
	mvhline(ScoresY + 1, ScoresX, '=', ScoresWidth);

	int newY = -1;
	for (size_t i = 0; i < ScoresTop; ++i) {
		if (!scores[i].score) break;
		if (i == new) newY = ScoresY + 2 + i;
		attr_set(i == new ? A_BOLD : A_NORMAL, 0, NULL);
		mvaddstr(ScoresY + 2 + i, ScoresX, formatScore(i));
	}
	if (new == ScoresLen) return;

	if (new >= ScoresTop) {
		newY = ScoresY + ScoresTop + 5;
		mvhline(newY - 3, ScoresX, '=', ScoresWidth);
		mvaddstr(newY - 2, ScoresX, formatScore(new - 2));
		mvaddstr(newY - 1, ScoresX, formatScore(new - 1));
		attr_set(A_BOLD, 0, NULL);
		mvaddstr(newY, ScoresX, formatScore(new));
		attr_set(A_NORMAL, 0, NULL);
		if (new + 1 < ScoresLen && scores[new + 1].score) {
			mvaddstr(newY + 1, ScoresX, formatScore(new + 1));
		}
		if (new + 2 < ScoresLen && scores[new + 2].score) {
			mvaddstr(newY + 2, ScoresX, formatScore(new + 2));
		}
	}
	move(newY, NameX);
}

int main(int argc, char *argv[]) {
	FILE *file = scoresOpen("2048.scores");

	if (argc > 1 && !strcmp(argv[1], "-t")) {
		scoresRead(file);
		printf(
			"%*s\n",
			ScoresWidth / 2 + (int)(sizeof(ScoresTitle) + 2) / 2,
			ScoresTitle
		);
		for (uint i = 0; i < ScoresWidth; ++i) putchar('=');
		putchar('\n');
		for (size_t i = 0; i < ScoresLen; ++i) {
			if (!scores[i].score) break;
			printf("%s\n", formatScore(i));
		}
		return EX_OK;
	}

	curse();

#ifdef __FreeBSD__
	int error = cap_enter();
	if (error) err(EX_OSERR, "cap_enter");

	cap_rights_t rights;
	cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_FLOCK);

	error = cap_rights_limit(fileno(file), &rights);
	if (error) err(EX_OSERR, "cap_rights_limit");
#endif

	struct Score new = {
		.date = time(NULL),
		.score = play2048(),
	};

	scoresRead(file);
	size_t index = scoresInsert(new);

	curse();
	draw(index);
	if (index < ScoresLen) {
		attr_set(A_BOLD, 0, NULL);
		getnstr(new.name, sizeof(new.name) - 1);
		for (char *ch = new.name; *ch; ++ch) {
			if (*ch < ' ') *ch = ' ';
		}

		scoresLock(file);
		scoresRead(file);
		scoresInsert(new);
		scoresWrite(file);
		fclose(file);
	}

	noecho();
	curs_set(0);
	getch();
	endwin();
	printf(
		"This program is AGPLv3 Free Software!\n"
		"Code is available from <https://code.causal.agency/june/play>.\n"
	);
}
