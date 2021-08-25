#include <cstdint>
#include <cstring>
#include "font.h"
#include "view.h"

void Text::enumerate_newlines() {
	if (!newlines) {
		nl_capacity = 1000;
		newlines = new int64_t[nl_capacity];
	}

	int64_t end = file_offset + (int64_t)(1024 * 1024);
	if (end > total_size)
		end = total_size;

	nl_size = 0;
	bool was_nl = true;
	for (int64_t i = file_offset; i < end; i++) {
		was_nl = data[i] == '\n';
		if (!was_nl)
			continue;

		newlines[nl_size++] = i;
		// plus one to allow for a pseudo newline to signal end of file
		if (nl_size + 1 >= nl_capacity) {
			int64_t old_cap = nl_capacity;
			nl_capacity *= 2;
			int64_t *temp = new int64_t[nl_capacity];
			memcpy(temp, newlines, old_cap);
			delete[] newlines;
			newlines = temp;
		}
	}

	if (!was_nl)
		newlines[nl_size++] = end;
}

void Grid::render_into(Text *text, Cell *cells, Formatter *formatter) {
	const Cell empty = {
		.background = formatter->colors[0]
	};

	int line = (int)(row_offset - text->lines_down);
	int idx = 0;

	int64_t offset = line > 0 ? text->newlines[line - 1] + 1LL : 0;

	int spt = formatter->spaces_per_tab;

	for (int i = 0; i < rows && offset < text->total_size; i++) {
		int64_t eol = text->newlines[line + i];
		int64_t char_cols = 0;
		int64_t vis_cols = 0;

		while (vis_cols < col_offset) {
			if (text->data[offset++] == '\t')
				vis_cols += spt - (vis_cols % spt);
			else
				vis_cols++;
		}

		int column = 0;
		int leading_cols = (int)(vis_cols - col_offset);
		for (column = 0; column < leading_cols; column++)
			cells[idx + column] = empty;

		while (column < cols && offset < eol) {
			char c = text->data[offset++];

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

			cells[idx + column] = {
				.glyph = (uint32_t)(c + 0xc0 - ' '),
				.modifier = 0,
				.foreground = formatter->colors[1],
				.background = formatter->colors[0]
			};

			column++;
		}

		for (int j = column; j < cols; j++)
			cells[idx + j] = empty;

		idx += cols;
		offset = eol + 1LL;
	}

	int grid_size = rows * cols;
	for (int i = idx; i < grid_size; i++)
		cells[i] = empty;
}
