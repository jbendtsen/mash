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
	int64_t line_offset;

	void render_into(File *file, Cell *cells, Formatter *formatter);
	void adjust_offsets(File *file, int64_t move_down, int64_t move_right);
};

struct View {
	Grid *grid;
	File *file;
	Formatter *formatter;
	int font_render_idx;
};
