#pragma once

#include "lexer.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"

#include <utility>
#include <vector>
#include <optional>

namespace toy {

struct VarType {
    std::vector<int64_t> shape;
};

class ExprAST {
    public:
        enum ExprASTKind {
            Expr_VarDecl,
            Expr_Return,
            Expr_Num,
            Expr_Literal,
            Expr_Var,
            Expr_BinOp,
            Expr_Call,
            Expr_Print,
        };

        ExprAST(ExprASTKind kind, Location location) 
            : kind(kind), location(std::move(location)) {}
        virtual ~ExprAST() = default;

        ExprASTKind get_kind() const { return kind; }

        const Location &loc() { return location; }

    private:
        const ExprASTKind kind;
        Location location;
};

using ExprASTList = std::vector<std::unique_ptr<ExprAST>>;

class NumberExprAST : public ExprAST {
    public: 
        NumberExprAST(Location location, double val)
            : ExprAST(Expr_Num, std::move(location)), val(val) {}

        double get_value() { return val; }

        static bool classof(const ExprAST *c) { return c->get_kind() == Expr_Num; }

    private: 
        double val;
};

class LiteralExprAST : public ExprAST {
    public: 
        LiteralExprAST(Location location, 
                        std::vector<std::unique_ptr<ExprAST>> values,
                        std::vector<int64_t> dims) 
            : ExprAST(Expr_Literal, std::move(location)), 
            values(std::move(values)),
            dims(std::move(dims)) {}

        llvm::ArrayRef<std::unique_ptr<ExprAST>> get_values() { return values; }

        llvm::ArrayRef<int64_t> get_dims() { return dims; }

        static bool classof(const ExprAST *c) { return c->get_kind() == Expr_Literal; }

    private:
        std::vector<std::unique_ptr<ExprAST>> values;
        std::vector<int64_t> dims;
};

class VariableExprAST : public ExprAST {
    public: 
        VariableExprAST(Location location, llvm::StringRef name) 
            : ExprAST(Expr_Var, std::move(location)), name(name) {}

        llvm::StringRef get_name() { return name; }

        static bool classof(const ExprAST *c) { return c->get_kind() == Expr_Var; }

    private: 
        std::string name;
};

class VarDeclExprAST : public ExprAST {
    public:
        VarDeclExprAST(Location location,
                        llvm::StringRef name,
                        VarType type,
                        std::unique_ptr<ExprAST> init_val) 
            : ExprAST(Expr_VarDecl, std::move(location)),
            name(name),
            type(std::move(type)),
            init_val(std::move(init_val)) {}

        llvm::StringRef get_name() { return name; }

        ExprAST *get_init_val() { return init_val.get(); }

        const VarType &get_type() { return type; }

        static bool classof(const ExprAST *c) { return c->get_kind() == Expr_VarDecl; }

    private:
        std::string name;
        VarType type;
        std::unique_ptr<ExprAST> init_val;
};

class ReturnExprAST : public ExprAST {
    public:
        ReturnExprAST(Location location, std::optional<std::unique_ptr<ExprAST>> expr) 
            : ExprAST(Expr_Return, std::move(location)), expr(std::move(expr)) {}

        std::optional<ExprAST *> get_expr() {
            if (expr.has_value()) return expr->get(); 
            return std::nullopt;
        }

        static bool classof(const ExprAST *c) { return c->get_kind() == Expr_Return; }

    private:
        std::optional<std::unique_ptr<ExprAST>> expr;
};

class BinaryExprAST : public ExprAST {
    public:
        BinaryExprAST(Location location, 
                    char op,
                    std::unique_ptr<ExprAST> lhs,
                    std::unique_ptr<ExprAST> rhs) 
            : ExprAST(Expr_BinOp, std::move(location)), 
            op(op),
            lhs(std::move(lhs)),
            rhs(std::move(rhs)) {}

        char get_op() { return op; }

        ExprAST *get_lhs() { return lhs.get(); }

        ExprAST *get_rhs() { return rhs.get(); }

        static bool classof(const ExprAST *c) { return c->get_kind() == Expr_BinOp; }

    private:
        char op;
        std::unique_ptr<ExprAST> lhs;
        std::unique_ptr<ExprAST> rhs;
};

class CallExprAST : public ExprAST {
    public:
        CallExprAST(Location location, 
                    std::string callee,
                    std::vector<std::unique_ptr<ExprAST>> args) 
            : ExprAST(Expr_Call, std::move(location)), 
            callee(callee),
            args(std::move(args)) {}

        llvm::StringRef get_callee() { return callee; }

        llvm::ArrayRef<std::unique_ptr<ExprAST>> get_args() { return args; }

        static bool classof(const ExprAST *c) { return c->get_kind() == Expr_Call; }

    private:
        std::string callee;
        std::vector<std::unique_ptr<ExprAST>> args;
};

class PrintExprAST : public ExprAST {
    public:
        PrintExprAST(Location location, std::unique_ptr<ExprAST> arg) 
            : ExprAST(Expr_Print, std::move(location)), arg(std::move(arg)) {}

        llvm::ArrayRef<std::unique_ptr<ExprAST>> get_arg() { return arg; }

        static bool classof(const ExprAST *c) { return c->get_kind() == Expr_Print; }

    private:
        std::unique_ptr<ExprAST> arg;
};

class PrototypeAST {
    public:
        PrototypeAST(Location location,
                    const std::string &name,
                    std::vector<std::unique_ptr<VariableExprAST>> args)
            : location(std::move(location)), name(name), args(std::move(args)) {}

        const Location &loc() { return location; }

        llvm::StringRef get_name() const { return name; }

        llvm::ArrayRef<std::unique_ptr<VariableExprAST>> get_args() { return args; }

    private:
        Location location;
        std::string name;
        std::vector<std::unique_ptr<VariableExprAST>> args;
};

class FunctionAST {
    public:
        FunctionAST(std::unique_ptr<PrototypeAST> proto, std::unique_ptr<ExprASTList> body) 
            : proto(std::move(proto)), body(std::move(body)) {}

        PrototypeAST *get_proto() { return proto.get(); }

        ExprASTList *get_body() { return body.get(); } 

    private:
        std::unique_ptr<PrototypeAST> proto;
        std::unique_ptr<ExprASTList> body;
};

class ModuleAST {
    public:
        ModuleAST(std::vector<FunctionAST> functions) 
            :functions(std::move(functions)) {}

        std::vector<FunctionAST>::const_iterator begin() { return functions.begin(); }

        std::vector<FunctionAST>::const_iterator end() { return functions.end(); }

    private:
        std::vector<FunctionAST> functions;
};

void dump(ModuleAST &);

} // end 'namespace toy'