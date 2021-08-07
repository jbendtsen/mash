#pragma once

typedef void* Font_Handle;

// Note: it is currently the responsibility of the application to manage Font_Render::buf
struct Font_Render {
	unsigned char *buf;
	float dpi_w;
	float dpi_h;
	int points;
	int baseline;
	int overlap_w;
	int glyph_w;
	int glyph_h;
	int total_size;

	int get_full_glyph_width() {
		return glyph_w + 2*overlap_w;
	}
};

Font_Handle load_font_face(const char *path);
Font_Render size_up_font_render(Font_Handle font_face, float size, float dpi_w, float dpi_h);
void make_font_render(Font_Handle font_face, Font_Render render);
void ft_quit(void);
