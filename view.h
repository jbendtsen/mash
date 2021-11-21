#pragma once

#define THUMB_WIDTH 14
#define THUMB_FRAC 0.15625

#define JUMP_FLAG_TOP            1
#define JUMP_FLAG_AFFECT_COLUMN  2

template <typename T>
struct Vector {
	static constexpr int INLINE_SIZE = 16;

	int cap;
	int size;
	T *data;
	T stack[INLINE_SIZE];

	Vector() {
		cap = INLINE_SIZE;
		size = INLINE_SIZE;
		data = &stack[0];
		stack = {0};
	}
	~Vector() {
		if (data && data != &stack[0])
			delete[] data;
	}

	void resize(int sz) {
		int new_cap = cap;
		while (sz > new_cap)
			new_cap *= 2;

		if (new_cap <= cap) {
			size = sz;
			return;
		}

		T *new_data = new T[new_cap]();
		if (size > 0)
			memcpy(new_data, data, size * sizeof(T));
		if (data != &stack[0])
			delete[] data;

		data = new_data;
		cap = new_cap;
		size = sz;
	}
};

struct File {
	char os_handle[16];

	char *data;
	int64_t total_size;

	int open(const char *name);
	void close();
};

struct Syntax_Mode {
	char accepted_min[8]; // eg. "0Aa_"
	char accepted_max[8]; // eg. "9Zz_"
	int fore_color_idx;
	int back_color_idx;
	int glyphset; // 0 = regular, 1 = bold, 2 = italic, 3 = bold italic
	int modifier; // 0 = normal, 1 = strikethrough, 2 = underline
};

struct Syntax_Token {
	char *str;
	int len;
	char required_mode_min[4]; // eg. 0, 2
	char required_mode_max[4]; // eg. 0, -1
	int mode_of;
	int mode_switch;

	int matches; // modified by update_highlighter()
};

struct Formatter {
	static constexpr int N_MODES = 32;
	Syntax_Mode modes[N_MODES];

	static constexpr int N_COLORS = 32;
	uint32_t colors[N_COLORS];

	uint32_t active_thumb_color;
	uint32_t hovered_thumb_color;
	uint32_t inactive_thumb_color;

	int cur_mode;

	void get_current_attrs(uint32_t& fore, uint32_t& back, uint32_t& glyph_off, uint32_t& modifier) {
		Syntax_Mode& mode = modes[cur_mode];
		fore = colors[mode.fore_color_idx];
		back = colors[mode.back_color_idx];
		glyph_off = mode.glyphset * 0x60;
		modifier = mode.modifier;
	}

	void update_highlighter(File *file, int64_t offset, char c);
};

struct Input_State {
	int left_flags;
	int right_flags;
	int column;
	int row;
	int x, y;
	int mod_flags;
	int thumb_inner_pos;
	int thumb_flags;

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
	int64_t primary_cursor;
	int64_t secondary_cursor;

	int rel_caret_col;
	int rel_caret_row;
	int target_column;

	int last_line_num_gap;
	bool text_held;

	int mode_at_current_line;

	int spaces_per_tab;

	int64_t grid_offset;
	int64_t end_grid_offset;
	// Vector<int64_t> line_offsets;

	void render_into(File *file, Cell *cells, Formatter *formatter, Input_State& mouse, int wnd_width, int wnd_height);
	void move_cursor_vertically(File *file, int dir, int target_col);
	void adjust_offsets(File *file, int64_t move_down, int64_t move_right);
	int64_t jump_to_offset(File *file, int64_t offset, int flags);
};

struct View {
	Grid *grid;
	File *file;
	Formatter *formatter;
	int font_render_idx;
};
