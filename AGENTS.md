# AGENTS.md

Rules for changes to this repository: a 2015 course project (an allocator with
defragmentation and a smart pointer), kept as a historical piece.

- Keep the design: all bookkeeping inside the managed buffer, descriptors as a
  dense array at the end, `Pointer` resolved by index. Fix bugs, do not rewrite.
- Format with `clang-format` (`.clang-format`); comments in English.
- The public API is documented with Doxygen (`@brief`, `@param`, `@return`, `@throws`).
- Standard C++17, no dependencies (the tests have their own tiny harness).
- No warnings: `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang), `/W4 /WX` (MSVC).
- Both builds stay in sync: `Makefile` and `CMakeLists.txt`.
- Every bug fix comes with a regression test in `allocator_test.cpp`.
- Tests pass under ASan + UBSan: `make sanitize`.
- `README.md` describes the behavior; update it with the code.
- Changes go through a pull request with green CI.
