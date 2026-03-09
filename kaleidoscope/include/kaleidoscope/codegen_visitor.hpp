#pragma once

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"

// Optimization imports
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"

#include "kaleidoscope_jit.hpp"

#include <map>
#include <string>
#include <iostream>

// Deal with circular dependency between 'ast.hpp' and 'codegen_visitor.hpp'
class ExprAST;
class NumberExprAST;
class VariableExprAST;
class BinaryExprAST;
class CallExprAST;
class FunctionAST;
class PrototypeAST;

class CodegenVisitor {
    public:
        std::unique_ptr<llvm::LLVMContext> context;
        std::unique_ptr<llvm::IRBuilder<>> builder;
        std::unique_ptr<llvm::Module> module;
        std::map<std::string, llvm::Value *> named_values;

        // Pass and analysis managers
        std::unique_ptr<llvm::FunctionPassManager> function_pass_manager;
        std::unique_ptr<llvm::LoopAnalysisManager> loop_analysis_manager;
        std::unique_ptr<llvm::FunctionAnalysisManager> function_analysis_manager;
        std::unique_ptr<llvm::CGSCCAnalysisManager> control_graph_analysis_manager;
        std::unique_ptr<llvm::ModuleAnalysisManager> module_analysis_manager;
        std::unique_ptr<llvm::PassInstrumentationCallbacks> pass_instrumentation_callbacks;
        std::unique_ptr<llvm::StandardInstrumentations> standard_instrumentations;

        // jit compiler
        std::shared_ptr<llvm::orc::KaleidoscopeJIT> jit;

        CodegenVisitor(std::shared_ptr<llvm::orc::KaleidoscopeJIT> og_jit_ptr);

        void initialize_module_and_managers();

        llvm::Value *visit_number_expr(NumberExprAST &);
        llvm::Value *visit_variable_expr(VariableExprAST &);
        llvm::Value *visit_binary_expr(BinaryExprAST &);
        llvm::Value *visit_call_expr(CallExprAST &);
        llvm::Function *visit_function(FunctionAST &);
        llvm::Function *visit_prototype(PrototypeAST &);
};