#pragma once

#include "codegen_visitor.hpp"

class ExprAST {
    public:
        virtual ~ExprAST() = default;
        virtual llvm::Value *codegen(CodegenVisitor &) { };
};

class NumberExprAST : public ExprAST {
    public: 
        double val;

        NumberExprAST(double val);

        llvm::Value *codegen(CodegenVisitor &) override;
};


class VariableExprAST : public ExprAST {    
    public:
        std::string name;

        VariableExprAST(std::string &name);

        llvm::Value *codegen(CodegenVisitor &) override;
};


class BinaryExprAST : public ExprAST {
    public:
        char op;
        std::unique_ptr<ExprAST> lhs, rhs;

        BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs);

        llvm::Value* codegen(CodegenVisitor &) override;
};

class CallExprAST : public ExprAST {
    public:
        std::string callee;
        std::vector<std::unique_ptr<ExprAST>> args;

        CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args);

        llvm::Value *codegen(CodegenVisitor &) override;
};

class PrototypeAST {
    public:
        std::string name;
        std::vector<std::string> args;
    
        PrototypeAST(const std::string &name, std::vector<std::string> args);

        llvm::Function *codegen(CodegenVisitor &);
};

class FunctionAST {
    
    public:
        std::unique_ptr<PrototypeAST> proto;
        std::unique_ptr<ExprAST> body;

        FunctionAST(std::unique_ptr<PrototypeAST> proto, std::unique_ptr<ExprAST> body);

        llvm::Function *codegen(CodegenVisitor &);
};