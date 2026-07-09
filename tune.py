#!/usr/bin/env python3
"""
Texel tuning for ZeroG chess engine.
Optimizes evaluation weights using L-BFGS-B on quiet training positions.
"""
import numpy as np
from scipy.optimize import minimize, minimize_scalar
import os
import sys
import argparse
import time

# ============================================================
# Parameter definitions — must match tune_export.h enum order
# ============================================================
param_names = [
    "piece_pawn", "piece_knight", "piece_bishop", "piece_rook", "piece_queen",
    "rook_open_file_mg", "rook_open_file_eg", "rook_semi_open_file_mg", "rook_semi_open_file_eg",
    "bishop_pair_bonus",
    "double_pawn_mg", "double_pawn_eg",
    "isolated_pawn_mg", "isolated_pawn_eg",
    "isolated_pawn_semi_open_mg", "isolated_pawn_semi_open_eg",
    # Passed pawns MG
    "passed_pawn_mg_r1", "passed_pawn_mg_r2", "passed_pawn_mg_r3", "passed_pawn_mg_r4", "passed_pawn_mg_r5", "passed_pawn_mg_r6",
    # Passed pawns EG
    "passed_pawn_eg_r1", "passed_pawn_eg_r2", "passed_pawn_eg_r3", "passed_pawn_eg_r4", "passed_pawn_eg_r5", "passed_pawn_eg_r6",
    "passed_pawn_defended_mg", "passed_pawn_defended_eg",
    "passed_pawn_friendly_behind_mg", "passed_pawn_friendly_behind_eg",
    "passed_pawn_enemy_behind_mg", "passed_pawn_enemy_behind_eg",
    # Knight mobility
    *[f"knight_mobility_mg_{i}" for i in range(9)],
    *[f"knight_mobility_eg_{i}" for i in range(9)],
    # Bishop mobility
    *[f"bishop_mobility_mg_{i}" for i in range(14)],
    *[f"bishop_mobility_eg_{i}" for i in range(14)],
    # Rook mobility
    *[f"rook_mobility_mg_{i}" for i in range(15)],
    *[f"rook_mobility_eg_{i}" for i in range(15)],
    # Queen mobility
    *[f"queen_mobility_mg_{i}" for i in range(28)],
    *[f"queen_mobility_eg_{i}" for i in range(28)],
    # PST (mirrored, 32 per piece)
    *[f"pst_pawn_{i}" for i in range(32)],
    *[f"pst_knight_{i}" for i in range(32)],
    *[f"pst_bishop_{i}" for i in range(32)],
    *[f"pst_rook_{i}" for i in range(32)],
    *[f"pst_queen_{i}" for i in range(32)],
    *[f"pst_king_mg_{i}" for i in range(32)],
    *[f"pst_king_eg_{i}" for i in range(32)],
    # King Safety
    "ks_pawn_shield_rank2", "ks_pawn_shield_rank3", "ks_pawn_shield_missing",
    "ks_knight_weight", "ks_bishop_weight", "ks_rook_weight", "ks_queen_weight",
    *[f"ks_safety_table_{i}" for i in range(16)],
]

NUM_PARAMS = len(param_names)

# ============================================================
# Mapping from param names to #define names in eval_constants.h
# ============================================================
def param_to_define(name):
    """Convert a parameter name to its eval_constants.h #define name."""
    mapping = {
        "piece_pawn": "PIECE_PAWN_VAL",
        "piece_knight": "PIECE_KNIGHT_VAL",
        "piece_bishop": "PIECE_BISHOP_VAL",
        "piece_rook": "PIECE_ROOK_VAL",
        "piece_queen": "PIECE_QUEEN_VAL",
        "rook_open_file_mg": "ROOK_OPEN_FILE_MG_VAL",
        "rook_open_file_eg": "ROOK_OPEN_FILE_EG_VAL",
        "rook_semi_open_file_mg": "ROOK_SEMI_OPEN_FILE_MG_VAL",
        "rook_semi_open_file_eg": "ROOK_SEMI_OPEN_FILE_EG_VAL",
        "bishop_pair_bonus": "BISHOP_PAIR_BONUS_VAL",
        "double_pawn_mg": "DOUBLE_PAWN_MG_VAL",
        "double_pawn_eg": "DOUBLE_PAWN_EG_VAL",
        "isolated_pawn_mg": "ISOLATED_PAWN_MG_VAL",
        "isolated_pawn_eg": "ISOLATED_PAWN_EG_VAL",
        "isolated_pawn_semi_open_mg": "ISOLATED_PAWN_SEMI_OPEN_MG_VAL",
        "isolated_pawn_semi_open_eg": "ISOLATED_PAWN_SEMI_OPEN_EG_VAL",
        "passed_pawn_defended_mg": "PASSED_PAWN_DEFENDED_MG_VAL",
        "passed_pawn_defended_eg": "PASSED_PAWN_DEFENDED_EG_VAL",
        "passed_pawn_friendly_behind_mg": "PASSED_PAWN_FRIENDLY_BEHIND_MG_VAL",
        "passed_pawn_friendly_behind_eg": "PASSED_PAWN_FRIENDLY_BEHIND_EG_VAL",
        "passed_pawn_enemy_behind_mg": "PASSED_PAWN_ENEMY_BEHIND_MG_VAL",
        "passed_pawn_enemy_behind_eg": "PASSED_PAWN_ENEMY_BEHIND_EG_VAL",
        "ks_pawn_shield_rank2": "KS_PAWN_SHIELD_RANK2_VAL",
        "ks_pawn_shield_rank3": "KS_PAWN_SHIELD_RANK3_VAL",
        "ks_pawn_shield_missing": "KS_PAWN_SHIELD_MISSING_VAL",
        "ks_knight_weight": "KS_KNIGHT_WEIGHT_VAL",
        "ks_bishop_weight": "KS_BISHOP_WEIGHT_VAL",
        "ks_rook_weight": "KS_ROOK_WEIGHT_VAL",
        "ks_queen_weight": "KS_QUEEN_WEIGHT_VAL",
    }
    if name in mapping:
        return mapping[name]
    # Passed pawns: e.g. passed_pawn_mg_r1 -> PASSED_PAWN_MG_R1_VAL
    if name.startswith("passed_pawn_"):
        suffix = name[len("passed_pawn_"):]
        return "PASSED_PAWN_" + suffix.upper() + "_VAL"
    # Mobility: e.g. knight_mobility_mg_0 -> KNIGHT_MOBILITY_MG_0_VAL
    for piece in ["knight", "bishop", "rook", "queen"]:
        for phase in ["mg", "eg"]:
            prefix = f"{piece}_mobility_{phase}_"
            if name.startswith(prefix):
                idx = name[len(prefix):]
                return f"{piece.upper()}_MOBILITY_{phase.upper()}_{idx}_VAL"
    # PST: e.g. pst_pawn_0 -> PST_PAWN_0_VAL
    if name.startswith("pst_"):
        return name.upper() + "_VAL"
    # King safety table: e.g. ks_safety_table_0 -> KS_SAFETY_TABLE_0_VAL
    if name.startswith("ks_safety_table_"):
        idx = name[len("ks_safety_table_"):]
        return f"KS_SAFETY_TABLE_{idx}_VAL"
    raise ValueError(f"Unknown parameter name: {name}")


def load_csv(filename):
    """Load CSV exported by tune_export using chunked reading for memory efficiency.

    Features are stored as float32 (they're small integers, so no precision loss).
    This avoids the ~10 GB peak memory that np.genfromtxt would use.
    """
    print(f"Loading data from {filename}...")

    # Read header and count lines
    with open(filename, 'r') as f:
        header = f.readline().strip().split(',')
        num_cols = len(header)
        num_lines = sum(1 for _ in f)

    num_features = num_cols - 2
    print(f"  {num_lines:,} positions, {num_features} features")

    # Pre-allocate arrays (features as float32 to save ~1.5 GB)
    results = np.empty(num_lines, dtype=np.float64)
    const_scores = np.empty(num_lines, dtype=np.float64)
    features = np.empty((num_lines, num_features), dtype=np.float32)

    # Read in chunks to avoid massive temporary memory
    chunk_size = 10000
    row = 0
    with open(filename, 'r') as f:
        f.readline()  # skip header
        batch = []
        for line in f:
            batch.append(line)
            if len(batch) >= chunk_size:
                chunk = np.genfromtxt(batch, delimiter=',', dtype=np.float64)
                n = chunk.shape[0]
                results[row:row+n] = chunk[:, 0]
                const_scores[row:row+n] = chunk[:, 1]
                features[row:row+n] = chunk[:, 2:].astype(np.float32)
                row += n
                batch = []
                if row % 100000 < chunk_size:
                    pct = row * 100.0 / num_lines
                    mem_gb = (features.nbytes + results.nbytes + const_scores.nbytes) / (1024**3)
                    print(f"  Loading: {row:,}/{num_lines:,} ({pct:.0f}%) | ~{mem_gb:.1f} GB", flush=True)

        # Final partial chunk
        if batch:
            chunk = np.genfromtxt(batch, delimiter=',', dtype=np.float64)
            n = chunk.shape[0] if chunk.ndim > 1 else 1
            if chunk.ndim == 1:
                chunk = chunk.reshape(1, -1)
            results[row:row+n] = chunk[:, 0]
            const_scores[row:row+n] = chunk[:, 1]
            features[row:row+n] = chunk[:, 2:].astype(np.float32)
            row += n

    print(f"  Loaded {row:,} positions, {num_features} features (float32)")
    return features, const_scores, results


def loss_func(w, features, const_scores, results, K):
    """Mean squared error between sigmoid(eval) and game result."""
    scores = const_scores + features @ w
    sig = K * scores / 400.0
    E = 1.0 / (1.0 + 10.0**(-sig))
    loss = np.mean((results - E)**2)

    # Analytical gradient
    d = (results - E) * E * (1.0 - E)
    grad = - (2.0 * np.log(10) * K / (400.0 * len(results))) * (features.T @ d)
    return loss, grad


def optimal_K(features, const_scores, results, w):
    """Find the optimal K scaling constant."""
    def f(K):
        scores = const_scores + features @ w
        sig = K * scores / 400.0
        E = 1.0 / (1.0 + 10.0**(-sig))
        return np.mean((results - E)**2)
    res = minimize_scalar(f, bounds=(0.1, 3.0), method='bounded')
    return res.x


def write_eval_constants_header(w, output_path):
    """Generate eval_constants.h from tuned weights."""
    w_int = np.round(w).astype(int)

    with open(output_path, 'w') as f:
        f.write("// Generated automatically by tune.py\n")
        f.write("#ifndef EVAL_CONSTANTS_H\n")
        f.write("#define EVAL_CONSTANTS_H\n\n")

        # Group 1: Piece values, rook files, bishop pair, pawn structure, passed pawns, mobility
        sections = [
            ("Piece values", ["piece_pawn", "piece_knight", "piece_bishop", "piece_rook", "piece_queen"]),
            ("Rook open file", ["rook_open_file_mg", "rook_open_file_eg", "rook_semi_open_file_mg", "rook_semi_open_file_eg"]),
            ("Bishop pair", ["bishop_pair_bonus"]),
            ("Pawn structure", ["double_pawn_mg", "double_pawn_eg", "isolated_pawn_mg", "isolated_pawn_eg", "isolated_pawn_semi_open_mg", "isolated_pawn_semi_open_eg"]),
        ]
        for title, names in sections:
            f.write(f"// {title}\n")
            for name in names:
                idx = param_names.index(name)
                f.write(f"#define {param_to_define(name)} {w_int[idx]}\n")
            f.write("\n")

        # Passed pawns
        f.write("// Passed pawns MG\n")
        for r in range(1, 7):
            name = f"passed_pawn_mg_r{r}"
            idx = param_names.index(name)
            f.write(f"#define {param_to_define(name)} {w_int[idx]}\n")
        f.write("\n// Passed pawns EG\n")
        for r in range(1, 7):
            name = f"passed_pawn_eg_r{r}"
            idx = param_names.index(name)
            f.write(f"#define {param_to_define(name)} {w_int[idx]}\n")
        f.write("\n// Passed pawns extra\n")
        for name in ["passed_pawn_defended_mg", "passed_pawn_defended_eg",
                      "passed_pawn_friendly_behind_mg", "passed_pawn_friendly_behind_eg",
                      "passed_pawn_enemy_behind_mg", "passed_pawn_enemy_behind_eg"]:
            idx = param_names.index(name)
            f.write(f"#define {param_to_define(name)} {w_int[idx]}\n")
        f.write("\n")

        # Mobility tables
        for piece, count in [("knight", 9), ("bishop", 14), ("rook", 15), ("queen", 28)]:
            for phase in ["mg", "eg"]:
                f.write(f"// {piece.capitalize()} mobility {phase.upper()}\n")
                for i in range(count):
                    name = f"{piece}_mobility_{phase}_{i}"
                    idx = param_names.index(name)
                    f.write(f"#define {param_to_define(name)} {w_int[idx]}\n")
                f.write("\n")

        # PST section
        f.write("// ============================================================\n")
        f.write("// Piece-Square Tables (mirrored, 32 values per piece)\n")
        f.write("// Index = rank * 4 + min(file, 7-file)\n")
        f.write("// ============================================================\n\n")

        pst_pieces = [
            ("Pawn", "pst_pawn"),
            ("Knight", "pst_knight"),
            ("Bishop", "pst_bishop"),
            ("Rook", "pst_rook"),
            ("Queen", "pst_queen"),
            ("King (middlegame)", "pst_king_mg"),
            ("King (endgame)", "pst_king_eg"),
        ]
        for title, prefix in pst_pieces:
            f.write(f"// {title} PST\n")
            for i in range(32):
                name = f"{prefix}_{i}"
                idx = param_names.index(name)
                f.write(f"#define {param_to_define(name)} {w_int[idx]}\n")
            f.write("\n")

        # PST expansion macro
        f.write("// ============================================================\n")
        f.write("// PST expansion macros: expand 32 mirrored values to 64-entry table\n")
        f.write("// Row pattern: idx+0, idx+1, idx+2, idx+3, idx+3, idx+2, idx+1, idx+0\n")
        f.write("// ============================================================\n\n")
        f.write("#define PST_EXPAND(P) { \\\n")
        for r in range(8):
            base = r * 4
            indices = [base, base+1, base+2, base+3, base+3, base+2, base+1, base]
            entries = ", ".join(f"P##{i}_VAL" for i in indices)
            comma = ", \\" if r < 7 else "  \\"
            f.write(f"    {entries}{comma}\n")
        f.write("}\n\n")

        table_names = [
            ("PST_PAWN_TABLE", "PST_PAWN_"),
            ("PST_KNIGHT_TABLE", "PST_KNIGHT_"),
            ("PST_BISHOP_TABLE", "PST_BISHOP_"),
            ("PST_ROOK_TABLE", "PST_ROOK_"),
            ("PST_QUEEN_TABLE", "PST_QUEEN_"),
            ("PST_KING_MG_TABLE", "PST_KING_MG_"),
            ("PST_KING_EG_TABLE", "PST_KING_EG_"),
        ]
        for macro_name, prefix in table_names:
            f.write(f"#define {macro_name} PST_EXPAND({prefix})\n")
        f.write("\n")

        # King Safety
        f.write("// ============================================================\n")
        f.write("// King Safety constants\n")
        f.write("// ============================================================\n\n")
        for name in ["ks_pawn_shield_rank2", "ks_pawn_shield_rank3", "ks_pawn_shield_missing",
                      "ks_knight_weight", "ks_bishop_weight", "ks_rook_weight", "ks_queen_weight"]:
            idx = param_names.index(name)
            f.write(f"#define {param_to_define(name)} {w_int[idx]}\n")
        for i in range(16):
            name = f"ks_safety_table_{i}"
            idx = param_names.index(name)
            f.write(f"#define {param_to_define(name)} {w_int[idx]}\n")

        f.write("\n#endif // EVAL_CONSTANTS_H\n")

    print(f"  Wrote {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Texel tuning for ZeroG")
    parser.add_argument("-i", "--input", default="tune_features.csv",
                        help="Input CSV file from tune_export (default: tune_features.csv)")
    parser.add_argument("-o", "--output", default="src/eval/eval_constants.h",
                        help="Output header file (default: src/eval/eval_constants.h)")
    parser.add_argument("--maxiter", type=int, default=500,
                        help="Maximum L-BFGS-B iterations (default: 500)")
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: Input file '{args.input}' not found.", file=sys.stderr)
        print(f"Run './builds/zerog --tune-export <epd_file> {args.input}' first.", file=sys.stderr)
        sys.exit(1)

    features, const_scores, results = load_csv(args.input)

    if features.shape[1] != NUM_PARAMS:
        print(f"Error: CSV has {features.shape[1]} feature columns but expected {NUM_PARAMS}.", file=sys.stderr)
        sys.exit(1)

    # Read initial values from CSV header to determine starting point
    with open(args.input, 'r') as f:
        header = f.readline().strip().split(',')
    csv_names = header[2:]  # skip 'result' and 'const_score'
    assert len(csv_names) == NUM_PARAMS, f"Header has {len(csv_names)} params, expected {NUM_PARAMS}"

    # Use initial_param_values from the C code (read from features CSV won't have them)
    # We need to construct the initial values from the existing eval_constants.h
    # For now, read them from the existing header if available
    w_initial = np.zeros(NUM_PARAMS)
    try:
        existing_header = args.output
        if os.path.exists(existing_header):
            with open(existing_header, 'r') as f:
                content = f.read()
            for i, name in enumerate(param_names):
                define_name = param_to_define(name)
                import re
                match = re.search(rf'#define\s+{re.escape(define_name)}\s+(-?\d+)', content)
                if match:
                    w_initial[i] = int(match.group(1))
                else:
                    print(f"  Warning: {define_name} not found in {existing_header}, using 0")
        else:
            print(f"  Warning: {existing_header} not found, using zero initial values")
    except Exception as e:
        print(f"  Warning: Could not read initial values: {e}")

    print(f"\nTotal parameters: {NUM_PARAMS}")
    print(f"Initial weight vector: min={w_initial.min():.0f}, max={w_initial.max():.0f}, mean={w_initial.mean():.1f}")

    # Step 1: Find optimal K
    print("\nStep 1: Finding optimal K...")
    K = optimal_K(features, const_scores, results, w_initial)
    print(f"  Optimal K = {K:.6f}")

    initial_loss = loss_func(w_initial, features, const_scores, results, K)[0]
    print(f"  Initial loss = {initial_loss:.8f}")

    # Step 2: Run L-BFGS-B
    print(f"\nStep 2: Running L-BFGS-B (maxiter={args.maxiter})...")
    t0 = time.time()

    iter_count = [0]
    def callback(xk):
        iter_count[0] += 1
        if iter_count[0] % 50 == 0:
            loss = loss_func(xk, features, const_scores, results, K)[0]
            print(f"  Iteration {iter_count[0]}: loss = {loss:.8f}")

    res = minimize(
        loss_func,
        w_initial,
        args=(features, const_scores, results, K),
        method='L-BFGS-B',
        jac=True,
        options={'maxiter': args.maxiter, 'ftol': 1e-12, 'gtol': 1e-8},
        callback=callback
    )

    elapsed = time.time() - t0
    print(f"\n  Optimization completed in {elapsed:.1f}s")
    print(f"  Status: {res.message}")
    print(f"  Final loss: {res.fun:.8f} (improved {initial_loss - res.fun:.8f})")
    print(f"  Iterations: {res.nit}, function evaluations: {res.nfev}")

    # Step 3: Re-optimize K with new weights
    print("\nStep 3: Re-optimizing K with tuned weights...")
    K2 = optimal_K(features, const_scores, results, res.x)
    print(f"  New K = {K2:.6f} (was {K:.6f})")

    loss_with_new_K = loss_func(res.x, features, const_scores, results, K2)[0]
    print(f"  Loss with new K: {loss_with_new_K:.8f}")

    # Step 4: Write results
    print(f"\nStep 4: Writing eval_constants.h...")
    write_eval_constants_header(res.x, args.output)

    # Print notable parameter changes
    w_final = np.round(res.x).astype(int)
    w_init_int = np.round(w_initial).astype(int)
    changes = np.abs(w_final - w_init_int)
    top_changes = np.argsort(changes)[::-1][:20]
    print("\nTop 20 parameter changes:")
    print(f"  {'Parameter':<35} {'Initial':>8} {'Tuned':>8} {'Change':>8}")
    print(f"  {'-'*35} {'-'*8} {'-'*8} {'-'*8}")
    for idx in top_changes:
        if changes[idx] == 0:
            break
        print(f"  {param_names[idx]:<35} {w_init_int[idx]:>8} {w_final[idx]:>8} {w_final[idx] - w_init_int[idx]:>+8}")

    print("\nDone! Rebuild the engine with 'make' to use the new constants.")


if __name__ == "__main__":
    main()
