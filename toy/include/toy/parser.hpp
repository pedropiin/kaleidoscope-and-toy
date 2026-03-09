#include "ast.hpp"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <utility>
#include <vector>
#include <optional>

namespace toy {

/// This is a simple recursive parser for the Toy language. It produces a well
/// formed AST from a stream of Token supplied by the Lexer. No semantic checks
/// or symbol resolution is performed. For example, variables are referenced by
/// string and the code could reference an undeclared variable and the parsing
/// succeeds.
class Parser {
    public:
        /// Create a Parser for the supplied lexer.
        Parser(Lexer &lexer) : lexer(lexer) {}

        /// Parse a full Module. A module is a list of function definitions.
        std::unique_ptr<ModuleAST> parse_module() {
            lexer.get_next_token(); // prime the lexer

            // Parse functions one at a time and accumulate in this vector.
            std::vector<FunctionAST> functions;
            while (auto f = parse_definition()) {
                functions.push_back(std::move(*f));
                if (lexer.get_cur_token() == tok_eof)
                    break;
            }
            // If we didn't reach EOF, there was an error during parsing
            if (lexer.get_cur_token() != tok_eof)
                return parse_error<ModuleAST>("nothing", "at end of module");

            return std::make_unique<ModuleAST>(std::move(functions));
        }

    private:
        Lexer &lexer;

        /// Parse a return statement.
        /// return :== return ; | return expr ;
        std::unique_ptr<ReturnExprAST> parse_return() {
            auto loc = lexer.get_last_location();
            lexer.consume(tok_return);

            // return takes an optional argument
            std::optional<std::unique_ptr<ExprAST>> expr;
            if (lexer.get_cur_token() != ';') {
                expr = parse_expression();
                if (!expr)
                    return nullptr;
            }
            return std::make_unique<ReturnExprAST>(std::move(loc), std::move(expr));
        }

        /// Parse a literal number.
        /// numberexpr ::= number
        std::unique_ptr<ExprAST> parse_number_expr() {
            auto loc = lexer.get_last_location();
            auto result =
                std::make_unique<NumberExprAST>(std::move(loc), lexer.get_value());
            lexer.consume(tok_number);
            return std::move(result);
        }

        /// Parse a literal array expression.
        /// tensorLiteral ::= [ literalList ] | number
        /// literalList ::= tensorLiteral | tensorLiteral, literalList
        std::unique_ptr<ExprAST> parse_tensor_literal_expr() {
            auto loc = lexer.get_last_location();
            lexer.consume(Token('['));

            // Hold the list of values at this nesting level.
            std::vector<std::unique_ptr<ExprAST>> values;
            // Hold the dimensions for all the nesting inside this level.
            std::vector<int64_t> dims;
            do {
                // We can have either another nested array or a number literal.
                if (lexer.get_cur_token() == '[') {
                    values.push_back(parse_tensor_literal_expr());
                    if (!values.back())
                        return nullptr; // parse error in the nested array.
                } else {
                    if (lexer.get_cur_token() != tok_number)
                        return parse_error<ExprAST>("<num> or [", "in literal expression");
                    values.push_back(parse_number_expr());
                }

                // End of this list on ']'
                if (lexer.get_cur_token() == ']')
                    break;

                // Elements are separated by a comma.
                if (lexer.get_cur_token() != ',')
                    return parse_error<ExprAST>("] or ,", "in literal expression");

                lexer.get_next_token(); // eat ,
            } while (true);
            if (values.empty())
                return parse_error<ExprAST>("<something>", "to fill literal expression");
            lexer.get_next_token(); // eat ]

            /// Fill in the dimensions now. First the current nesting level:
            dims.push_back(values.size());

            /// If there is any nested array, process all of them and ensure that
            /// dimensions are uniform.
            if (llvm::any_of(values, [](std::unique_ptr<ExprAST> &expr) {
                    return llvm::isa<LiteralExprAST>(expr.get());
                })) {
                auto *first_literal = llvm::dyn_cast<LiteralExprAST>(values.front().get());
                if (!first_literal)
                    return parse_error<ExprAST>("uniform well-nested dimensions",
                                                "inside literal expression");

                // Append the nested dimensions to the current level
                auto first_dims = first_literal->get_dims();
                dims.insert(dims.end(), first_dims.begin(), first_dims.end());

                // Sanity check that shape is uniform across all elements of the list.
                for (auto &expr : values) {
                    auto *expr_literal = llvm::cast<LiteralExprAST>(expr.get());
                    if (!expr_literal)
                        return parse_error<ExprAST>("uniform well-nested dimensions",
                                                    "inside literal expression");
                    if (expr_literal->get_dims() != first_dims)
                        return parse_error<ExprAST>("uniform well-nested dimensions",
                                                    "inside literal expression");
                }
            }
            return std::make_unique<LiteralExprAST>(std::move(loc), std::move(values),
                                                    std::move(dims));
        }

        /// parenexpr ::= '(' expression ')'
        std::unique_ptr<ExprAST> parse_paren_expr() {
            lexer.get_next_token(); // eat (.
            auto v = parse_expression();
            if (!v)
                return nullptr;

            if (lexer.get_cur_token() != ')')
                return parse_error<ExprAST>(")", "to close expression with parentheses");
            lexer.consume(Token(')'));
            return v;
        }

        /// identifierexpr
        ///   ::= identifier
        ///   ::= identifier '(' expression ')'
        std::unique_ptr<ExprAST> parse_identifier_expr() {
            std::string name(lexer.get_id());

            auto loc = lexer.get_last_location();
            lexer.get_next_token(); // eat identifier.

            if (lexer.get_cur_token() != '(') // Simple variable ref.
                return std::make_unique<VariableExprAST>(std::move(loc), name);

            // This is a function call.
            lexer.consume(Token('('));
            std::vector<std::unique_ptr<ExprAST>> args;
            if (lexer.get_cur_token() != ')') {
                while (true) {
                    if (auto arg = parse_expression())
                        args.push_back(std::move(arg));
                    else
                        return nullptr;

                    if (lexer.get_cur_token() == ')')
                        break;

                    if (lexer.get_cur_token() != ',')
                        return parse_error<ExprAST>(", or )", "in argument list");
                    lexer.get_next_token();
                }
            }
            lexer.consume(Token(')'));

            // It can be a builtin call to print
            if (name == "print") {
                if (args.size() != 1)
                    return parse_error<ExprAST>("<single arg>", "as argument to print()");

                return std::make_unique<PrintExprAST>(std::move(loc), std::move(args[0]));
            }

            // Call to a user-defined function
            return std::make_unique<CallExprAST>(std::move(loc), name, std::move(args));
        }

        /// primary
        ///   ::= identifierexpr
        ///   ::= numberexpr
        ///   ::= parenexpr
        ///   ::= tensorliteral
        std::unique_ptr<ExprAST> parse_primary() {
            switch (lexer.get_cur_token()) {
            default:
                llvm::errs() << "unknown token '" << lexer.get_cur_token()
                                << "' when expecting an expression\n";
                return nullptr;
            case tok_identifier:
                return parse_identifier_expr();
            case tok_number:
                return parse_number_expr();
            case '(':
                return parse_paren_expr();
            case '[':
                return parse_tensor_literal_expr();
            case ';':
                return nullptr;
            case '}':
                return nullptr;
            }
        }

        /// Recursively parse the right hand side of a binary expression, the ExprPrec
        /// argument indicates the precedence of the current binary operator.
        ///
        /// binoprhs ::= ('+' primary)*
        std::unique_ptr<ExprAST> parse_bin_op_rhs(int expr_prec,
                                                    std::unique_ptr<ExprAST> lhs) {
            // If this is a binop, find its precedence.
            while (true) {
                int tok_prec = get_tok_precedence();

                // If this is a binop that binds at least as tightly as the current binop,
                // consume it, otherwise we are done.
                if (tok_prec < expr_prec)
                    return lhs;

                // Okay, we know this is a binop.
                int bin_op = lexer.get_cur_token();
                lexer.consume(Token(bin_op));
                auto loc = lexer.get_last_location();

                // Parse the primary expression after the binary operator.
                auto rhs = parse_primary();
                if (!rhs)
                    return parse_error<ExprAST>("expression", "to complete binary operator");

                // If BinOp binds less tightly with rhs than the operator after rhs, let
                // the pending operator take rhs as its lhs.
                int next_prec = get_tok_precedence();
                if (tok_prec < next_prec) {
                    rhs = parse_bin_op_rhs(tok_prec + 1, std::move(rhs));
                    if (!rhs)
                        return nullptr;
                }

                // Merge lhs/RHS.
                lhs = std::make_unique<BinaryExprAST>(std::move(loc), bin_op,
                                                        std::move(lhs), std::move(rhs));
            }
        }

        /// expression::= primary binop rhs
        std::unique_ptr<ExprAST> parse_expression() {
            auto lhs = parse_primary();
            if (!lhs)
                return nullptr;

            return parse_bin_op_rhs(0, std::move(lhs));
        }

        /// type ::= < shape_list >
        /// shape_list ::= num | num , shape_list
        std::unique_ptr<VarType> parse_type() {
            if (lexer.get_cur_token() != '<')
                return parse_error<VarType>("<", "to begin type");
            lexer.get_next_token(); // eat <

            auto type = std::make_unique<VarType>();

            while (lexer.get_cur_token() == tok_number) {
                type->shape.push_back(lexer.get_value());
                lexer.get_next_token();
                if (lexer.get_cur_token() == ',')
                    lexer.get_next_token();
            }

            if (lexer.get_cur_token() != '>')
                return parse_error<VarType>(">", "to end type");
            lexer.get_next_token(); // eat >
            return type;
        }

        /// Parse a variable declaration, it starts with a `var` keyword followed by
        /// and identifier and an optional type (shape specification) before the
        /// initializer.
        /// decl ::= var identifier [ type ] = expr
        std::unique_ptr<VarDeclExprAST> parse_declaration() {
            if (lexer.get_cur_token() != tok_var)
                return parse_error<VarDeclExprAST>("var", "to begin declaration");
            auto loc = lexer.get_last_location();
            lexer.get_next_token(); // eat var

            if (lexer.get_cur_token() != tok_identifier)
                return parse_error<VarDeclExprAST>("identified",
                                                    "after 'var' declaration");
            std::string id(lexer.get_id());
            lexer.get_next_token(); // eat id

            std::unique_ptr<VarType> type; // Type is optional, it can be inferred
            if (lexer.get_cur_token() == '<') {
                type = parse_type();
                if (!type)
                    return nullptr;
            }

            if (!type)
                type = std::make_unique<VarType>();
            lexer.consume(Token('='));
            auto expr = parse_expression();
            return std::make_unique<VarDeclExprAST>(std::move(loc), std::move(id),
                                                    std::move(*type), std::move(expr));
        }

        /// Parse a block: a list of expression separated by semicolons and wrapped in
        /// curly braces.
        ///
        /// block ::= { expression_list }
        /// expression_list ::= block_expr ; expression_list
        /// block_expr ::= decl | "return" | expr
        std::unique_ptr<ExprASTList> parse_block() {
            if (lexer.get_cur_token() != '{')
                return parse_error<ExprASTList>("{", "to begin block");
            lexer.consume(Token('{'));

            auto expr_list = std::make_unique<ExprASTList>();

            // Ignore empty expressions: swallow sequences of semicolons.
            while (lexer.get_cur_token() == ';')
                lexer.consume(Token(';'));

            while (lexer.get_cur_token() != '}' && lexer.get_cur_token() != tok_eof) {
                if (lexer.get_cur_token() == tok_var) {
                    // Variable declaration
                    auto var_decl = parse_declaration();
                    if (!var_decl)
                        return nullptr;
                    expr_list->push_back(std::move(var_decl));
                } else if (lexer.get_cur_token() == tok_return) {
                    // Return statement
                    auto ret = parse_return();
                    if (!ret)
                        return nullptr;
                    expr_list->push_back(std::move(ret));
                } else {
                    // General expression
                    auto expr = parse_expression();
                    if (!expr)
                        return nullptr;
                    expr_list->push_back(std::move(expr));
                }
                // Ensure that elements are separated by a semicolon.
                if (lexer.get_cur_token() != ';')
                    return parse_error<ExprASTList>(";", "after expression");

                // Ignore empty expressions: swallow sequences of semicolons.
                while (lexer.get_cur_token() == ';')
                    lexer.consume(Token(';'));
            }

            if (lexer.get_cur_token() != '}')
                return parse_error<ExprASTList>("}", "to close block");

            lexer.consume(Token('}'));
            return expr_list;
        }

        /// prototype ::= def id '(' decl_list ')'
        /// decl_list ::= identifier | identifier, decl_list
        std::unique_ptr<PrototypeAST> parse_prototype() {
            auto loc = lexer.get_last_location();

            if (lexer.get_cur_token() != tok_def)
                return parse_error<PrototypeAST>("def", "in prototype");
            lexer.consume(tok_def);

            if (lexer.get_cur_token() != tok_identifier)
                return parse_error<PrototypeAST>("function name", "in prototype");

            std::string fn_name(lexer.get_id());
            lexer.consume(tok_identifier);

            if (lexer.get_cur_token() != '(')
                return parse_error<PrototypeAST>("(", "in prototype");
            lexer.consume(Token('('));

            std::vector<std::unique_ptr<VariableExprAST>> args;
            if (lexer.get_cur_token() != ')') {
                do {
                    std::string name(lexer.get_id());
                    auto loc = lexer.get_last_location();
                    lexer.consume(tok_identifier);
                    auto decl = std::make_unique<VariableExprAST>(std::move(loc), name);
                    args.push_back(std::move(decl));
                    if (lexer.get_cur_token() != ',')
                        break;
                    lexer.consume(Token(','));
                    if (lexer.get_cur_token() != tok_identifier)
                        return parse_error<PrototypeAST>(
                            "identifier", "after ',' in function parameter list");
                } while (true);
            }
            if (lexer.get_cur_token() != ')')
                return parse_error<PrototypeAST>(")", "to end function prototype");

            // success.
            lexer.consume(Token(')'));
            return std::make_unique<PrototypeAST>(std::move(loc), fn_name,
                                                    std::move(args));
        }

        /// Parse a function definition, we expect a prototype initiated with the
        /// `def` keyword, followed by a block containing a list of expressions.
        ///
        /// definition ::= prototype block
        std::unique_ptr<FunctionAST> parse_definition() {
            auto proto = parse_prototype();
            if (!proto)
                return nullptr;

            if (auto block = parse_block())
                return std::make_unique<FunctionAST>(std::move(proto), std::move(block));
            return nullptr;
        }

        /// Get the precedence of the pending binary operator token.
        int get_tok_precedence() {
            if (!isascii(lexer.get_cur_token()))
                return -1;

            // 1 is lowest precedence.
            switch (static_cast<char>(lexer.get_cur_token())) {
            case '-':
                return 20;
            case '+':
                return 20;
            case '*':
                return 40;
            default:
                return -1;
            }
        }

        /// Helper function to signal errors while parsing, it takes an argument
        /// indicating the expected token and another argument giving more context.
        /// Location is retrieved from the lexer to enrich the error message.
        template <typename R, typename T, typename U = const char *>
        std::unique_ptr<R> parse_error(T &&expected, U &&context = "") {
            auto cur_token = lexer.get_cur_token();
            llvm::errs() << "Parse error (" << lexer.get_last_location().line << ", "
                         << lexer.get_last_location().col << "): expected '" << expected
                         << "' " << context << " but has Token " << cur_token;
            if (isprint(cur_token))
                llvm::errs() << " '" << (char)cur_token << "'";
            llvm::errs() << "\n";
            return nullptr;
        }
};

} // namespace toy
