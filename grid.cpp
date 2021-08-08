#include "mash.h"

void Text::enumerate_newlines() {
	if (!newlines) {
		nl_capacity = 1000;
		newlines = new int64_t[nl_capacity];
	}

	nl_size = 0;
	for (int64_t i = nl_start; i < total_size; i++) {
		if (data[i] != '\n')
			continue;

		newlines[nl_size++] = i;
		if (nl_size >= nl_capacity) {
			int64_t old_cap = nl_capacity;
			nl_capacity *= 2;
			int64_t *temp = new int64_t[nl_capacity];
			memcpy(temp, newlines, old_cap);
			delete[] newlines;
			newlines = temp;
		}
	}
}

void Grid::render_into(Text& text, Cell *cells, Highlighter& syntax) {
	const Cell empty = {
		.background = syntax.colors[0]
	};

	int line = (int)(row_offset - text.lines_down);
	int idx = 0;

	for (int i = 0; i < rows; i++) {
		int64_t offset = text.newlines[line + i] + 1LL;
		int col = 0;
		for (col = 0; col < cols; col++) {
			char c = text.data[offset++];
			if (c == '\n')
				break;

			if (c < ' ' || c > '~')
				c = 0x7f;

			cells[idx + col] = {
				.glyph = (uint32_t)(c - ' '),
				.modifier = 0,
				.background = syntax.colors[0],
				.foreground = syntax.colors[1]
			};
		}

		for (int j = col; j < cols; j++)
			cells[idx + j] = empty;

		idx += cols;
	}
}
