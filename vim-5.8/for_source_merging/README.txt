Run diff to see what changes has been made to files copied here (against their originals).

Note: to avoid using __builtin_va_arg_pack() & __builtin_va_arg_pack_len(), which are not supported by Clang, I manually change ./configure to use O0 for all possible optimizations. Later, I also changed Makefile to use O0 for all optimizations. Then after make, vim_comb.c doesn't contain any such builtin calls.
