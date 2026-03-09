#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Error.h"

#include <map>

#include "parser.cpp"
#include "include/kaleidoscope/codegen_visitor.hpp"
#include "include/kaleidoscope/kaleidoscope_jit.hpp"

class KaleidoscopeConfig {
	public:
		llvm::ExitOnError exit_on_err;
		
		Parser parser;
		std::shared_ptr<llvm::orc::KaleidoscopeJIT> jit = std::move(exit_on_err(llvm::orc::KaleidoscopeJIT::Create()));
		CodegenVisitor visitor = CodegenVisitor(jit);

		void handle_definition() {
			if (std::unique_ptr<FunctionAST> function_node = parser.parse_definition()) {
				if (llvm::Function *function_ir = function_node->codegen(visitor)) {
					fprintf(stderr, "Read function definition:\n");
					function_ir->print(llvm::errs());
				}
			} else {
				parser.get_next_token();
			}
		}

		void handle_extern() {
			if (std::unique_ptr<PrototypeAST> prototype_node = parser.parse_extern()) {
				if (llvm::Function *prototype_ir = prototype_node->codegen(visitor)) {
					fprintf(stderr, "Read extern:\n");
					prototype_ir->print(llvm::errs());
				}
			} else {
				parser.get_next_token();
			}
		}

		// void handle_top_level_expr() {
		// 	if (std::unique_ptr<FunctionAST> function_node = parser.parse_top_level_expr()) {
		// 		if (llvm::Function *function_ir = function_node->codegen(visitor)) {
		// 			fprintf(stderr, "Read top-level expression:\n");
		// 			function_ir->print(llvm::errs());
		// 		}
		// 	} else {
		// 		parser.get_next_token();
		// 	}
		// }

		void handle_top_level_expr() {
			if (std::unique_ptr<FunctionAST> function_node = parser.parse_top_level_expr()) {
				if (llvm::Function *function_ir = function_node->codegen(visitor)) {
					// Printing expression's IR
					fprintf(stderr, "Read top-level expression:\n");
					function_ir->print(llvm::errs());
					
					// Create ResourceTracker for the jit'd memory
					llvm::orc::ResourceTrackerSP resource_tracker = jit->getMainJITDylib().createResourceTracker();

					llvm::orc::ThreadSafeModule thread_safe_module = llvm::orc::ThreadSafeModule(std::move(visitor.module), std::move(visitor.context));
					exit_on_err(jit->addModule(std::move(thread_safe_module), resource_tracker));
					visitor.initialize_module_and_managers();

					// Search for the "__anon_expr" symbol in the JIT
					llvm::orc::ExecutorSymbolDef expr_symbol_def = exit_on_err(jit->lookup("__anon_expr"));

					// Get symbol's address and cast it to the right type,
					// so we can call it as a native function
					double (*function_ptr)() = expr_symbol_def.toPtr<double (*)()>();
					fprintf(stderr, "Evaluated to %f\n", function_ptr());

					// Delete anonymous expression module from the JIT
					exit_on_err(resource_tracker->remove());
				}
			} else {
				parser.get_next_token();
			}
		}
};