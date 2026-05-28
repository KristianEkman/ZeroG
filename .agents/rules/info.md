# Agent Information & Rules

This project uses a modular design for its C99 chess engine. 

## Project Documentation
- The master description of project modules, build steps, tests, and architecture is located in the root [README.md](../../README.md) file.
- Refer to [README.md](../../README.md) to understand the responsibilities of each engine module (hashing, movegen, search, board representation, FEN parsing, and UCI protocol loop).

## Rules for AI Agents
1. **Always Update Documentation**: When adding new features, refactoring, or modifying existing modules (including creating new executables/binaries, adding Makefile targets, or writing new scripts), you **MUST** update the root [README.md](../../README.md) to reflect the additions, changes, or deletions. This is a critical step and must not be skipped.
2. **Follow C99 Standards**: Maintain the current compilation rules configured in the [Makefile](../../Makefile).
3. **Run Verification**: Ensure all tests pass using `make test` (or specific tests like `make test_nn` if modifying the neural network module) before completing your work.

