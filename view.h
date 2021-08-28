#pragma once

struct Formatter {
	static constexpr int N_COLORS = 32;
	uint32_t colors[N_COLORS];
	int spaces_per_tab;
};

struct File {
	char os_handle[16];

	char *data;
	int64_t total_size;

	int open(const char *name);
	void close();
};

struct Text {
	File *file;

	int64_t file_offset;
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

	void render_into(Text *text, Cell *cells, Formatter *formatter);
};

struct View {
	Grid *grid;
	Text *text;
	Formatter *formatter;
	int font_render_idx;
};
