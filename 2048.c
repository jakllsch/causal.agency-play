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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned uint;

static uint score;
static uint grid[4][4];

static void spawn(void) {
	uint y, x;
	do {
		y = arc4random_uniform(4);
		x = arc4random_uniform(4);
	} while (grid[y][x]);
	grid[y][x] = (arc4random_uniform(10) ? 1 : 2);
}

static bool slideLeft(void) {
	bool slid = false;
	for (uint y = 0; y < 4; ++y) {
		uint x = 0;
		for (uint i = 0; i < 4; ++i) {
			if (grid[y][i] != grid[y][x]) slid = true;
			if (grid[y][i]) grid[y][x++] = grid[y][i];
		}
		while (x < 4) grid[y][x++] = 0;
	}
	return slid;
}
static bool slideRight(void) {
	bool slid = false;
	for (uint y = 0; y < 4; ++y) {
		uint x = 3;
		for (uint i = 3; i < 4; --i) {
			if (grid[y][i] != grid[y][x]) slid = true;
			if (grid[y][i]) grid[y][x--] = grid[y][i];
		}
		while (x < 4) grid[y][x--] = 0;
	}
	return slid;
}
static bool slideUp(void) {
	bool slid = false;
	for (uint x = 0; x < 4; ++x) {
		uint y = 0;
		for (uint i = 0; i < 4; ++i) {
			if (grid[i][x] != grid[y][x]) slid = true;
			if (grid[i][x]) grid[y++][x] = grid[i][x];
		}
		while (y < 4) grid[y++][x] = 0;
	}
	return slid;
}
static bool slideDown(void) {
	bool slid = false;
	for (uint x = 0; x < 4; ++x) {
		uint y = 3;
		for (uint i = 3; i < 4; --i) {
			if (grid[i][x] != grid[y][x]) slid = true;
			if (grid[i][x]) grid[y--][x] = grid[i][x];
		}
		while (y < 4) grid[y--][x] = 0;
	}
	return slid;
}

static bool mergeLeft(void) {
	bool merged = false;
	for (uint y = 0; y < 4; ++y) {
		for (uint x = 0; x < 3; ++x) {
			if (!grid[y][x]) continue;
			if (grid[y][x] != grid[y][x + 1]) continue;
			score += 1 << ++grid[y][x];
			grid[y][x + 1] = 0;
			merged = true;
		}
	}
	return merged;
}
static bool mergeRight(void) {
	bool merged = false;
	for (uint y = 0; y < 4; ++y) {
		for (uint x = 3; x > 0; --x) {
			if (!grid[y][x]) continue;
			if (grid[y][x] != grid[y][x - 1]) continue;
			score += 1 << ++grid[y][x];
			grid[y][x - 1] = 0;
			merged = true;
		}
	}
	return merged;
}
static bool mergeUp(void) {
	bool merged = false;
	for (uint x = 0; x < 4; ++x) {
		for (uint y = 0; y < 3; ++y) {
			if (!grid[y][x]) continue;
			if (grid[y][x] != grid[y + 1][x]) continue;
			score += 1 << ++grid[y][x];
			grid[y + 1][x] = 0;
			merged = true;
		}
	}
	return merged;
}
static bool mergeDown(void) {
	bool merged = false;
	for (uint x = 0; x < 4; ++x) {
		for (uint y = 3; y > 0; --y) {
			if (!grid[y][x]) continue;
			if (grid[y][x] != grid[y - 1][x]) continue;
			score += 1 << ++grid[y][x];
			grid[y - 1][x] = 0;
			merged = true;
		}
	}
	return merged;
}

static bool left(void) {
	return slideLeft() | mergeLeft() | slideLeft();
}
static bool right(void) {
	return slideRight() | mergeRight() | slideRight();
}
static bool up(void) {
	return slideUp() | mergeUp() | slideUp();
}
static bool down(void) {
	return slideDown() | mergeDown() | slideDown();
}

static void curse(void) {
	initscr();
	cbreak();
	noecho();
	curs_set(0);
	keypad(stdscr, true);
	leaveok(stdscr, true);
	start_color();
	use_default_colors();
	short bright = (COLORS > 8 ? 8 : 0);
	init_pair(1,  COLOR_WHITE, COLOR_RED);
	init_pair(2,  COLOR_WHITE, COLOR_GREEN);
	init_pair(3,  COLOR_WHITE, COLOR_YELLOW);
	init_pair(4,  COLOR_WHITE, COLOR_BLUE);
	init_pair(5,  COLOR_WHITE, COLOR_MAGENTA);
	init_pair(6,  COLOR_WHITE, COLOR_CYAN);
	init_pair(7,  COLOR_WHITE, bright + COLOR_RED);
	init_pair(8,  COLOR_WHITE, bright + COLOR_GREEN);
	init_pair(9,  COLOR_WHITE, bright + COLOR_YELLOW);
	init_pair(10, COLOR_WHITE, bright + COLOR_BLUE);
	init_pair(11, COLOR_WHITE, bright + COLOR_MAGENTA);
	init_pair(12, COLOR_WHITE, bright + COLOR_CYAN);
	init_pair(13, COLOR_WHITE, COLOR_BLACK);
}

static void addchn(char ch, uint n) {
	for (uint i = 0; i < n; ++i) {
		addch(ch);
	}
}

enum {
	TileHeight = 3,
	TileWidth = 7,
	GridY = 2,
	GridX = 2,
	ScoreY = 0,
	ScoreX = GridX + 4 * TileWidth - 10,
};

static void drawTile(uint y, uint x) {
	if (grid[y][x]) {
		attr_set(A_BOLD, 1 + (grid[y][x] - 1) % 12, NULL);
	} else {
		attr_set(A_NORMAL, 13, NULL);
	}

	char buf[8];
	int len = snprintf(buf, sizeof(buf), "%d", 1 << grid[y][x]);
	if (!grid[y][x]) buf[0] = '.';

	move(GridY + TileHeight * y, GridX + TileWidth * x);
	addchn(' ', TileWidth);

	move(GridY + TileHeight * y + 1, GridX + TileWidth * x);
	addchn(' ', (TileWidth - len + 1) / 2);
	addstr(buf);
	addchn(' ', (TileWidth - len) / 2);

	move(GridY + TileHeight * y + 2, GridX + TileWidth * x);
	addchn(' ', TileWidth);
}

static void draw(void) {
	char buf[11];
	snprintf(buf, sizeof(buf), "%10d", score);

	attr_set(A_NORMAL, 0, NULL);
	mvaddstr(ScoreY, ScoreX, buf);

	for (uint y = 0; y < 4; ++y) {
		for (uint x = 0; x < 4; ++x) {
			drawTile(y, x);
		}
	}
}

static bool input(void) {
	switch (getch()) {
		break; case 'h': case KEY_LEFT: if (left()) spawn();
		break; case 'j': case KEY_DOWN: if (down()) spawn();
		break; case 'k': case KEY_UP: if (up()) spawn();
		break; case 'l': case KEY_RIGHT: if (right()) spawn();
		break; case 'q': return false;
	}
	return true;
}

uint play2048(void) {
	curse();
	spawn();
	spawn();
	do {
		draw();
	} while (input());
	return score;
}
