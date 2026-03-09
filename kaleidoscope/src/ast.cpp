#include "include/kaleidoscope/ast.hpp"

/*
	NumberExprAST methods
*/
NumberExprAST::NumberExprAST(double val) : val(val) {}

llvm::Value *NumberExprAST::codegen(CodegenVisitor &visitor) {
	return visitor.visit_number_expr(const_cast<NumberExprAST &>(*this));
}


/*
	VariableExprAST methods
*/
VariableExprAST::VariableExprAST(std::string &name) : name(name) {}

llvm::Value *VariableExprAST::codegen(CodegenVisitor &visitor) {
	return visitor.visit_variable_expr(const_cast<VariableExprAST &>(*this));
}


/*
	BinaryExprAST methods
*/
BinaryExprAST::BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs) 
			: op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

llvm::Value *BinaryExprAST::codegen(CodegenVisitor &visitor) {
	return visitor.visit_binary_expr(const_cast<BinaryExprAST &>(*this));
}


/*
	CallExprAST methods
*/
CallExprAST::CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args) 
			: callee(callee), args(std::move(args)) {}

llvm::Value *CallExprAST::codegen(CodegenVisitor &visitor) {
	return visitor.visit_call_expr(const_cast<CallExprAST &>(*this));
}


/*
	PrototypeAST methods
*/
PrototypeAST::PrototypeAST(const std::string &name, std::vector<std::string> args) 
			: name(name), args(std::move(args)) {}

llvm::Function *PrototypeAST::codegen(CodegenVisitor &visitor) {
	return visitor.visit_prototype(const_cast<PrototypeAST &>(*this));
}


/*
	FunctionAST methods
*/
FunctionAST::FunctionAST(std::unique_ptr<PrototypeAST> proto, std::unique_ptr<ExprAST> body)
			: proto(std::move(proto)), body(std::move(body)) {}

llvm::Function *FunctionAST::codegen(CodegenVisitor &visitor) {
	return visitor.visit_function(const_cast<FunctionAST &>(*this));
}
