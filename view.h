#pragma once

struct Highlighter {
	static constexpr int N_COLORS = 32;
	uint32_t colors[N_COLORS];
};

struct Text {
	char *data;
	int64_t total_size;

	int64_t file_begin;
	int64_t lines_down;

	int64_t *newlines;
	int nl_capacity;
	int nl_size;

	void enumerate_newlines();
};

struct Cell {
	uint32_t glyph;
	uint32_t modifier;
	uint32_t foreground;
	uint32_t background;
};

struct Grid {
	int rows;
	int cols;
	int64_t row_offset;
	int64_t col_offset;

	void render_into(Text *text, Cell *cells, Highlighter *syntax);
};

struct View {
	Grid *grid;
	Text *text;
	Highlighter *highlighter;
	int font_render_idx;
};
