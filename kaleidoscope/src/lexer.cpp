#include <cctype>
#include <cstdio>
#include <string>

enum token {
	tok_eof = -1,

	// commands
	tok_def = -2,
	tok_extern = -3,
	
	// primary
	tok_identifier = -4,
	tok_number = -5,
};

class Lexer {
	public: 
		std::string identifier_str;
		double num_val;
		int last_char = ' ';

		int gettok() {

			while (isspace(last_char)) {
				last_char = getchar();
			}

			if (isalpha(last_char)) {
				identifier_str = last_char;
				while (isalnum((last_char = getchar()))) {
					identifier_str += last_char;
				}

				if (identifier_str == "def") 
					return token::tok_def;
				if (identifier_str == "extern")
					return token::tok_extern;
				return token::tok_identifier;
			}

			if (isdigit(last_char) || last_char == '.') {
				std::string num_str;
				bool has_decimal_point = (last_char == '.') ? true : false;
				do {
					num_str += last_char;
					last_char = getchar();
				} while (isdigit(last_char) || (!has_decimal_point && last_char == '.'));

				num_val = strtod(num_str.c_str(), 0);
				return token::tok_number;
			}

			if (last_char == '#') {
				do {
					last_char = getchar();
				} while (last_char != EOF && last_char != '\n' && last_char != '\r');

				if (last_char != EOF) 
					return gettok();
			}

			if (last_char == EOF) 
				return token::tok_eof;

			// returning character as its ascii value
			int ascii_char = last_char;
			last_char = getchar();
			return ascii_char;
		}
};
