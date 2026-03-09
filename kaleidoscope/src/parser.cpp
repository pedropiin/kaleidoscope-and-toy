#include <string>
#include <vector>
#include <unordered_map>

#include "lexer.cpp"
#include "include/kaleidoscope/ast.hpp"
#include "include/kaleidoscope/error.hpp"

class Parser {
	public:
		int curr_tok;
		Lexer lexer;

		std::unique_ptr<FunctionAST> parse_definition() {
			get_next_token();
			std::unique_ptr<PrototypeAST> prototype = parse_prototype();
			if (!prototype) 
				return nullptr;

			std::unique_ptr<ExprAST> expr = parse_expression();
			if (expr)
				return std::make_unique<FunctionAST>(std::move(prototype), std::move(expr));
			
			return nullptr;
		}

		std::unique_ptr<PrototypeAST> parse_extern() {
			get_next_token();
			return parse_prototype();
		}

		std::unique_ptr<FunctionAST> parse_top_level_expr() {
			std::unique_ptr<ExprAST> expr = parse_expression();
			if (expr) {
				auto prototype = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
				return std::make_unique<FunctionAST>(std::move(prototype), std::move(expr));
			}
			return nullptr;
		}

		int get_next_token() {
			return curr_tok = lexer.gettok();
		}

	private:
		const std::unordered_map<char, int> m_binary_op_precedence = {
			{'<', 10},
			{'>', 10},

			{'+', 20},
			{'-', 20},

			{'*', 40},
			{'/', 40},
		};

		std::unique_ptr<PrototypeAST> parse_prototype() {
			if (curr_tok != tok_identifier) 
				return log_error_proto("Expected function name in prototype.");

			std::string func_name = lexer.identifier_str;
			get_next_token();

			if (curr_tok != '(')
				return log_error_proto("Expected '(' in prototype.");

			std::vector<std::string> arg_names;
			while (get_next_token() == tok_identifier) 
				arg_names.push_back(lexer.identifier_str);
			if (curr_tok != ')')
				return log_error_proto("Expected ')' in prototype.");

			get_next_token();
			
			return std::make_unique<PrototypeAST>(func_name, std::move(arg_names));
		}

		std::unique_ptr<ExprAST> parse_expression() {
			std::unique_ptr<ExprAST> lhs = parse_primary();
			if (!lhs) 
				return nullptr;

			return parse_bin_op_rhs(0, std::move(lhs));
		}

		std::unique_ptr<ExprAST>  parse_primary() {
			switch (curr_tok) {
				case token::tok_identifier:
					return parse_identifier_expr();
				case token::tok_number:
					return parse_number_expr();
				case '(':
					return parse_paren_expr();
				default:
					return log_error("Unkown token when expecting an expression");
			}
		}

		std::unique_ptr<ExprAST> parse_bin_op_rhs(int expr_prec, std::unique_ptr<ExprAST> lhs) {
			while (true) {
				int tok_prec = get_tok_precedence();

				if (tok_prec < expr_prec) // non operators have precedence == -1
					return lhs;

				int bin_op = curr_tok;
				get_next_token();
				std::unique_ptr<ExprAST> rhs = parse_primary();
				if (!rhs)
					return nullptr;

				int next_prec = get_tok_precedence();
				if (next_prec > tok_prec) {
					rhs = parse_bin_op_rhs(tok_prec+1, std::move(rhs));
					if (!rhs)
						return nullptr;
				}

				lhs = std::make_unique<BinaryExprAST>(bin_op, std::move(lhs), std::move(rhs));
			}
		}

		std::unique_ptr<ExprAST> parse_number_expr() {
			std::unique_ptr<NumberExprAST> result = std::make_unique<NumberExprAST>(lexer.num_val);
			get_next_token();
			return std::move(result);
		}

		std::unique_ptr<ExprAST> parse_paren_expr() {
			get_next_token();
			std::unique_ptr<ExprAST> v = parse_expression();

			if (!v) return nullptr;
			if (curr_tok != ')') return log_error("expected ')'");

			get_next_token();
			return v;
		}

		std::unique_ptr<ExprAST> parse_identifier_expr() {
			std::string id_name = lexer.identifier_str;

			get_next_token();

			if (curr_tok != '(') 
				return std::make_unique<VariableExprAST>(id_name);

			get_next_token();
			std::vector<std::unique_ptr<ExprAST>> args;
			if (curr_tok != ')') {
				while (true) {
					std::unique_ptr<ExprAST> arg = parse_expression();
					if (arg) {
						args.push_back(std::move(arg));
					} else {
						return nullptr;
					}

					if (curr_tok == ')') break;

					if (curr_tok != ',') return log_error("Expected ')' or ',' in argument list.");
					get_next_token();
				}
			}

			get_next_token();
			return std::make_unique<CallExprAST>(id_name, std::move(args));
		}

		int get_tok_precedence() {
			if (!isascii(curr_tok)) return -1;

			std::unordered_map<char, int>::const_iterator tok_prec;
			tok_prec = m_binary_op_precedence.find(curr_tok);
			if (tok_prec == m_binary_op_precedence.end()) return -1;

			return tok_prec->second;
		}
};
