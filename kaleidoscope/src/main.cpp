#include "kaleidoscope_config.cpp"

int main(int argc, char* argv[]) {

	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();

	KaleidoscopeConfig kconfig;
	fprintf(stderr, "ready> ");
	kconfig.parser.get_next_token();

	int eof = 0;
	while (!eof) {
		fprintf(stderr, "ready> ");
		switch (kconfig.parser.curr_tok) {
			case tok_def: 
				kconfig.handle_definition();
				break;
			case tok_extern:
				kconfig.handle_extern();
				break;
			case ';':
				kconfig.parser.get_next_token();
				break;
			case tok_eof:
				eof = 1;
				break;
			default:
				kconfig.handle_top_level_expr();
				break;
		}
	}

	return 0;
}
