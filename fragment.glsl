#version 450

const uint N_CHAR_GLYPHS = 384;

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
	uint grid_cell_offset;
	uint glyphset_byte_offset;
	uint glyph_overlap_w;
} params;

layout (location = 0) out vec4 outColor;

float get_glyph_value(uint slot, uvec2 pos) {
	uint idx = params.glyphset_byte_offset + (slot * params.cell_size.x * params.cell_size.y) + pos;

	uint shift = (3 - (idx & 3)) * 8;
	uint value = (glyph_buffer[idx / 4] >> shift) & 0xff;

	return float(value) / 255.0;
}

vec3 get_color(uint c) {
	return vec3((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff) / 255.0;
}

void main() {
	uvec2 view_pos = uvec2(gl_FragCoord.xy) - params.view_origin;

	uint cell_w = params.cell_size.x;
	uvec2 line = uvec2(view_pos.y / params.cell_size.y, view_pos.y % params.cell_size.y);
	// it makes sense later trust me
	row[0] = params.grid_cell_offset + row[0] * params.view_size.x;
	row[1] *= cell_w;

	uint x_left  = view_pos.x - params.glyph_overlap_w;
	uint x_right = view_pos.x + params.glyph_overlap_w;

	uvec2 left  = uvec2(x_left / cell_w, x_left % cell_w);
	uvec2 right = uvec2(x_right / cell_w, x_right % cell_w);

	float value_right = 0.0;
	float value_left = 0.0;

	{
		uint cell_idx = row[0] + right[0];
		uint pos = row[1] + right[1];

		value_right = get_glyph_value(grid[cell_idx].glyph, pos);

		uint modifier = grid[cell_idx].modifier;
		if (modifier != 0)
			value_right += get_glyph_value(N_CHAR_GLYPHS - 1 + modifier, pos);
	}

	if (left[0] >= 0 && left[0] != right[0]) {
		uint cell_idx = row[0] + left[0];
		uint pos = row[1] + left[1];

		value_left = get_glyph_value(grid[cell_idx].glyph, pos);
	}

	vec3 back = get_color(grid[cell].background);
	vec3 fore = get_color(grid[cell].foreground);

	float lum = clamp(value[0] + value[1], 0.0, 1.0);
	outColor = vec4(mix(back, fore, lum), 1.0);
}
