#pragma once

#include "llvm/ADT/StringRef.h"

#include <cstdlib>
#include <memory>
#include <string>

namespace toy {

struct Location {
    std::shared_ptr<std::string> file;
    int line;
    int col;
};

enum Token : int {
    tok_semicolon = ';',
    tok_parenthese_open = '(',
    tok_parenthese_close = ')',
    tok_bracket_open = '{',
    tok_bracket_close = '}',
    tok_sbracket_open = '[',
    tok_sbracket_close = ']',

    tok_eof = -1,

    // commands
    tok_return = -2,
    tok_var = -3,
    tok_def = -4,

    // primary
    tok_identifier = -5,
    tok_number = -6,
};

class Lexer {
    public:
        Lexer(std::string filename)
            : last_location(
                    {std::make_shared<std::string>(std::move(filename)), 0, 0}) {}
        virtual ~Lexer() = default;

        Token get_cur_token() { return cur_tok; }

        Token get_next_token() { return cur_tok = get_tok(); }

        void consume(Token tok) {
            assert(tok == cur_tok && "consume Token mismatch expectation");
            get_next_token();
        }

        llvm::StringRef get_id() {
            assert(cur_tok == tok_identifier);
            return identifier_str;
        }

        double get_value() {
            assert(cur_tok == tok_number);
            return num_val;
        }

        Location get_last_location() { return last_location; }

        int get_line() { return cur_line_num; }

        int get_col() { return cur_col; }

    private:
        virtual llvm::StringRef readNextLine() = 0;

        int get_next_char() {
            if (cur_line_buffer.empty()) return EOF;
            ++cur_col;
            auto nextchar = cur_line_buffer.front();
            cur_line_buffer = cur_line_buffer.drop_front();
            if (cur_line_buffer.empty())
            cur_line_buffer = readNextLine();
            if (nextchar == '\n') {
            ++cur_line_num;
            cur_col = 0;
            }
            return nextchar;
        }

        Token get_tok() {
            while (isspace(last_char))
            last_char = Token(get_next_char());

            // Save the current location before reading the token characters.
            last_location.line = cur_line_num;
            last_location.col = cur_col;

            // Identifier: [a-zA-Z][a-zA-Z0-9_]*
            if (isalpha(last_char)) {
            identifier_str = (char)last_char;
            while (isalnum((last_char = Token(get_next_char()))) || last_char == '_')
                identifier_str += (char)last_char;

            if (identifier_str == "return")
                return tok_return;
            if (identifier_str == "def")
                return tok_def;
            if (identifier_str == "var")
                return tok_var;
            return tok_identifier;
            }

            // Number: [0-9.]+
            if (isdigit(last_char) || last_char == '.') {
            std::string numStr;
            do {
                numStr += last_char;
                last_char = Token(get_next_char());
            } while (isdigit(last_char) || last_char == '.');

            num_val = strtod(numStr.c_str(), nullptr);
            return tok_number;
            }

            if (last_char == '#') {
            do {
                last_char = Token(get_next_char());
            } while (last_char != EOF && last_char != '\n' && last_char != '\r');

            if (last_char != EOF)
                return get_tok();
            }

            // Check for end of file.  Don't eat the EOF.
            if (last_char == EOF)
            return tok_eof;

            Token thisChar = Token(last_char);
            last_char = Token(get_next_char());
            return thisChar;
        }

        Token cur_tok = tok_eof;
        Location last_location;
        std::string identifier_str;
        double num_val = 0;
        Token last_char = Token(' ');
        int cur_line_num = 0;
        int cur_col = 0;
        llvm::StringRef cur_line_buffer = "\n";
};

class LexerBuffer final : public Lexer {
    public:
        LexerBuffer(const char *begin, const char *end, std::string filename)
            : Lexer(std::move(filename)), current(begin), end(end) {}

    private:
        llvm::StringRef readNextLine() override {
            auto *begin = current;
            while (current <= end && *current && *current != '\n')
            ++current;
            if (current <= end && *current)
            ++current;
            llvm::StringRef result{begin, static_cast<size_t>(current - begin)};
            return result;
        }
        const char *current, *end;
};

} // end 'namespace toy'