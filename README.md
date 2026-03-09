# Kaleidoscope && Toy  

Repo to store the implementation of both the Kaleidoscope and the Toy programming languages. 

Both implemented based on their respective tutorials ([Kaleidoscope Tutorial](https://llvm.org/docs/tutorial/index.html) and [Toy Tutorial](https://mlir.llvm.org/docs/Tutorials/Toy/)).

In regards to the Kaleidoscope code, it only differs from the original in the sense that is reformatted to follow an OOP approach. So code generation is done with a Visitor Pattern, each frontend "pass" has its own class (and file, given the 'single-file strategy' used in the og).
