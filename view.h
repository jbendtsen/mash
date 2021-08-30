#pragma once

struct File {
	char os_handle[16];

	char *data;
	int64_t total_size;

	int open(const char *name);
	void close();
};

struct Syntax_Mode {
	char accepted_min[8]; // .eg "0Aa_"
	char accepted_max[8]; // .eg "9Zz_"
	int fore_color_idx;
	int back_color_idx;
	int glyphset; // 0 = regular, 1 = bold, 2 = italic, 3 = bold italic
	int modifier; // 0 = normal, 1 = strikethrough, 2 = underline
};

struct Syntax_Token {
	char *str;
	int len;
	int required_mode;
	int mode_of;
	int mode_switch;

	int matches; // modified by update_highlighter()
};

struct Formatter {
	static constexpr int N_COLORS = 32;
	uint32_t colors[N_COLORS];

	static constexpr int N_MODES = 32;
	Syntax_Mode modes[N_MODES];

	int cur_mode;
	int spaces_per_tab;

	void get_current_attrs(uint32_t& fore, uint32_t& back, uint32_t& glyph_off, uint32_t& modifier) {
		Syntax_Mode& mode = modes[cur_mode];
		fore = colors[mode.fore_color_idx];
		back = colors[mode.back_color_idx];
		glyph_off = mode.glyphset * 0x60;
		modifier = mode.modifier;
	}

	void update_highlighter(File *file, int64_t offset, char c);
};

struct Mouse_State {
	int left_flags;
	int right_flags;
	int x, y;

	void advance() {
		left_flags &= 1;
		left_flags |= left_flags << 1;
		right_flags &= 1;
		right_flags |= right_flags << 1;
	}
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
	int64_t primary_cursor;

	int rel_cursor_col;
	int rel_cursor_row;
	int mode_at_current_line;

	void render_into(File *file, Cell *cells, Formatter *formatter, Mouse_State& mouse);
	void adjust_offsets(File *file, int64_t move_down, int64_t move_right);
};

struct View {
	Grid *grid;
	File *file;
	Formatter *formatter;
	int font_render_idx;
};
