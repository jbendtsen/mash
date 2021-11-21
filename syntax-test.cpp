#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

typedef unsigned char u8;

struct Syntax_Mode {
	char accepted_min[8]; // eg. "0Aa_"
	char accepted_max[8]; // eg. "9Zz_"
	int fore_color_idx;
	int back_color_idx;
	int glyphset; // 0 = regular, 1 = bold, 2 = italic, 3 = bold italic
	int modifier; // 0 = normal, 1 = strikethrough, 2 = underline
};

struct Syntax_Token {
	char *str;
	int len;
	char required_mode_min[4]; // eg. 0, 2
	char required_mode_max[4]; // eg. 0, -1
	int mode_of;
	int mode_switch;

	int matches; // modified by update_highlighter()
};

struct String_Pool {
	static constexpr CAPACITY_KERNEL = 32;

	char *buffer;
	int capacity;
	int size;

	String_Pool() : buffer(nullptr), capacity(0), size(0) {}
	~String_Pool() { if (buffer) delete[] buffer; }

	void resize(int sz) {
		int new_cap = CAPACITY_KERNEL;
		while (new_cap < sz)
			new_cap *= 2;

		if (new_cap > capacity) {
			char *new_buf = new char[new_cap];

			if (buffer) {
				if (size > 0) memcpy(new_buf, buffer, size);
				delete[] buffer;
			}
	
			buffer = new_buf;
			capacity = new_cap;
		}

		if (sz >= 0)
			size = sz;
	}

	char *add_string(const char *str, int len) {
		int add_len = len + 1;
		int head = size;
		resize(head + add_len);

		memcpy(&buffer[head], str, len);
		buffer[head + len] = 0;

		return &buffer[head];
	}
};

struct Loaded_File {
	u8 *buffer;
	int size;

	Loaded_File() : buffer(nullptr), size(0) {}
	~Loaded_File() { if (buffer) delete[] buffer; }

	bool load(char *name) {
		FILE *f = fopen(name, "rb");
		if (!f) {
			fprintf(stderr, "Could not open file \"%s\"\n", name);
			return false;
		}

		fseek(f, 0, SEEK_END);
		int sz = ftell(f);
		rewind(f);

		if (sz < 1) {
			fprintf(stderr, "File \"%s\" not readable\n", name);
			return false;
		}

		size = sz;
		buffer = new u8[size];
		fread(buffer, 1, size, f);
		fclose(f);
		return true;
	}
}

enum class TextStyle {
	Regular = 0,
	Bold,
	Italic,
	BoldItalic
}

enum class TextModifier {
	Normal = 0,
	Strikethrough,
	Underline
}

enum class ConfigLine {
	None = 0,
	Mode,
	Token
};

enum class ModeParam {
	None = 0,
	Unknown,
	Min,
	Max,
	Back,
	Fore,
	Style,
	Modifier,
};

enum class TokenParam {
	None = 0,
	Unknown,
	ModeOf,
	ModeSwitch,
	PrevModeMin,
	PrevModeMax
};

struct Line_Params {
	ModeParam mode_param;
	int point_mode;
	int point_token;
	int point_min;
	int point_max;
	int point_back;
	int point_fore;
	int point_style;
	int point_modifier;

	TokenParam token_param;
	int point_mode_of;
	int point_mode_switch;
	int point_prev_mode_min;
	int point_prev_mode_max;
};

int parse_syntax_config(u8 *buf, int size, std::vector<Syntax_Mode>& modes, std::vector<Syntax_Token>& tokens, String_Pool& token_pool)
{
	const int64_t default_mode_min_chars = ((int64_t)'0' << 56) | ((int64_t)'A' << 48) | ((int64_t)'a' << 40) | ((int64_t)'_' << 32);
	const int64_t default_mode_max_chars = ((int64_t)'9' << 56) | ((int64_t)'Z' << 48) | ((int64_t)'z' << 40) | ((int64_t)'_' << 32);

	int param = 0;
	int pos = 0;
	int value_idx = 0;
	int mode_index = 0;
	ConfigLine kind = None;

	Line_Params lp;
	memset(&lp, 0, sizeof(Line_Params));

	Syntax_Mode cur_mode = {0};
	Syntax_Token cur_token = {0};

	for (int i = 0; i < size; i++) {
		u8 c = buf[i];
		if (param == 0) {
			if (pos < 4 && c == "mode"[pos])
				point_mode++;
			if (pos < 5 && c == "token"[pos])
				point_token++;
		}
		else if (param == 1) {
			if (kind == Mode) {
				if (c >= '0' && c <= '9') {
					mode_index *= 10;
					mode_index += c - '0';
				}
			}
			// token is handled later
		}
		else {
			if (kind == Mode) {
				if (lp.mode_param == None) {
					if (pos < 3 && c == "min"[pos]) point_min++;
					if (pos < 3 && c == "max"[pos]) point_max++;
					if (pos < 4 && c == "back"[pos]) point_back++;
					if (pos < 4 && c == "fore"[pos]) point_fore++;
					if (pos < 5 && c == "style"[pos]) point_style++;
					if (pos < 8 && c == "modifier"[pos]) point_modifier++;

					if (c == '=') {
						if (pos == 3 && point_min == pos) mode_param = Min;
						else if (pos == 3 && point_max == pos) mode_param = Max;
						else if (pos == 4 && point_back == pos) mode_param = Back;
						else if (pos == 4 && point_fore == pos) mode_param = Fore;
						else if (pos == 5 && point_style == pos) mode_param = Style;
						else if (pos == 8 && point_modifier == pos) mode_param = Modifier;
						else mode_param = Unknown;
					}
				}
				else if (lp.mode_param == Min) {
					if (pos < 8)
						cur_mode.accepted_min[pos] = c;
				}
				else if (lp.mode_param == Max) {
					if (pos < 8)
						cur_mode.accepted_max[pos] = c;
				}
				else if (lp.mode_param == Back) {
					if (c >= '0' && c <= '9') {
						cur_mode.back_color_idx *= 10;
						cur_mode.back_color_idx += c - '0';
					}
				}
				else if (lp.mode_param == Fore) {
					if (c >= '0' && c <= '9') {
						cur_mode.fore_color_idx *= 10;
						cur_mode.fore_color_idx += c - '0';
					}
				}
				else if (lp.mode_param == Style) {
					if (pos <  4 && c == "bold"[pos]) point_style_bold++;
					if (pos <  6 && c == "italic"[pos]) point_style_italic++;
					if (pos <  7 && c == "regular"[pos]) point_style_regular++;
					if (pos < 11 && c == "bold-italic"[pos]) point_style_bi++;
				}
				else if (lp.mode_param == Modifier) {
					if (pos <  6 && c == "normal"[pos]) point_mod_normal++;
					if (pos <  9 && c == "underline"[pos]) point_mod_underline++;
					if (pos < 13 && c == "strikethrough"[pos]) point_mod_strikethrough++;
				}
			}
			else if (kind == Token) {
				if (lp.token_param == None) {
					if (pos <  7 && c == "mode-of"[pos]) point_mode_of++;
					if (pos < 11 && c == "mode-switch"[pos]) point_mode_switch++;
					if (pos < 13 && c == "prev-mode-min"[pos]) point_prev_mode_min++;
					if (pos < 13 && c == "prev-mode-max"[pos]) point_prev_mode_max++;

					if (c == '=') {
						if (pos ==  7 && point_mode_of == pos) token_param = ModeOf;
						else if (pos == 11 && point_mode_switch == pos) token_param = ModeSwitch;
						else if (pos == 13 && point_prev_mode_min == pos) token_param = PrevModeMin;
						else if (pos == 13 && point_prev_mode_max == pos) token_param = PrevModeMax;
						else token_param = Unknown;
					}
				}
				else if (lp.token_param == ModeOf) {
					if (c >= '0' && c <= '9') {
						cur_token.mode_of *= 10;
						cur_token.mode_of += c - '0';
					}
				}
				else if (lp.token_param == ModeSwitch) {
					if (c >= '0' && c <= '9') {
						cur_token.mode_switch *= 10;
						cur_token.mode_switch += c - '0';
					}
				}
				else if (lp.token_param == PrevModeMin || lp.token_param == PrevModeMax) {
					u8 *array = lp.token_param == PrevModeMin ? cur_token.required_mode_min : cur_token.required_mode_max;

					if (c == ',') {
						value_idx++;
					}
					else if (value_idx < 4) {
						int value = array[value_idx] & 0xff;
						if (value >= 0 && c >= '0' && c <= '9')
							value = (value * 10) + c - '0';
						else
							value = -1;

						array[value_idx] = value;
					}
				}
			}
		}

		if (pos > 0 && (c == ' ' || c == '\t' || c == '\n')) {
			if (param == 1 && kind == Token) {
				char *str = token_pool.add_string((char*)buf + i - pos, pos);
				cur_token.str = str;
				cur_token.len = pos;
			}

			if (param == 0) {
				if (pos == 3 && point_mode == pos+1)
					kind = Mode;
				else if (pos == 4 && point_token == pos+1)
					kind = Token;
			}
			else if (kind == Mode) {
				if (lp.mode_param == Style) {
					if (pos ==  4 && point_style_bold == pos)
						cur_mode.glyphset = Bold;
					else if (pos ==  6 && point_style_italic == pos)
						cur_mode.glyphset = Italic;
					else if (pos ==  7 && point_style_regular == pos)
						cur_mode.glyphset = Regular;
					else if (pos == 11 && point_style_bi == pos)
						cur_mode.glyphset = BoldItalic;
				}
				else if (lp.mode_param == Modifier) {
					if (pos ==  6 && point_mod_normal == pos)
						cur_mode.modifier = Normal;
					else if (pos ==  9 && point_mod_underline == pos)
						cur_mode.modifier = Underline;
					else if (pos == 13 && point_mod_strikethrough == pos)
						cur_mode.modifier = Strikethrough;
				}
			}

			if (c == '\n') {
				if (kind == Mode)
					modes.push_back(cur_mode);
				else if (kind == Token)
					tokens.push_back(cur_token);

				param = 0;
				kind = None;
				mode_index = 0;
				memset(&lp, 0, sizeof(Line_Params));
			}
			else
				param++;

			pos = 0;
			value_idx = 0;
		}
		else
			pos++;
	}

	if (kind == Mode)
		modes.push_back(cur_mode);
	else if (kind == Token)
		tokens.push_back(cur_token);
}

int main(int argc, char **argv) {
	if (argc != 3) {
		printf("Syntax Highlighter\n"
			"Usage: %s <file to print> <syntax config file> [extra arg for no output]\n", argv[0]);
		return 1;
	}

	Loaded_File file, syntax_file;
	if (!file.load(argv[1]) || !syntax_file.load(argv[2]))
		return 2;

	std::vector<Syntax_Mode> modes;
	std::vector<Syntax_Token> tokens;
	String_Pool token_pool;

	parse_syntax_config(syntax_file.buffer, syntax_file.size, modes, tokens, token_pool);

	for (int i = 0; i < file.size; i++) {
		char c = file.buffer[i];
		
	}
}
