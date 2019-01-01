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
	ScoresTop = 15,
	ScoresY = 0,
	ScoresX = 2,
	ScoreX = ScoresX + 5,
	NameX = ScoreX + 12,
	DateX = NameX + 33,
	ScoresWidth = DateX + 11 - ScoresX,
};
static const char ScoresTitle[] = "TOP SCORES";

static void drawScore(int y, size_t i) {
	char buf[11];
	snprintf(buf, sizeof(buf), "%3zu.", 1 + i);
	mvaddstr(y, ScoresX, buf);

	snprintf(buf, sizeof(buf), "%10d", scores[i].score);
	mvaddstr(y, ScoreX, buf);

	mvaddstr(y, NameX, scores[i].name);

	struct tm *time = localtime(&scores[i].date);
	if (!time) err(EX_SOFTWARE, "localtime");
	char date[sizeof("YYYY-MM-DD")];
	strftime(date, sizeof(date), "%F", time);
	mvaddstr(y, DateX, date);
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
		drawScore(ScoresY + 2 + i, i);
	}
	if (new == ScoresLen) return;

	if (new >= ScoresTop) {
		newY = ScoresY + ScoresTop + 5;
		mvhline(newY - 3, ScoresX, '=', ScoresWidth);
		drawScore(newY - 2, new - 2);
		drawScore(newY - 1, new - 1);
		attr_set(A_BOLD, 0, NULL);
		drawScore(newY, new);
		attr_set(A_NORMAL, 0, NULL);
		if (new + 1 < ScoresLen && scores[new + 1].score) {
			drawScore(newY + 1, new + 1);
		}
		if (new + 2 < ScoresLen && scores[new + 2].score) {
			drawScore(newY + 2, new + 2);
		}
	}
	move(newY, NameX);
}

int main(void) {
	curse();
	FILE *file = scoresOpen("2048.scores");

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
