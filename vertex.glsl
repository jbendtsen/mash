#version 450

/*
struct Params {
	uvec2 view_origin;
	uvec2 view_size;
	uvec2 cell_size;
	uint grid_cell_offset;
	uint glyphset_byte_offset;
};

layout (binding = 0) uniform PARAMS_LIST {
	Params params_list[];
};

layout (location = 0) in SCREEN {
	vec2 screen_size_f;
};

layout (location = 0) out PARAMS {
	Params cur_params;
};
*/

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
/*
	cur_params = params_list[gl_InstanceID];

	vec4 view = vec4(cur_params.view_origin, cur_params.view_size);
	view.xy /= 0.5 * screen_size_f.x;
	view.zw /= 0.5 * screen_size_f.y;
	view -= 1.0;
	view.yw *= -1.0;

	vec2 point = view.xy;
	point += view.zw * vec2(float(gl_VertexID & 1), float((gl_VertexID & 2) >> 1));
*/
	vec2 point = vec2(float(gl_VertexIndex & 1) - 0.5, 0.5 - float(gl_VertexIndex >> 1));
	point *= 2.0;

	gl_Position = vec4(point, 0.0, 1.0);
}
