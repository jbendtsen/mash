#include "font.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_STROKER_H

#include <vector>

typedef unsigned char u8;

#define N_GLYPHS 384
#define SLANT 0.2

#define FLOAT_FROM_16_16(n) ((float)((n) >> 16) + (float)((n) & 0xffff) / 65536.0)

static FT_Library library = nullptr;
static FT_Stroker stroker = nullptr;

static std::vector<FT_Face> font_faces;

static bool g_italic = false;

Font_Handle load_font_face(const char *path) {
	if (!library) {
		if (FT_Init_FreeType(&library) != 0) {
			fprintf(stderr, "Failed to load freetype\n");
			return nullptr;
		}
	}

	if (!stroker)
		FT_Stroker_New(library, &stroker);

	FT_Face face;
	if (FT_New_Face(library, path, 0, &face) != 0) {
		fprintf(stderr, "Error loading font \"%s\"\n", path);
		return nullptr;
	}

	font_faces.push_back(face);

	return (Font_Handle)face;
}

const char *get_font_type_name(bool bold) {
	if (g_italic) {
		if (bold)
			return "bold-italic";
		else
			return "italic";
	}
	else if (bold)
		return "bold";

	return "regular";
}

template<bool bold>
void render_ascii(Font_Render render, FT_Face face, int start_idx) {
	int full_glyph_w = render.glyph_w + 2*render.overlap_w;

	for (int c = 0x20; c <= 0x7e; c++) {
		FT_Bitmap bmp;
		int left;
		int top;

		if constexpr (bold) {
			FT_Load_Char(face, c, FT_LOAD_NO_BITMAP);

			FT_Glyph glyph;
			FT_Get_Glyph(face->glyph, &glyph);

			FT_Glyph_StrokeBorder(&glyph, stroker, 0, 1);
			FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, 1);
			FT_BitmapGlyph bg = (FT_BitmapGlyph)glyph;

			bmp = bg->bitmap;
			left = bg->left;
			top = bg->top;
		}
		else {
			FT_Load_Char(face, c, FT_LOAD_RENDER);

			bmp = face->glyph->bitmap;
			left = face->glyph->bitmap_left;
			top = face->glyph->bitmap_top;
		}

		int w = (int)bmp.width;
		int h = (int)bmp.rows;
		int x = render.overlap_w + left;
		int y = render.baseline - top;

		// HACK
		if (x < 0) x = 0;

		if (x < 0 || y < 0 || x + w > full_glyph_w || y + h > render.glyph_h) {
			fprintf(stderr, "%s: '%c' doesn't fit: glyph=(%d,%d,%dx%d), frame=%dx%d\n", get_font_type_name(false), c, x, y, w, h, full_glyph_w, render.glyph_h);
			continue;
		}

		int idx = start_idx + c - 0x20;
		int offset = (idx * render.glyph_h + y) * full_glyph_w;

		for (int i = 0; i < h; i++) {
			u8 *in = &bmp.buffer[i*w];
			u8 *out = &render.buf[offset + x];
			memcpy(out, in, w);
			offset += full_glyph_w;
		}
	}
}

Font_Render size_up_font_render(Font_Handle fh, float size, float dpi_w, float dpi_h) {
	auto face = (FT_Face)fh;

	float req_size = size * 64.0;
	int pts = (int)(req_size + 0.5);

	FT_Set_Char_Size(face, 0, pts, dpi_w, dpi_h);

	float pt_w = size * dpi_w / 72.0;
	float pt_h = size * dpi_h / 72.0;

	float em_units = (float)(face->units_per_EM ? face->units_per_EM : 1);
	float glyph_w_f = pt_w * (float)face->max_advance_width / em_units;
	float glyph_h_f = pt_h * (float)face->max_advance_height / em_units;

	int overlap_w = 1 + (int)(glyph_w_f / 16.0);
	int glyph_w = (int)glyph_w_f;
	int glyph_h = (int)glyph_h_f;

	Font_Render render = {
		.dpi_w = dpi_w,
		.dpi_h = dpi_h,
		.points = pts,
		.baseline = (glyph_h * 3) / 4,
		.overlap_w = overlap_w,
		.glyph_w = glyph_w,
		.glyph_h = glyph_h
	};

	render.total_size = N_GLYPHS * render.get_full_glyph_width() * glyph_h;
	return render;
}

void make_font_render(Font_Handle fh, Font_Render render) {
	auto face = (FT_Face)fh;

	FT_Set_Char_Size(face, 0, render.points, render.dpi_w, render.dpi_h);
	FT_Stroker_Set(stroker, render.points / 20, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

	render_ascii<false>(render, face, 0);
	render_ascii<true>(render, face, 96);

	FT_Matrix matrix = {
		.xx = 0x10000,
		.xy = (int)(SLANT * 0x10000),
		.yx = 0,
		.yy = 0x10000
	};
	FT_Set_Transform(face, &matrix, nullptr);
	g_italic = true;

	render_ascii<false>(render, face, 192);
	render_ascii<true>(render, face, 288);

	int full_glyph_w = render.get_full_glyph_width();
/*
	int bar_h = 1 + (render.glyph_h / 15);
	int bar_bottom = render.glyph_h - bar_h;
	int bar_middle = bar_bottom / 2;

	int underline_offset     = (N_GLYPHS - 2) * full_glyph_w * render.glyph_h;
	int strikethrough_offset = (N_GLYPHS - 1) * full_glyph_w * render.glyph_h;

	memset(render.buf + underline_offset + (bar_bottom * full_glyph_w), 0xff, bar_h * full_glyph_w);
	memset(render.buf + strikethrough_offset + (bar_middle * full_glyph_w), 0xff, bar_h * full_glyph_w);
*/
}

void ft_quit() {
	for (auto& face : font_faces)
		FT_Done_Face(face);

	FT_Stroker_Done(stroker);
	FT_Done_FreeType(library);
}
