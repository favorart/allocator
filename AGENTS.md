# AGENTS.md

Rules for changes to this repository: a 2015 course project (an allocator with
defragmentation and a smart pointer), kept as a historical piece.

- Keep the design: all bookkeeping inside the managed buffer, descriptors as a
  dense array at the end, `Pointer` resolved by index. Fix bugs, do not rewrite.
- Format with `clang-format` (`.clang-format`); comments in English.
- The public API is documented with Doxygen (`@brief`, `@param`, `@return`, `@throws`).
- Layout: public API in `include/`, implementation in `src/`, tests in `test/`, examples in `examples/`, the benchmark in `bench/`.
- Standard C++17; the library has no dependencies, the tests use GoogleTest.
- No warnings: `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang), `/W4 /WX` (MSVC).
- Both builds stay in sync: `Makefile` and `CMakeLists.txt`.
- Every bug fix comes with a regression test in `test/allocator_test.cpp`.
- Tests pass under ASan + UBSan: `make sanitize`.
- Changes that may affect performance come with `make bench` results before and after.
- `README.md` describes the behavior; update it with the code.
- Changes go through a pull request with green CI.
- One commit is one complete change; the build and the tests pass at every commit.
- Commit messages follow Conventional Commits (`fix:`, `feat:`, `refactor:`, `test:`,
  `docs:`, `ci:`, `build:`).
- Every push to `master` is released as the next patch version (`v0.0.N`); the version comes
  from the tag at build time (`git describe`), never a constant in the code.
