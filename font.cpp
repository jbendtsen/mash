#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_STROKER_H

typedef unsigned char u8;

#define N_CHARS 386
#define SLANT 0.2

struct Font_Render {
	u8 *buf;
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

#define FLOAT_FROM_16_16(n) ((float)((n) >> 16) + (float)((n) & 0xffff) / 65536.0)

static FT_Library library = nullptr;
static FT_Stroker stroker = nullptr;

static bool g_italic = false;

FT_Face load_font_face(const char *path) {
	if (!library) {
		if (FT_Init_FreeType(&library) != 0) {
			fprintf(stderr, "Failed to load freetype\n");
			return nullptr;
		}
	}

	FT_Face face;
	if (FT_New_Face(library, path, 0, &face) != 0) {
		fprintf(stderr, "Error loading font \"%s\"\n", path);
		FT_Done_FreeType(library);
		return nullptr;
	}

	FT_Stroker_New(library, &stroker);

	return face;
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
void render_ascii(Font_Render& render, FT_Face face, int start_idx) {
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

		if (x < 0 || y < 0 || x + w > full_glyph_w || y + h > render.glyph_h) {
			printf("%s: '%c' doesn't fit: glyph=(%d,%d,%dx%d), frame=%dx%d\n", get_font_type_name(false), c, x, y, w, h, full_glyph_w, render.glyph_h);
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

Font_Render size_up_font_render(FT_Face face, float size, float dpi_w, float dpi_h) {
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

	if (glyph_w <= 0)
		return {};

	Font_Render render = {
		.dpi_w = dpi_w,
		.dpi_h = dpi_h,
		.points = pts,
		.baseline = (glyph_h * 3) / 4,
		.overlap_w = overlap_w,
		.glyph_w = glyph_w,
		.glyph_h = glyph_h
	};

	render.total_size = N_CHARS * render.get_full_glyph_width() * glyph_h;
	return render;
}

void make_font_render(FT_Face face, Font_Render& render) {
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

	int bar_h = 1 + (render.glyph_h / 15);
	int bar_bottom = render.glyph_h - bar_h;
	int bar_middle = bar_bottom / 2;

	int underline_offset     = (N_CHARS - 2) * full_glyph_w * render.glyph_h;
	int strikethrough_offset = (N_CHARS - 1) * full_glyph_w * render.glyph_h;

	memset(render.buf + underline_offset + (bar_bottom * full_glyph_w), 0xff, bar_h * full_glyph_w);
	memset(render.buf + strikethrough_offset + (bar_middle * full_glyph_w), 0xff, bar_h * full_glyph_w);
}
/*
void destroy_font_face(Font_Face face) {
	FT_Done_Face((FT_Face)face);
}
*/
void ft_quit() {
	FT_Stroker_Done(stroker);
	FT_Done_FreeType(library);
}

int main(int argc, char **argv) {
	if (argc != 2)
		return 1;

	FT_Face face = load_font_face(argv[1]);
	if (!face)
		return 2;

	Font_Render render = size_up_font_render(face, 32, 96, 96);
	if (render.total_size) {
		render.buf = new u8[render.total_size]();
		make_font_render(face, render);
	}

	FT_Done_Face(face);
	ft_quit();

	if (!render.total_size)
		return 3;

	FILE *f = fopen("debug.bin", "wb");
	fwrite(render.buf, 1, render.total_size, f);
	fclose(f);

	int full_glyph_w = render.get_full_glyph_width();
	printf("glyph slot: %dx%d\n", full_glyph_w, render.glyph_h);

	int size = 384 * full_glyph_w * render.glyph_h;
	int width = full_glyph_w * 24;
	u8 *buf = new u8[size];

	for (int i = 0; i < size / full_glyph_w; i++) {
		int col = i % 24;
		int row = i / 24;
		int inner_row = row % render.glyph_h;
		int outer_row = row / render.glyph_h;

		int gl = col + outer_row * 24;
		int idx = (gl * render.glyph_h + inner_row) * full_glyph_w;
		memcpy(buf + i*full_glyph_w, render.buf + idx, full_glyph_w);
	}

	f = fopen("debug_frand.bin", "wb");
	fwrite(buf, 1, size, f);
	fclose(f);

	delete[] buf;
	delete[] render.buf;
	return 0;
}
