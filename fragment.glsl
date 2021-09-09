#version 450

const uint N_GLYPHS = 384;

struct Cell {
	uint glyph;
	uint modifier;
	uint foreground;
	uint background;
};

layout (binding = 0) buffer readonly restrict GRID {
	Cell grid[];
};

layout (binding = 1) buffer readonly restrict GLYPH_BUFFER {
	uint glyph_buffer[];
};

layout (push_constant) uniform PARAMS {
	uvec2 view_origin;
	uvec2 view_size;
	uvec2 cell_size;
	ivec2 cursor;
	uint cursor_color;
	uint columns;
	uint grid_cell_offset;     // offset in cells (16 or so bytes)
	uint glyphset_byte_offset; // offset in bytes
	uint glyph_overlap_w;
	uint glyph_full_w;
} params;

layout (location = 0) out vec4 outColor;

float get_glyph_value(uint slot, uint pos, uint full_cell_w) {
	uint idx = params.glyphset_byte_offset + (slot * full_cell_w * params.cell_size.y) + pos;

	uint shift = (idx & 3) * 8;
	uint value = (glyph_buffer[idx / 4] >> shift) & 0xff;

	return float(value) / 255.0;
}

vec3 get_color(uint c) {
	return vec3((c >> 24) & 0xff, (c >> 16) & 0xff, (c >> 8) & 0xff) / 255.0;
}

void main() {
	uvec2 view_pos = uvec2(gl_FragCoord.xy) - params.view_origin;

	uint cell_w = params.cell_size.x;
	uint full_cell_w = params.glyph_full_w;

	uint outer_row = view_pos.y / params.cell_size.y;
	uint inner_row = view_pos.y % params.cell_size.y;

	uint outer_col = view_pos.x / cell_w;
	uint inner_col = view_pos.x % cell_w;

	uint bar_h   = 1 + (params.cell_size.y / 20);
	uint bar_mid = (params.cell_size.y - bar_h) / 2;

	uint row_start_cell = params.grid_cell_offset + outer_row * params.columns;

	uint cell_idx = row_start_cell + outer_col;
	uint modifier = grid[cell_idx].modifier;
	uint top = modifier * bar_mid;

	vec3 back = get_color(grid[cell_idx].background);
	vec3 fore_cur = get_color(grid[cell_idx].foreground);
	vec3 fore = fore_cur;
	float lum = 0.0;

	if (outer_row == params.cursor.y && outer_col == params.cursor.x && inner_col < params.glyph_overlap_w) {
		lum = 1.0;
		fore = get_color(params.cursor_color);
	}
	else if (modifier > 0 && inner_row >= top && inner_row < top + bar_h) {
		lum = 1.0;
	}
	else {
		uint glyph_pixel_offset = inner_row * full_cell_w;
		uint next_threshold = full_cell_w - params.glyph_overlap_w;

		uint pos = glyph_pixel_offset + inner_col + params.glyph_overlap_w;
		float value_cur = get_glyph_value(grid[cell_idx].glyph, pos, full_cell_w);
		lum = value_cur;

		if (outer_col > 0 && inner_col <= params.glyph_overlap_w) {
			uint prev_pos = glyph_pixel_offset + cell_w + params.glyph_overlap_w + inner_col;
			uint prev_idx = cell_idx - 1;
			float value_prev = get_glyph_value(grid[prev_idx].glyph, prev_pos, full_cell_w);

			float value = value_cur + value_prev;
			if (value > 0.0) {
				lum = clamp(value, 0.0, 1.0);
				fore = mix(fore_cur, get_color(grid[prev_idx].foreground), value_prev / value);
			}
		}
	}

	outColor = vec4(mix(back, fore, lum), 1.0);
}
