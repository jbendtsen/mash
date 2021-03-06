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
	int n_mode_ranges;
	char required_mode_min[4]; // eg. 0, 2
	char required_mode_max[4]; // eg. 0, -1
	int mode_of;
	int mode_switch;

	int matches; // modified by update_highlighter()
};

struct String_Pool {
	static constexpr int CAPACITY_KERNEL = 32;

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
};

enum class TextStyle {
	Regular = 0,
	Bold,
	Italic,
	BoldItalic
};

enum class TextModifier {
	Normal = 0,
	Strikethrough,
	Underline
};

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

	int point_style_regular;
	int point_style_bold;
	int point_style_italic;
	int point_style_bi;

	int point_mod_normal;
	int point_mod_strikethrough;
	int point_mod_underline;
};

int64_t parse_syntax_config(u8 *buf, int size, Syntax_Mode *modes, int max_modes, Syntax_Token *tokens, int max_tokens, String_Pool& token_pool)
{
	const int64_t default_mode_min_chars = ((int64_t)'0' << 56) | ((int64_t)'A' << 48) | ((int64_t)'a' << 40) | ((int64_t)'_' << 32);
	const int64_t default_mode_max_chars = ((int64_t)'9' << 56) | ((int64_t)'Z' << 48) | ((int64_t)'z' << 40) | ((int64_t)'_' << 32);

	int mode_idx = 0;
	int token_idx = 0;

	bool is_comment = false;
	int param = 0;
	int pos = 0;
	int value_idx = 0;
	int mode_index = 0;
	auto kind = ConfigLine::None;

	Line_Params lp;
	memset(&lp, 0, sizeof(Line_Params));

	Syntax_Mode cur_mode = {0};
	Syntax_Token cur_token = {0};

	u8 c = buf[0];
	for (int i = 0; i < size; i++) {
		if (i == 0 || c == '\n') {
			is_comment = buf[i] == '#';
		}

		c = buf[i];
		if (is_comment) {
			if (c == '\n') is_comment = false;
			continue;
		}

		if (pos > 0 && (c == ' ' || c == '\t' || c == '\n')) {
			if (param == 1 && kind == ConfigLine::Token) {
				char *str = token_pool.add_string((char*)buf + i - pos, pos);

				bool was_esc = false;
				int j = 0, k = 0;
				for (j = 0; j < pos; j++) {
					char c = str[j];
					if (was_esc) {
						if (c == ' ') str[k++] = ' ';
						else if (c == 'n') str[k++] = '\n';
						else if (c == 't') str[k++] = '\t';
						else if (c == '\\') str[k++] = '\\';
					}
					else if (c != '\\') {
						str[k++] = c;
					}
					was_esc = c == '\\';
				}
				str[k] = 0;

				cur_token.str = str;
				cur_token.len = k;
			}

			if (param == 0) {
				if (pos == 4 && lp.point_mode == pos)
					kind = ConfigLine::Mode;
				else if (pos == 5 && lp.point_token == pos)
					kind = ConfigLine::Token;
			}
			else if (kind == ConfigLine::Mode) {
				if (lp.mode_param == ModeParam::Style) {
					if (pos ==  4 && lp.point_style_bold == pos)
						cur_mode.glyphset = static_cast<int>(TextStyle::Bold);
					else if (pos ==  6 && lp.point_style_italic == pos)
						cur_mode.glyphset = static_cast<int>(TextStyle::Italic);
					else if (pos ==  7 && lp.point_style_regular == pos)
						cur_mode.glyphset = static_cast<int>(TextStyle::Regular);
					else if (pos == 11 && lp.point_style_bi == pos)
						cur_mode.glyphset = static_cast<int>(TextStyle::BoldItalic);
				}
				else if (lp.mode_param == ModeParam::Modifier) {
					if (pos ==  6 && lp.point_mod_normal == pos)
						cur_mode.modifier = static_cast<int>(TextModifier::Normal);
					else if (pos ==  9 && lp.point_mod_underline == pos)
						cur_mode.modifier = static_cast<int>(TextModifier::Underline);
					else if (pos == 13 && lp.point_mod_strikethrough == pos)
						cur_mode.modifier = static_cast<int>(TextModifier::Strikethrough);
				}
			}
			else if (kind == ConfigLine::Token) {
				if (lp.token_param == TokenParam::PrevModeMin || lp.token_param == TokenParam::PrevModeMax) {
					cur_token.n_mode_ranges = value_idx + 1;
				}
			}

			if (c == '\n') {
				if (kind == ConfigLine::Mode && mode_idx < max_modes)
					modes[mode_idx++] = cur_mode;
				else if (kind == ConfigLine::Token && token_idx < max_tokens)
					tokens[token_idx++] = cur_token;

				memset(&cur_mode, 0, sizeof(Syntax_Mode));
				memset(&cur_token, 0, sizeof(Syntax_Token));

				param = 0;
				kind = ConfigLine::None;
				mode_index = 0;
			}
			else {
				param++;
			}

			memset(&lp, 0, sizeof(Line_Params));
			value_idx = 0;
			pos = 0;

			continue;
		}

		if (param == 0) {
			if (pos < 4 && c == "mode"[pos])
				lp.point_mode++;
			if (pos < 5 && c == "token"[pos])
				lp.point_token++;
		}
		else if (param == 1) {
			if (kind == ConfigLine::Mode) {
				if (c >= '0' && c <= '9') {
					mode_index *= 10;
					mode_index += c - '0';
				}
			}
			// token is handled later
		}
		else {
			if (kind == ConfigLine::Mode) {
				if (lp.mode_param == ModeParam::None) {
					if (pos < 3 && c == "min"[pos]) lp.point_min++;
					if (pos < 3 && c == "max"[pos]) lp.point_max++;
					if (pos < 4 && c == "back"[pos]) lp.point_back++;
					if (pos < 4 && c == "fore"[pos]) lp.point_fore++;
					if (pos < 5 && c == "style"[pos]) lp.point_style++;
					if (pos < 8 && c == "modifier"[pos]) lp.point_modifier++;

					if (c == '=') {
						if (pos == 3 && lp.point_min == pos) lp.mode_param = ModeParam::Min;
						else if (pos == 3 && lp.point_max == pos) lp.mode_param = ModeParam::Max;
						else if (pos == 4 && lp.point_back == pos) lp.mode_param = ModeParam::Back;
						else if (pos == 4 && lp.point_fore == pos) lp.mode_param = ModeParam::Fore;
						else if (pos == 5 && lp.point_style == pos) lp.mode_param = ModeParam::Style;
						else if (pos == 8 && lp.point_modifier == pos) lp.mode_param = ModeParam::Modifier;
						else lp.mode_param = ModeParam::Unknown;
					}
				}
				else if (lp.mode_param == ModeParam::Min) {
					int idx = pos - 4;
					if (idx >= 0 && idx < 8) {
						cur_mode.accepted_min[idx] = c;
					}
				}
				else if (lp.mode_param == ModeParam::Max) {
					int idx = pos - 4;
					if (idx >= 0 && idx < 8) {
						cur_mode.accepted_max[idx] = c;
					}
				}
				else if (lp.mode_param == ModeParam::Back) {
					if (c >= '0' && c <= '9') {
						cur_mode.back_color_idx *= 10;
						cur_mode.back_color_idx += c - '0';
					}
				}
				else if (lp.mode_param == ModeParam::Fore) {
					if (c >= '0' && c <= '9') {
						cur_mode.fore_color_idx *= 10;
						cur_mode.fore_color_idx += c - '0';
					}
				}
				else if (lp.mode_param == ModeParam::Style) {
					int idx = pos - 6;
					if (idx >= 0) {
						if (idx <  4 && c == "bold"[idx]) lp.point_style_bold++;
						if (idx <  6 && c == "italic"[idx]) lp.point_style_italic++;
						if (idx <  7 && c == "regular"[idx]) lp.point_style_regular++;
						if (idx < 11 && c == "bold-italic"[idx]) lp.point_style_bi++;
					}
				}
				else if (lp.mode_param == ModeParam::Modifier) {
					int idx = pos - 9;
					if (idx >= 0) {
						if (idx <  6 && c == "normal"[idx]) lp.point_mod_normal++;
						if (idx <  9 && c == "underline"[idx]) lp.point_mod_underline++;
						if (idx < 13 && c == "strikethrough"[idx]) lp.point_mod_strikethrough++;
					}
				}
			}
			else if (kind == ConfigLine::Token) {
				if (lp.token_param == TokenParam::None) {
					if (pos <  7 && c == "mode-of"[pos]) lp.point_mode_of++;
					if (pos < 11 && c == "mode-switch"[pos]) lp.point_mode_switch++;
					if (pos < 13 && c == "prev-mode-min"[pos]) lp.point_prev_mode_min++;
					if (pos < 13 && c == "prev-mode-max"[pos]) lp.point_prev_mode_max++;

					if (c == '=') {
						if (pos ==  7 && lp.point_mode_of == pos) lp.token_param = TokenParam::ModeOf;
						else if (pos == 11 && lp.point_mode_switch == pos) lp.token_param = TokenParam::ModeSwitch;
						else if (pos == 13 && lp.point_prev_mode_min == pos) lp.token_param = TokenParam::PrevModeMin;
						else if (pos == 13 && lp.point_prev_mode_max == pos) lp.token_param = TokenParam::PrevModeMax;
						else lp.token_param = TokenParam::Unknown;
					}
				}
				else if (lp.token_param == TokenParam::ModeOf) {
					if (c >= '0' && c <= '9') {
						cur_token.mode_of *= 10;
						cur_token.mode_of += c - '0';
					}
				}
				else if (lp.token_param == TokenParam::ModeSwitch) {
					if (c >= '0' && c <= '9') {
						cur_token.mode_switch *= 10;
						cur_token.mode_switch += c - '0';
					}
				}
				else if (lp.token_param == TokenParam::PrevModeMin || lp.token_param == TokenParam::PrevModeMax) {
					char *array = lp.token_param == TokenParam::PrevModeMin ? cur_token.required_mode_min : cur_token.required_mode_max;

					if (c == ',') {
						value_idx++;
					}
					else if (value_idx < 4) {
						int value = array[value_idx];
						if (value >= 0 && c >= '0' && c <= '9')
							value = (value * 10) + c - '0';
						else
							value = -1;

						array[value_idx] = value;
					}
				}
			}
		}

		pos++;
	}

	if (kind == ConfigLine::Mode && mode_idx < max_modes)
		modes[mode_idx++] = cur_mode;
	else if (kind == ConfigLine::Token && token_idx < max_tokens)
		tokens[token_idx++] = cur_token;

	return ((int64_t)mode_idx << 32L) | (int64_t)token_idx;
}

#define TEST_MAX_MODES 32
#define TEST_MAX_TOKENS 32

#include <unistd.h>

void splat(char c) {
	write(0, &c, 1);
}

void splat(const char *str) {
	write(0, str, strlen(str));
}

void splat(const char *str, int len) {
	write(0, str, len);
}

struct Mode_State {
	int mode_of;
	int mode_switch;
};

void emit_span(Mode_State& state, char *str, int len, Syntax_Mode *modes, int n_modes, Syntax_Token *tokens, int n_tokens) {
	bool should_change_mode = false;

	for (int j = 0; j < n_tokens; j++) {
		bool found = false;

		if (len == tokens[j].len) {
			found = true;
			for (int k = 0; k < len; k++) {
				if (str[k] != tokens[j].str[k]) {
					found = false;
					break;
				}
			}
		}

		if (found) {
			int n_ranges = tokens[j].n_mode_ranges;
			found = n_ranges == 0;

			for (int k = 0; k < 4 && k < n_ranges; k++) {
				int min = tokens[j].required_mode_min[k];
				int max = tokens[j].required_mode_max[k];
				if (min >= 0 && max >= 0 && state.mode_of >= min && state.mode_of <= max) {
					found = true;
					break;
				}
			}

			if (found) {
				if (tokens[j].mode_of >= 0 && tokens[j].mode_of < n_modes) {
					state.mode_of = tokens[j].mode_of;
				}
				if (tokens[j].mode_switch >= 0 && tokens[j].mode_switch < n_modes) {
					state.mode_switch = tokens[j].mode_switch;
					should_change_mode = true;
				}

				break;
			}
		}
	}

	Syntax_Mode *m = &modes[state.mode_of];

	char seq_buf[32];
	char *s = seq_buf;
	*s++ = '\x1b';
	*s++ = '[';
	*s++ = '0';

	if (m->glyphset == 1 || m->glyphset == 3) {
		*s++ = ';';
		*s++ = '1';
	}
	if (m->glyphset == 2 || m->glyphset == 3) {
		*s++ = ';';
		*s++ = '3';
	}
	if (m->modifier == 1) {
		*s++ = ';';
		*s++ = '9';
	}
	if (m->modifier == 2) {
		*s++ = ';';
		*s++ = '4';
	}

	if (m->fore_color_idx != 0) {
		*s++ = ';';
		*s++ = '3';
		*s++ = '0' + (m->fore_color_idx & 7);
	}
	if (m->back_color_idx != 0) {
		*s++ = ';';
		*s++ = '4';
		*s++ = '0' + (m->back_color_idx & 7);
	}

	*s++ = 'm';
	*s++ = 0;
	splat(seq_buf);

	splat(str, len);
	splat("\x1b[0m");

	if (should_change_mode)
		state.mode_of = state.mode_switch;
}

/* Here's like a comment
man
*/ /*yes*/ /* no */

void print_file_highlighted(Loaded_File& file, Syntax_Mode *modes, int n_modes, Syntax_Token *tokens, int n_tokens) {
	splat("\x1b[0m");

	Mode_State mode_state = {0};
	int start = -1;
	int mode = 0;
	int old_mode = 0;

	int n_new = 0;
	int n_still_good = 0;

	for (int i = 0; i < file.size; i++) {
		char c = file.buffer[i];
		bool good = false;
		Syntax_Mode *m = &modes[mode];

		for (int j = 0; j < 8; j++) {
			char min = m->accepted_min[j];
			char max = m->accepted_max[j];
			if (!min || !max)
				break;

			if (c >= min && c <= max) {
				good = true;
				break;
			}
		}

		// NOT GOOD
		if (!good) {
			if (start >= 0) {
				emit_span(mode_state, (char*)&file.buffer[start], i - start, modes, n_modes, tokens, n_tokens);
			}

			emit_span(mode_state, (char*)&file.buffer[i], 1, modes, n_modes, tokens, n_tokens);

			start = -1;
		}
		// YEAH GOOD
		else {
			if (start < 0) {
				start = i;
			}
		}
	}

	splat("\x1b[0m");
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

	auto modes = new Syntax_Mode[TEST_MAX_MODES];
	auto tokens = new Syntax_Token[TEST_MAX_TOKENS];
	String_Pool token_pool;

	int64_t stats = parse_syntax_config(syntax_file.buffer, syntax_file.size, modes, TEST_MAX_MODES, tokens, TEST_MAX_TOKENS, token_pool);
	int n_modes = (int)(stats >> 32);
	int n_tokens = (int)stats;

	print_file_highlighted(file, modes, n_modes, tokens, n_tokens);

	delete[] tokens;
	delete[] modes;

	return 0;
}
