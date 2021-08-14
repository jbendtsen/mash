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

void Grid::render_into(Text *text, Cell *cells, Highlighter *syntax) {
	const Cell empty = {
		.background = syntax->colors[0]
	};

	int line = (int)(row_offset - text->lines_down);
	int idx = 0;

	int64_t offset = line > 0 ? text->newlines[line - 1] + 1LL : 0;
	offset += col_offset;

	for (int i = 0; i < rows && offset < text->total_size; i++) {
		int64_t eol = text->newlines[line + i];
		int line_len = (int)(eol - offset);

		int column = 0;
		for (column = 0; column < cols && column < line_len; column++) {
			char c = text->data[offset++];

			if (c < ' ' || c > '~')
				c = 0x7f;

			cells[idx + column] = {
				.glyph = (uint32_t)(c + 0xc0 - ' '),
				.modifier = 0,
				.foreground = syntax->colors[1],
				.background = syntax->colors[0]
			};
		}

		for (int j = column; j < cols; j++)
			cells[idx + j] = empty;

		idx += cols;
		offset = eol + 1LL + col_offset;
	}

	int grid_size = rows * cols;
	for (int i = idx; i < grid_size; i++)
		cells[i] = empty;
}
