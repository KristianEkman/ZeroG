---
name: profile
description: Profiles the chess engine performance (perft_bench) using the Makefile's profile target. Supports valgrind/callgrind on Linux and macOS sample command.
---

## Core Concepts

Profiling is critical for optimizing the chess engine. The project provides a `profile` target in the root Makefile to run the CPU profiler on `perft_bench` (or other future bench targets).

Depending on the environment, the profiler automatically chooses:
1. **Callgrind (Valgrind)**: If both `valgrind` and `callgrind_annotate` are installed. This is the preferred tool on Linux.
2. **macOS `sample`**: If `sample` is installed. This is standard on macOS and gathers stack samples of a running process at regular intervals (1ms).

## Configuration Variables

You can configure the profiling runs by overriding the following variables on the command line or environment:

| Variable | Default Value | Description |
| :--- | :--- | :--- |
| `PROFILE_BUILD_DIR` | `builds/profile` | The directory where the profile build binaries and reports are saved. |
| `PROFILE_DEPTH` | `6` | The perft search depth. Depth 5 (~0.17s) is too short to sample; depth 6 (~4s) is recommended. |
| `PROFILE_SAMPLE_SECONDS` | `5` | The duration (in seconds) that the macOS `sample` tool will collect stack traces. |

## Workflow Patterns

### 1. Running the Profiler

Execute the profile command directly from the project root:
```bash
make profile
```

This compiles a specialized build under `builds/profile/perft_bench` using optimized flags with debug symbols (`-O3 -g -DNDEBUG -march=native -flto`), starts the benchmark, and attaches the active profiler.

### 2. Custom Depth or Sample Duration

If you want to run a deeper profiling session (e.g. depth 7, which may take longer) or sample for more seconds:
```bash
make profile PROFILE_DEPTH=7 PROFILE_SAMPLE_SECONDS=10
```

### 3. Analyzing macOS `sample` Reports

The macOS `sample` output is written to `$(PROFILE_BUILD_DIR)/sample.txt`.
- The top part contains details about the environment, path, and sample count.
- The **Call graph** section shows recursive stacks. Look for nodes with higher sample counts to identify hot spots (e.g. `perft`, `apply_move`, `is_square_attacked`).
- The **Sort by top of stack** section shows the functions that were actively running at the tip of the stack when samples were captured. This lists absolute hotspots directly.

### 4. Analyzing Callgrind Reports

The Callgrind report is saved to `$(PROFILE_BUILD_DIR)/callgrind.out`.
- The Makefile automatically runs `callgrind_annotate` to print out the line/function-level instruction counts.
- For interactive analysis, you can load `callgrind.out` in visualizers like **KCachegrind** (Linux) or **QCachegrind** (macOS).

## Troubleshooting

- **Sampling fails/Short execution**: If `PROFILE_DEPTH` is set too low (e.g. <= 4), `perft_bench` might exit before `sample` can attach or gather enough samples. Keep `PROFILE_DEPTH >= 6` for reliable macOS sampling.
- **Symbol resolution**: Ensure you do not strip the binary. The Makefile target handles this by compiling with `-g`.
- **System Integrity Protection (SIP)**: macOS `sample` might warn or restrict sampling if trying to profile processes that require privileges. However, since the compiled binary is owned by the local user, it should run without `sudo`.
