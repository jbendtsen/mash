#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "font.h"
#include "view.h"

#ifndef alloca
#define alloca _alloca
#endif

void Formatter::update_highlighter(File *file, int64_t offset, char c) {
	
}

void Grid::render_into(File *file, Cell *cells, Formatter *formatter, Input_State& input) {
	uint32_t default_bg = formatter->colors[0];
	uint32_t hl_color = formatter->colors[2];

	bool hl =
		((primary_cursor < grid_offset && secondary_cursor >= grid_offset) ||
		(primary_cursor >= grid_offset && secondary_cursor < grid_offset));

	Cell empty = {
		.background = hl ? hl_color : default_bg
	};

	rel_caret_col = -1;
	rel_caret_row = -1;

	//line_offsets.resize(rows);

	bool mouse_held     = (input.left_flags & 1) != 0;
	bool mouse_was_held = (input.left_flags & 2) != 0;

	int idx = 0;
	int64_t offset = grid_offset;
	int64_t new_cursor = primary_cursor;

	formatter->cur_mode = mode_at_current_line;

	char *data = file->data;
	int64_t total_size = file->total_size;

	int line = 0;
	for ( ; line < rows && offset < total_size; line++) {
		int64_t char_cols = 0;
		int64_t vis_cols = 0;
		bool early_bail = false;
		bool cursor_set = false;

		//line_offsets.data[line] = offset;

		while (vis_cols < col_offset) {
			char c = data[offset];
			formatter->update_highlighter(file, offset, c);

			if (primary_cursor != secondary_cursor && (offset == primary_cursor || offset == secondary_cursor))
				hl = !hl;

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
			empty.background = hl ? hl_color : default_bg;

			for (int j = 0; j < cols; j++)
				cells[idx++] = empty;

			continue;
		}

		// If we reached a tab character that spans over the given column offset
		int column = 0;
		int leading_cols = (int)(vis_cols - col_offset);

		if (mouse_held && input.y == line && input.x >= 0 && input.x < leading_cols) {
			new_cursor = offset;
			hl = !hl;
			cursor_set = true;
		}

		empty.background = hl ? hl_color : default_bg;

		for (column = 0; column < leading_cols; column++)
			cells[idx + column] = empty;

		while (column < cols && offset < total_size) {
			char c = data[offset];
			formatter->update_highlighter(file, offset, c);

			int n_spaces = c == '\t' ? spaces_per_tab - (((int)col_offset + column) % spaces_per_tab) : 1;

			if (mouse_held && input.y == line && input.x >= column && input.x < column + n_spaces) {
				new_cursor = offset;
				cursor_set = true;
			}
			if (offset == new_cursor) {
				rel_caret_col = column;
				rel_caret_row = line;
			}

			if (primary_cursor != secondary_cursor && (offset == primary_cursor || offset == secondary_cursor))
				hl = !hl;

			if (c == '\t') {
				for (int j = 0; j < n_spaces; j++) {
					cells[idx + column] = empty;
					column++;
				}

				offset++;
				continue;
			}

			if (c == '\n')
				break;

			offset++;

			if (c < ' ' || c > '~')
				c = 0x7f;

			uint32_t fg, bg, glyph_off, modifier;
			formatter->get_current_attrs(fg, bg, glyph_off, modifier);

			if (hl)
				bg = hl_color;

			cells[idx + column] = {
				.glyph = (uint32_t)(c - ' ') + glyph_off,
				.modifier = modifier,
				.foreground = fg,
				.background = bg
			};

			column++;
		}

		// target_cursor_col is intentionally not updated here
		if (mouse_held && input.y == line && !cursor_set) {
			new_cursor = offset;
			rel_caret_col = column;
			rel_caret_row = line;
		}

		empty.background = hl ? hl_color : default_bg;

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

	primary_cursor = new_cursor;

	//if (!input.should_hl) secondary_cursor = primary_cursor
	if (mouse_held && !mouse_was_held)
		secondary_cursor = primary_cursor;

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
		int n = -dir + 1;
		for (int i = 0; i < n; i++) {
			while (offset > 0) {
				char c = data[--offset];
				if (c == '\n')
					break;
			}
		}

		if (offset > 0)
			offset++;
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
		grid_offset = 0;
	}
	else if (row_offset > old_row_off) {
		int64_t r = old_row_off;

		while (r < row_offset && grid_offset < size) {
			if (data[grid_offset++] == '\n')
				r++;
		}
	}
	else if (row_offset < old_row_off) {
		int64_t r = old_row_off;

		while (r >= row_offset && grid_offset > 0) {
			if (data[--grid_offset] == '\n')
				r--;
		}
		grid_offset++;
	}
}

void Grid::jump_to_offset(File *file, int64_t offset) {
	if (offset < 0)
		offset = 0;
	if (offset > file->total_size)
		offset = file->total_size;

	int64_t row = 0;
	int64_t col = 0;

	int64_t rows_64 = (int64_t)rows;
	int64_t cols_64 = (int64_t)cols;
	int64_t spt_64 = (int64_t)spaces_per_tab;

	char *data = file->data;
	int64_t size = file->total_size;

	if (offset < grid_offset) {
		// aim for n_cols/2

		int64_t off = grid_offset;
		int64_t lines_up = 0;

		while (off > offset) {
			char c = data[--off];
			if (c == '\n') {
				lines_up++;
			}
		}

		row_offset -= lines_up;
		off = offset;

		while (off > 0) {
			char c = data[--off];
			if (c == '\n') {
				off++;
				break;
			}

			// this is likely wrong
			if (c == '\t')
				col += spt_64 - (col % spt_64);
			else
				col++;
		}

		grid_offset = off;
	}
	else {
		int64_t off = grid_offset;
		int64_t lines_down = 0;

		int64_t *offset_list = (int64_t*)alloca(rows * sizeof(int64_t));
		offset_list[0] = grid_offset;

		while (off < offset) {
			char c = data[off++];
			if (c == '\n') {
				lines_down++;
				offset_list[lines_down % rows_64] = off;
				col = 0;
			}
			else if (c == '\t')
				col += spt_64 - (col % spt_64);
			else
				col++;
		}

		if (lines_down >= rows_64) {
			grid_offset = offset_list[(lines_down + 1) % rows_64];
			row_offset += lines_down - rows_64 + 1;
		}
	}

	col_offset = 0;

	int64_t mid = cols_64 / 2;
	if (col >= cols_64)
		col_offset = col - mid;

	primary_cursor = offset;
}
