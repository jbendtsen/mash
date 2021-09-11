#include <cstdint>
#include <cstring>
#include "font.h"
#include "view.h"

void Formatter::update_highlighter(File *file, int64_t offset, char c) {
	
}

void Grid::render_into(File *file, Cell *cells, Formatter *formatter, Mouse_State& mouse) {
	const Cell empty = {
		.background = formatter->colors[0]
	};

	rel_caret_col = -1;
	rel_caret_row = -1;

	bool mouse_held     = (mouse.left_flags & 1) != 0;
	bool mouse_was_held = (mouse.left_flags & 2) != 0;

	int idx = 0;
	int64_t offset = line_offset;

	formatter->cur_mode = mode_at_current_line;

	char *data = file->data;
	int64_t total_size = file->total_size;

	for (int i = 0; i < rows && offset < total_size; i++) {
		int64_t char_cols = 0;
		int64_t vis_cols = 0;
		bool early_bail = false;
		bool cursor_set = false;

		while (vis_cols < col_offset) {
			char c = data[offset];
			formatter->update_highlighter(file, offset, c);
			offset++;

			if (c == '\n') {
				early_bail = true;
				break;
			}
			else if (c == '\t')
				vis_cols += spaces_per_tab - (vis_cols % spaces_per_tab);
			else
				vis_cols++;
		}

		if (early_bail) {
			for (int j = 0; j < cols; j++)
				cells[idx++] = empty;

			continue;
		}

		// If we reached a tab character that spans over the given column offset
		int column = 0;
		int leading_cols = (int)(vis_cols - col_offset);

		if (mouse_held && mouse.y == i && mouse.x < leading_cols) {
			primary_cursor = offset;
			cursor_set = true;
		}

		for (column = 0; column < leading_cols; column++)
			cells[idx + column] = empty;

		while (column < cols && offset < total_size) {
			char c = data[offset];
			formatter->update_highlighter(file, offset, c);

			if (mouse_held && mouse.y == i && mouse.x == column) {
				primary_cursor = offset;
				cursor_set = true;
			}

			if (c == '\t') {
				int n_spaces = spaces_per_tab - (((int)col_offset + column) % spaces_per_tab);

				if (mouse_held && mouse.y == i && mouse.x >= column && mouse.x < column + n_spaces) {
					primary_cursor = offset;
					cursor_set = true;
				}
				if (primary_cursor == offset) {
					rel_caret_col = column;
					rel_caret_row = i;
				}

				for (int j = 0; j < n_spaces; j++) {
					cells[idx + column] = empty;
					column++;
				}

				offset++;
				continue;
			}

			if (offset == primary_cursor) {
				rel_caret_col = column;
				rel_caret_row = i;
			}

			if (c == '\n')
				break;

			offset++;

			if (c < ' ' || c > '~')
				c = 0x7f;

			uint32_t fg, bg, glyph_off, modifier;
			formatter->get_current_attrs(fg, bg, glyph_off, modifier);

			cells[idx + column] = {
				.glyph = (uint32_t)(c - ' ') + glyph_off,
				.modifier = modifier,
				.foreground = fg,
				.background = bg
			};

			column++;
		}

		// target_cursor_col is intentionally not updated here
		if (mouse_held && mouse.y == i && !cursor_set) {
			primary_cursor = offset;
			rel_caret_col = column;
			rel_caret_row = i;
		}

		for (int j = column; j < cols; j++)
			cells[idx + j] = empty;

		idx += cols;

		if (offset < total_size) {
			char c = 0;
			do {
				c = data[offset];
				formatter->update_highlighter(file, offset, c);
				offset++;
			} while (c != '\n');
		}
	}

	int grid_size = rows * cols;
	for (int i = idx; i < grid_size; i++)
		cells[i] = empty;
}

void Grid::move_cursor_vertically(File *file, int dir, int64_t target_col) {
	char *data = file->data;
	int64_t size = file->total_size;
	int64_t spt_64 = (int64_t)spaces_per_tab;
	int64_t offset = primary_cursor;

	if (dir > 0) {
		for (int i = 0; i < dir; i++) {
			while (offset < size) {
				char c = data[offset++];
				if (c == '\n')
					break;
			}

			if (offset >= size) {
				primary_cursor = size;
				return;
			}
		}
	}
	else {
		for (int i = 0; i < dir+1; i++) {
			while (offset > 0) {
				char c = data[--offset];
				if (c == '\n')
					break;
			}
		}

		if (offset > 0)
			offset += 2;
	}

	int64_t col = 0;
	while (col < target_col && offset < size) {
		char c = data[offset];
		if (c == '\n')
			break;

		col += c == '\t' ? spt_64 - (col % spt_64) : 1;
		offset++;
	}

	primary_cursor = offset;
}

void Grid::adjust_offsets(File *file, int64_t move_down, int64_t move_right) {
	int64_t old_row_off = row_offset;
	int64_t temp = row_offset + move_down;
	row_offset = temp >= 0 ? temp : 0;

	temp = col_offset + move_right;
	col_offset = temp >= 0 ? temp : 0;

	char *data = file->data;
	int64_t size = file->total_size;

	if (row_offset == 0) {
		line_offset = 0;
	}
	else if (row_offset > old_row_off) {
		int64_t r = old_row_off;

		while (r < row_offset && line_offset < size) {
			if (data[line_offset++] == '\n')
				r++;
		}
	}
	else if (row_offset < old_row_off) {
		int64_t r = old_row_off;

		while (r >= row_offset && line_offset > 0) {
			if (data[--line_offset] == '\n')
				r--;
		}
		line_offset++;
	}
}

bool Grid::outside_grid(File *file, int64_t offset) {
	if (offset < line_offset)
		return true;

	int64_t off = line_offset;
	int64_t row = 0;
	int64_t col = 0;

	int64_t rows_64 = (int64_t)rows;
	int64_t cols_64 = (int64_t)cols;
	int64_t spt_64 = (int64_t)spaces_per_tab;

	char *data = file->data;
	int64_t size = file->total_size;

	while (row < rows_64 && off <= size) {
		if (off == offset)
			return col < col_offset || col >= col_offset + cols_64;

		char c = data[off++];
		if (c == '\n') {
			row++;
			col = 0;
		}
		else if (c == '\t')
			col += spt_64 - (col % spt_64);
		else
			col++;
	}

	return true;
}

void Grid::jump_to(File *file, int64_t offset) {
	if (offset < 0)
		offset = 0;
	if (offset > file->total_size)
		offset = file->total_size;

	if (offset < line_offset) {
		
	}
	else {
		
	}
}
