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

	rel_cursor_col = -1;
	rel_cursor_row = -1;

	bool mouse_held     = (mouse.left_flags & 1) != 0;
	bool mouse_was_held = (mouse.left_flags & 2) != 0;

	int idx = 0;
	int64_t offset = line_offset;

	formatter->cur_mode = mode_at_current_line;

	char *data = file->data;
	int64_t total_size = file->total_size;
	int spt = formatter->spaces_per_tab;

	for (int i = 0; i < rows && offset < total_size; i++) {
		int64_t char_cols = 0;
		int64_t vis_cols = 0;
		bool early_bail = false;

		while (vis_cols < col_offset) {
			char c = data[offset];
			formatter->update_highlighter(file, offset, c);
			offset++;

			if (c == '\n') {
				early_bail = true;
				break;
			}
			else if (c == '\t')
				vis_cols += spt - (vis_cols % spt);
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
		}

		for (column = 0; column < leading_cols; column++)
			cells[idx + column] = empty;

		while (column < cols && offset < total_size) {
			char c = data[offset];
			formatter->update_highlighter(file, offset, c);

			if (mouse_held && mouse.y == i && mouse.x == column) {
				primary_cursor = offset;
			}

			if (offset == primary_cursor) {
				rel_cursor_col = column;
				rel_cursor_row = i;
			}

			if (c == '\n')
				break;

			offset++;

			if (c == '\t') {
				int n_spaces = spt - (((int)col_offset + column) % spt);
				for (int j = 0; j < n_spaces; j++) {
					cells[idx + column] = empty;
					column++;
				}
				continue;
			}

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
