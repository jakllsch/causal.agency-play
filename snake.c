/* Copyright (C) 2021  C. McEnroe <june@causal.agency>
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
#include <unistd.h>

typedef unsigned uint;
typedef unsigned char byte;

enum {
	Rows = 24,
	Cols = 48,
};

static uint score;
static const char *over;

static struct {
	uint len;
	byte y[Rows * Cols];
	byte x[Rows * Cols];
} snake = { .len = 1 };

static struct {
	int y, x;
	int dy, dx;
} head = { Rows / 2, Cols / 2, 0, 1 };

enum {
	FoodCap = 25,
	FoodChance = 15,
	FoodRipe = Rows + Cols,
	FoodSpoil = FoodRipe + Cols,
	FoodMulch = FoodSpoil * 10,
};

static struct {
	uint len;
	byte y[FoodCap];
	byte x[FoodCap];
	uint age[FoodCap];
} food;

static void tick(void) {
	for (uint i = 0; i < food.len; ++i) {
		if (head.y + head.dy != food.y[i]) continue;
		if (head.x + head.dx != food.x[i]) continue;
		if (food.age[i] > FoodSpoil) {
			over = "You ate spoiled food!";
			return;
		}
		score += snake.len * (food.age[i] > FoodRipe ? 2 : 1);
		food.len--;
		food.y[i] = food.y[food.len];
		food.x[i] = food.x[food.len];
		food.age[i] = food.age[food.len];
		snake.len++;
		break;
	}
	for (uint i = food.len - 1; i < food.len; --i) {
		if (food.age[i]++ < FoodMulch) continue;
		food.len--;
		food.y[i] = food.y[food.len];
		food.x[i] = food.x[food.len];
		food.age[i] = food.age[food.len];
	}
	if (!food.len || (food.len < FoodCap && !arc4random_uniform(FoodChance))) {
		int y = arc4random_uniform(Rows);
		int x = arc4random_uniform(Cols);
		bool empty = true;
		if (y == head.y && x == head.x) empty = false;
		for (uint i = 0; i < snake.len; ++i) {
			if (y == snake.y[i] && x == snake.x[i]) empty = false;
		}
		for (uint i = 0; i < food.len; ++i) {
			if (y == food.y[i] && x == food.x[i]) empty = false;
		}
		if (empty) {
			food.y[food.len] = y;
			food.x[food.len] = x;
			food.age[food.len] = 0;
			food.len++;
		}
	}
	for (uint i = snake.len - 1; i < snake.len; --i) {
		if (i) {
			snake.y[i] = snake.y[i-1];
			snake.x[i] = snake.x[i-1];
		} else {
			snake.y[i] = head.y;
			snake.x[i] = head.x;
		}
	}
	head.y += head.dy;
	head.x += head.dx;
	if (head.y < 0 || head.x < 0 || head.y >= Rows || head.x >= Cols) {
		over = "You eated the wall D:";
	}
	for (uint i = 0; i < snake.len; ++i) {
		if (head.y != snake.y[i] || head.x != snake.x[i]) continue;
		over = "You eated yourself :(";
	}
}

enum { KeyOK = KEY_MAX + 1 };
static void curse(void) {
	initscr();
	cbreak();
	noecho();
	curs_set(0);
	keypad(stdscr, true);
	nodelay(stdscr, true);
	define_key("\33[0n", KeyOK);
	start_color();
	use_default_colors();
	init_pair(1, COLOR_GREEN, -1);
	init_pair(2, COLOR_YELLOW, -1);
	init_pair(3, COLOR_RED, -1);
	mvhline(Rows, 0, ACS_HLINE, Cols);
	mvvline(0, Cols, ACS_VLINE, Rows);
	mvaddch(Rows, Cols, ACS_LRCORNER);
}

static void draw(void) {
	char buf[16];
	snprintf(buf, sizeof(buf), "%u", score);
	mvaddstr(0, Cols + 2, buf);
	if (over) {
		mvaddstr(2, Cols + 2, over);
		mvaddstr(3, Cols + 2, "Press any key to");
		mvaddstr(4, Cols + 2, "view the scoreboard.");
	}
	for (int y = 0; y < Rows; ++y) {
		mvhline(y, 0, ' ', Cols);
	}
	for (uint i = 0; i < food.len; ++i) {
		if (food.age[i] > FoodSpoil) {
			mvaddch(food.y[i], food.x[i], '*' | COLOR_PAIR(3));
		} else if (food.age[i] > FoodRipe) {
			mvaddch(food.y[i], food.x[i], '%' | COLOR_PAIR(2));
		} else {
			mvaddch(food.y[i], food.x[i], '&' | COLOR_PAIR(1));
		}
	}
	for (uint i = 0; i < snake.len; ++i) {
		if (i + 1 < snake.len) {
			mvaddch(snake.y[i], snake.x[i], '#' | COLOR_PAIR(2));
		} else {
			mvaddch(snake.y[i], snake.x[i], '*' | COLOR_PAIR(2));
		}
	}
	mvaddch(head.y, head.x, '@' | A_BOLD);
	move(head.y, head.x);
	refresh();
}

static void input(void) {
	int ch = getch();
	if (ch == ERR) {
		putp("\33[5n");
		fflush(stdout);
		nodelay(stdscr, false);
		ch = getch();
		nodelay(stdscr, true);
	}
	int dy = head.dy;
	int dx = head.dx;
	switch (ch) {
		break; case 'h': case KEY_LEFT:  dy =  0; dx = -1;
		break; case 'j': case KEY_DOWN:  dy = +1; dx =  0;
		break; case 'k': case KEY_UP:    dy = -1; dx =  0;
		break; case 'l': case KEY_RIGHT: dy =  0; dx = +1;
		break; case 'q': over = "You are satisfied.";
		break; case ERR: exit(EXIT_FAILURE);
	}
	if (dy == -head.dy && dx == -head.dx) return;
	head.dy = dy;
	head.dx = dx;
}

uint playSnake(void) {
	curse();
	do {
		tick();
		draw();
		usleep(150000);
		input();
	} while (!over);
	draw();
	while (ERR != getch());
	nodelay(stdscr, false);
	int ch;
	do {
		ch = getch();
	} while (
		ch == KEY_LEFT || ch == KEY_DOWN || ch == KEY_UP || ch == KEY_RIGHT
	);
	return score;
}

int main(void) {
	playSnake();
	endwin();
}
