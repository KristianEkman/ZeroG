#!/usr/bin/env python3
"""
Texel tuning for ZeroG chess engine.
Optimizes evaluation weights using L-BFGS-B on quiet training positions.
"""
import numpy as np
from scipy.optimize import minimize, minimize_scalar
from scipy.special import expit
import os
import sys
import argparse
import time
import re

# ============================================================
# Parameter definitions — must match tune_export.h enum order
# ============================================================
param_names = [
    "piece_pawn", "piece_knight", "piece_bishop", "piece_rook", "piece_queen",
    "rook_open_file_mg", "rook_open_file_eg", "rook_semi_open_file_mg", "rook_semi_open_file_eg",
    "rook_on_7th_mg", "rook_on_7th_eg", "connected_rooks_mg", "connected_rooks_eg",
    "bishop_pair_bonus",
    "double_pawn_mg", "double_pawn_eg",
    "isolated_pawn_mg", "isolated_pawn_eg",
    "isolated_pawn_semi_open_mg", "isolated_pawn_semi_open_eg",
    "knight_outpost_mg", "knight_outpost_eg", "bishop_outpost_mg", "bishop_outpost_eg",
    # Passed pawns MG
    "passed_pawn_mg_r1", "passed_pawn_mg_r2", "passed_pawn_mg_r3", "passed_pawn_mg_r4", "passed_pawn_mg_r5", "passed_pawn_mg_r6",
    # Passed pawns EG
    "passed_pawn_eg_r1", "passed_pawn_eg_r2", "passed_pawn_eg_r3", "passed_pawn_eg_r4", "passed_pawn_eg_r5", "passed_pawn_eg_r6",
    "passed_pawn_connected_mg", "passed_pawn_connected_eg",
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
PARAM_INDEX = {name: i for i, name in enumerate(param_names)}

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
        "rook_on_7th_mg": "ROOK_ON_7TH_MG_VAL",
        "rook_on_7th_eg": "ROOK_ON_7TH_EG_VAL",
        "connected_rooks_mg": "CONNECTED_ROOKS_MG_VAL",
        "connected_rooks_eg": "CONNECTED_ROOKS_EG_VAL",
        "bishop_pair_bonus": "BISHOP_PAIR_BONUS_VAL",
        "double_pawn_mg": "DOUBLE_PAWN_MG_VAL",
        "double_pawn_eg": "DOUBLE_PAWN_EG_VAL",
        "isolated_pawn_mg": "ISOLATED_PAWN_MG_VAL",
        "isolated_pawn_eg": "ISOLATED_PAWN_EG_VAL",
        "isolated_pawn_semi_open_mg": "ISOLATED_PAWN_SEMI_OPEN_MG_VAL",
        "isolated_pawn_semi_open_eg": "ISOLATED_PAWN_SEMI_OPEN_EG_VAL",
        "knight_outpost_mg": "KNIGHT_OUTPOST_MG_VAL",
        "knight_outpost_eg": "KNIGHT_OUTPOST_EG_VAL",
        "bishop_outpost_mg": "BISHOP_OUTPOST_MG_VAL",
        "bishop_outpost_eg": "BISHOP_OUTPOST_EG_VAL",
        "passed_pawn_connected_mg": "PASSED_PAWN_CONNECTED_MG_VAL",
        "passed_pawn_connected_eg": "PASSED_PAWN_CONNECTED_EG_VAL",
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


# Constants for the Texel sigmoid
LN10_F64 = float(np.log(10.0))


def quantize_weights(w):
    """Quantize floating-point weights to integers and enforce rook parameter rules."""
    w_int = np.rint(w).astype(np.int32)

    open_mg = PARAM_INDEX["rook_open_file_mg"]
    semi_mg = PARAM_INDEX["rook_semi_open_file_mg"]
    w_int[open_mg] = max(w_int[open_mg], w_int[semi_mg] + 1)

    open_eg = PARAM_INDEX["rook_open_file_eg"]
    semi_eg = PARAM_INDEX["rook_semi_open_file_eg"]
    w_int[open_eg] = max(w_int[open_eg], w_int[semi_eg] + 1)

    return w_int



PIECE_NAMES = [
    "piece_pawn",
    "piece_knight",
    "piece_bishop",
    "piece_rook",
    "piece_queen",
]

DEFAULT_MATERIAL_VALUES = {
    "piece_pawn": 100.0,
    "piece_knight": 320.0,
    "piece_bishop": 330.0,
    "piece_rook": 500.0,
    "piece_queen": 950.0,
}

DEFAULT_MATERIAL_BOUNDS = {
    "piece_pawn": (80.0, 120.0),
    "piece_knight": (250.0, 400.0),
    "piece_bishop": (270.0, 420.0),
    "piece_rook": (430.0, 650.0),
    "piece_queen": (800.0, 1100.0),
}


def read_initial_weights(initial_header_path, allow_zero_init):
    """Read the parameter vector from eval_constants.h."""
    weights = np.zeros(NUM_PARAMS, dtype=np.float64)

    if not os.path.exists(initial_header_path):
        if not allow_zero_init:
            raise FileNotFoundError(
                f"Initial header file '{initial_header_path}' not found. "
                "Use --allow-zero-init if you intentionally want zeros."
            )
        print(
            f"  Warning: Initial header '{initial_header_path}' not found; "
            "using zero initial values"
        )
        return weights

    try:
        with open(initial_header_path, "r", encoding="utf-8") as f:
            content = f.read()

        missing = []
        for i, name in enumerate(param_names):
            define_name = param_to_define(name)
            match = re.search(
                rf"#define\s+{re.escape(define_name)}\s+(-?\d+)",
                content,
            )
            if match:
                weights[i] = int(match.group(1))
            else:
                missing.append(define_name)
                if allow_zero_init:
                    print(
                        f"  Warning: {define_name} not found in "
                        f"{initial_header_path}; using 0"
                    )

        if missing and not allow_zero_init:
            raise ValueError(
                f"Initial header '{initial_header_path}' is incomplete. "
                f"Missing {len(missing)} definitions: "
                f"{', '.join(missing[:10])}"
                f"{'...' if len(missing) > 10 else ''}. "
                "Use --allow-zero-init only if this is intentional."
            )
    except Exception as exc:
        if not allow_zero_init:
            raise
        print(
            f"  Warning: Could not read initial values from "
            f"'{initial_header_path}': {exc}. Using zeros."
        )
        weights.fill(0.0)

    return weights


def parse_csv_layout(filename):
    """Validate a tuning CSV header and return numeric column indices."""
    with open(filename, "r", encoding="utf-8-sig") as f:
        header = f.readline().strip().split(",")

    if not header or header == [""]:
        raise ValueError(f"CSV '{filename}' has no header")

    duplicates = sorted({name for name in header if header.count(name) > 1})
    if duplicates:
        raise ValueError(
            f"CSV '{filename}' contains duplicate columns: {duplicates[:10]}"
        )

    required = {"result", "const_score", *param_names}
    allowed = required | {"engine_score"}
    missing = sorted(required - set(header))
    extra = sorted(set(header) - allowed)

    if missing or extra:
        message = f"CSV header in '{filename}' does not match the tuner."
        if missing:
            message += f" Missing: {missing[:10]}."
        if extra:
            message += f" Extra: {extra[:10]}."
        raise ValueError(message)

    indices = [
        header.index("result"),
        header.index("const_score"),
        *[header.index(name) for name in param_names],
    ]
    engine_score_index = header.index("engine_score") if "engine_score" in header else None
    if engine_score_index is not None:
        indices.append(engine_score_index)

    return tuple(indices), engine_score_index is not None


def load_csv(filename, soft_labels=False):
    """Load and validate an exported tuning CSV.

    Required numeric columns are result, const_score and every parameter in
    param_names. An optional engine_score column enables exact reconstruction
    verification before tuning.
    """
    print(f"Loading data from {filename}...")
    usecols, has_engine_score = parse_csv_layout(filename)

    t0 = time.time()
    data = np.loadtxt(
        filename,
        delimiter=",",
        skiprows=1,
        dtype=np.float32,
        ndmin=2,
        usecols=usecols,
    )
    elapsed = time.time() - t0

    if data.size == 0 or data.shape[0] == 0:
        raise ValueError(f"Dataset '{filename}' contains no positions")

    expected_columns = 2 + NUM_PARAMS + int(has_engine_score)
    if data.shape[1] != expected_columns:
        raise ValueError(
            f"CSV '{filename}' produced {data.shape[1]} numeric columns; "
            f"expected {expected_columns}"
        )

    results = np.ascontiguousarray(data[:, 0], dtype=np.float64)
    const_scores = np.ascontiguousarray(data[:, 1], dtype=np.float32)
    features = np.ascontiguousarray(data[:, 2:2 + NUM_PARAMS], dtype=np.float32)
    engine_scores = None
    if has_engine_score:
        engine_scores = np.ascontiguousarray(data[:, -1], dtype=np.float64)
    del data

    if not np.isfinite(features).all():
        raise ValueError(f"Feature matrix in '{filename}' contains NaN or infinity")
    if not np.isfinite(const_scores).all():
        raise ValueError(f"const_score in '{filename}' contains NaN or infinity")
    if not np.isfinite(results).all():
        raise ValueError(f"Results in '{filename}' contain NaN or infinity")
    if engine_scores is not None and not np.isfinite(engine_scores).all():
        raise ValueError(f"engine_score in '{filename}' contains NaN or infinity")

    if soft_labels:
        valid_results = (results >= 0.0) & (results <= 1.0)
        error_text = "outside [0, 1]"
    else:
        valid_results = np.isin(results, (0.0, 0.5, 1.0))
        error_text = "not one of 0, 0.5, or 1"

    if not np.all(valid_results):
        bad = np.unique(results[~valid_results])
        raise ValueError(
            f"Invalid results in '{filename}' ({error_text}): {bad[:10]}"
        )

    memory_gb = (
        features.nbytes
        + results.nbytes
        + const_scores.nbytes
        + (0 if engine_scores is None else engine_scores.nbytes)
    ) / (1024 ** 3)

    print(
        f"  Loaded {len(results):,} positions and {features.shape[1]} features "
        f"in {elapsed:.2f}s"
    )
    print(f"  Memory footprint: ~{memory_gb:.2f} GB")

    return {
        "filename": filename,
        "features": features,
        "const_scores": const_scores,
        "results": results,
        "engine_scores": engine_scores,
    }


def predict_scores(w, dataset):
    """Reconstruct static evaluation scores in centipawns."""
    w_f32 = np.asarray(w, dtype=np.float32)
    return dataset["const_scores"] + dataset["features"] @ w_f32


def prediction_loss(w, dataset, K):
    """Return unregularized mean squared Texel loss."""
    scores = predict_scores(w, dataset).astype(np.float64)
    probabilities = expit((LN10_F64 * K / 400.0) * scores)
    error = probabilities - dataset["results"]
    return float(np.mean(error * error))


def loss_func(
    w,
    features,
    const_scores,
    results,
    K,
    regularization_center=None,
    regularization_inv_scale_sq=None,
    regularization_lambda=0.0,
):
    """Return regularized Texel MSE and its analytical gradient."""
    w_f32 = np.asarray(w, dtype=np.float32)
    scores_f32 = const_scores + features @ w_f32
    scores_f64 = scores_f32.astype(np.float64)

    sigmoid_scale = LN10_F64 * K / 400.0
    probabilities = expit(sigmoid_scale * scores_f64)
    error = probabilities - results
    loss = float(np.mean(error * error))

    dscore = (
        (2.0 * sigmoid_scale / len(results))
        * error
        * probabilities
        * (1.0 - probabilities)
    ).astype(np.float32)
    grad = (features.T @ dscore).astype(np.float64)

    if regularization_lambda > 0.0:
        if regularization_center is None or regularization_inv_scale_sq is None:
            raise ValueError("Regularization requested without center/scales")
        delta = np.asarray(w, dtype=np.float64) - regularization_center
        weighted_square = delta * delta * regularization_inv_scale_sq
        loss += regularization_lambda * float(np.mean(weighted_square))
        grad += (
            2.0
            * regularization_lambda
            / NUM_PARAMS
            * delta
            * regularization_inv_scale_sq
        )

    return loss, grad


def optimal_K(dataset, w):
    """Find the optimal Texel sigmoid scaling constant for a dataset."""
    scores = predict_scores(w, dataset).astype(np.float64)
    results = dataset["results"]

    def objective(K):
        probabilities = expit((LN10_F64 * K / 400.0) * scores)
        error = probabilities - results
        return float(np.mean(error * error))

    result = minimize_scalar(
        objective,
        bounds=(0.1, 3.0),
        method="bounded",
        options={"xatol": 1e-8},
    )

    if not result.success or not np.isfinite(result.x):
        raise RuntimeError(f"K optimization failed: {result.message}")

    if np.isclose(result.x, 0.1, atol=1e-4) or np.isclose(result.x, 3.0, atol=1e-4):
        print("Warning: optimal K is at the configured search boundary")

    return float(result.x)


def verify_reconstruction(dataset, header_weights, args, label):
    """Verify const_score + features @ weights against exported engine_score."""
    engine_scores = dataset["engine_scores"]
    if engine_scores is None:
        message = (
            f"{label} CSV has no engine_score column; exact evaluation "
            "reconstruction could not be verified"
        )
        if args.require_engine_score:
            raise ValueError(message)
        print(f"  Warning: {message}")
        return

    reconstructed = predict_scores(header_weights, dataset).astype(np.float64)
    error = reconstructed - engine_scores
    absolute_error = np.abs(error)
    mae = float(np.mean(absolute_error))
    rmse = float(np.sqrt(np.mean(error * error)))
    maximum = float(np.max(absolute_error))

    print(f"  {label} reconstruction MAE:  {mae:.6f} cp")
    print(f"  {label} reconstruction RMSE: {rmse:.6f} cp")
    print(f"  {label} reconstruction max:  {maximum:.6f} cp")

    failed = (
        mae > args.max_reconstruction_mae
        or maximum > args.max_reconstruction_error
    )
    if failed:
        message = (
            f"{label} reconstruction failed: MAE {mae:.6f} cp "
            f"(limit {args.max_reconstruction_mae}) and max {maximum:.6f} cp "
            f"(limit {args.max_reconstruction_error})"
        )
        if not args.allow_reconstruction_error:
            raise ValueError(message)
        print(f"  Warning: {message}")


def feature_diagnostics(features, rare_threshold):
    """Print feature coverage diagnostics and return nonzero counts."""
    print("\nFeature diagnostics:")
    nonzero_count = np.count_nonzero(features, axis=0)
    feature_std = np.std(features, axis=0, dtype=np.float64)

    zero_indices = np.flatnonzero(nonzero_count == 0)
    rare_indices = np.flatnonzero(
        (nonzero_count > 0) & (nonzero_count < rare_threshold)
    )
    constant_indices = np.flatnonzero(feature_std < 1e-12)

    print(f"  Zero-coverage features: {len(zero_indices)}")
    print(f"  Features below {rare_threshold:,} occurrences: {len(rare_indices)}")
    print(f"  Near-constant features: {len(constant_indices)}")

    for idx in zero_indices[:20]:
        print(f"    ZERO: {param_names[idx]}")
    for idx in rare_indices[:20]:
        print(
            f"    RARE: {param_names[idx]:<35} "
            f"nonzero={nonzero_count[idx]:,}"
        )
    if len(zero_indices) > 20 or len(rare_indices) > 20:
        print("    ... additional diagnostics omitted")

    return nonzero_count


def parameter_base_scale(name):
    """Return a prior scale used by L2 regularization."""
    if name.startswith("piece_"):
        return 100.0
    if "mobility" in name:
        return 40.0
    if name.startswith("pst_"):
        return 35.0
    if name.startswith("ks_safety_table_"):
        return 100.0
    if name.startswith("ks_") and name.endswith("_weight"):
        return 3.0
    return 30.0


def make_regularization_scales(nonzero_count, rarity_reference):
    """Regularize rare features more strongly than common features."""
    base = np.array(
        [parameter_base_scale(name) for name in param_names],
        dtype=np.float64,
    )
    coverage = np.maximum(nonzero_count.astype(np.float64), 1.0)
    rarity_factor = np.sqrt(np.minimum(coverage / max(rarity_reference, 1), 1.0))
    rarity_factor = np.clip(rarity_factor, 0.20, 1.0)
    effective_scale = base * rarity_factor
    return 1.0 / (effective_scale * effective_scale)


def apply_bound(bounds, index, lower, upper):
    """Intersect a parameter bound with its current bound."""
    old_lower, old_upper = bounds[index]
    if old_lower is not None:
        lower = old_lower if lower is None else max(old_lower, lower)
    if old_upper is not None:
        upper = old_upper if upper is None else min(old_upper, upper)
    if lower is not None and upper is not None and lower > upper:
        raise ValueError(f"Inconsistent bounds for {param_names[index]}")
    bounds[index] = (lower, upper)


def lock_parameter(bounds, index, value):
    bounds[index] = (float(value), float(value))


def build_start_and_bounds(header_weights, nonzero_count, args):
    """Create the optimization start vector, bounds and lock report."""
    w_initial = header_weights.copy()
    bounds = [(None, None) for _ in range(NUM_PARAMS)]
    lock_reasons = {}

    if not args.keep_initial_material:
        changed_material = []
        for name, value in DEFAULT_MATERIAL_VALUES.items():
            idx = PARAM_INDEX[name]
            old_value = w_initial[idx]
            w_initial[idx] = value
            if old_value != value:
                changed_material.append((name, old_value, value))
        if changed_material:
            print("  Reset material starting values:")
            for name, old, new_value in changed_material:
                print(f"    {name:<35} {old:>9.2f} -> {new_value:>9.2f}")

    pawn_idx = PARAM_INDEX["piece_pawn"]
    w_initial[pawn_idx] = args.pawn_value

    if args.fix_piece_values and args.unfix_pawn:
        raise ValueError(
            "--fix-piece-values cannot be combined with --unfix-pawn"
        )

    if not args.no_material_bounds:
        for name, bound in DEFAULT_MATERIAL_BOUNDS.items():
            apply_bound(bounds, PARAM_INDEX[name], *bound)

    if args.fix_piece_values:
        for name in PIECE_NAMES:
            idx = PARAM_INDEX[name]
            lock_parameter(bounds, idx, w_initial[idx])
            lock_reasons[idx] = "fixed piece value"
    elif not args.unfix_pawn:
        lock_parameter(bounds, pawn_idx, args.pawn_value)
        lock_reasons[pawn_idx] = "fixed pawn scale"

    freeze_mobility = args.freeze_mobility_zero_buckets or args.fix_mobility_base
    if args.fix_mobility_base:
        print(
            "  Warning: --fix-mobility-base is deprecated; use "
            "--freeze-mobility-zero-buckets. It freezes current values and "
            "does not normalize them to zero."
        )
    if freeze_mobility:
        for piece in ("knight", "bishop", "rook", "queen"):
            for phase in ("mg", "eg"):
                idx = PARAM_INDEX[f"{piece}_mobility_{phase}_0"]
                lock_parameter(bounds, idx, w_initial[idx])
                lock_reasons[idx] = "frozen mobility zero bucket"

    if args.min_feature_count > 0:
        for idx, count in enumerate(nonzero_count):
            if count < args.min_feature_count:
                lock_parameter(bounds, idx, w_initial[idx])
                lock_reasons[idx] = f"feature count {count}"

    clipped = []
    for idx, (lower, upper) in enumerate(bounds):
        old_value = w_initial[idx]
        if lower is not None and w_initial[idx] < lower:
            w_initial[idx] = lower
        if upper is not None and w_initial[idx] > upper:
            w_initial[idx] = upper
        if w_initial[idx] != old_value:
            clipped.append((param_names[idx], old_value, w_initial[idx]))

    if clipped:
        print(f"  Reset/clipped {len(clipped)} starting parameters:")
        for name, old, new in clipped[:20]:
            print(f"    {name:<35} {old:>9.2f} -> {new:>9.2f}")
        if len(clipped) > 20:
            print("    ... additional clipped values omitted")

    if lock_reasons:
        print(f"  Locked parameters: {len(lock_reasons)}")
        for idx in sorted(lock_reasons)[:20]:
            print(f"    {param_names[idx]:<35} {lock_reasons[idx]}")
        if len(lock_reasons) > 20:
            print("    ... additional locks omitted")

    return w_initial, bounds


def free_parameter_mask(bounds):
    return np.array(
        [
            not (
                lower is not None
                and upper is not None
                and np.isclose(lower, upper)
            )
            for lower, upper in bounds
        ],
        dtype=bool,
    )


def write_eval_constants_header(w, output_path):
    """Generate eval_constants.h from tuned weights."""
    w_int = quantize_weights(w)

    output_dir = os.path.dirname(os.path.abspath(output_path))
    os.makedirs(output_dir, exist_ok=True)
    temp_path = f"{output_path}.tmp.{os.getpid()}"

    with open(temp_path, 'w') as f:
        f.write("// Generated automatically by tune.py\n")
        f.write("#ifndef EVAL_CONSTANTS_H\n")
        f.write("#define EVAL_CONSTANTS_H\n\n")

        # Group 1: Piece values, rook files, bishop pair, pawn structure, outposts, passed pawns, mobility
        sections = [
            ("Piece values", ["piece_pawn", "piece_knight", "piece_bishop", "piece_rook", "piece_queen"]),
            ("Rook open file & activity", ["rook_open_file_mg", "rook_open_file_eg", "rook_semi_open_file_mg", "rook_semi_open_file_eg", "rook_on_7th_mg", "rook_on_7th_eg", "connected_rooks_mg", "connected_rooks_eg"]),
            ("Bishop pair", ["bishop_pair_bonus"]),
            ("Pawn structure", ["double_pawn_mg", "double_pawn_eg", "isolated_pawn_mg", "isolated_pawn_eg", "isolated_pawn_semi_open_mg", "isolated_pawn_semi_open_eg"]),
            ("Outposts", ["knight_outpost_mg", "knight_outpost_eg", "bishop_outpost_mg", "bishop_outpost_eg"]),
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
        for name in ["passed_pawn_connected_mg", "passed_pawn_connected_eg",
                      "passed_pawn_defended_mg", "passed_pawn_defended_eg",
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

    os.replace(temp_path, output_path)
    print(f"  Wrote {output_path}")
    return w_int



def main():
    parser = argparse.ArgumentParser(
        description="Robust Texel tuning for ZeroG"
    )
    parser.add_argument(
        "-i",
        "--input",
        default="tune_features.csv",
        help="Training CSV exported by tune_export",
    )
    parser.add_argument(
        "--validation",
        help=(
            "Optional held-out validation CSV. Split by game before export; "
            "do not randomly mix positions from the same game."
        ),
    )
    parser.add_argument(
        "-o",
        "--output",
        default="src/eval/eval_constants.h",
        help="Output eval_constants.h path",
    )
    parser.add_argument(
        "--initial-header",
        default="src/eval/eval_constants.h",
        help="Header containing the exporter/current engine constants",
    )
    parser.add_argument(
        "--allow-zero-init",
        action="store_true",
        help="Allow missing/incomplete initial headers to produce zero values",
    )
    parser.add_argument(
        "--maxiter",
        type=int,
        default=500,
        help="Maximum L-BFGS-B iterations",
    )
    parser.add_argument(
        "--ftol",
        type=float,
        default=1e-9,
        help="L-BFGS-B relative function tolerance (float32-safe default: 1e-9)",
    )
    parser.add_argument(
        "--gtol",
        type=float,
        default=1e-6,
        help="L-BFGS-B projected-gradient tolerance (float32-safe default: 1e-6)",
    )
    parser.add_argument(
        "--maxls",
        type=int,
        default=40,
        help="Maximum line-search steps",
    )
    parser.add_argument(
        "--report-every",
        type=int,
        default=10,
        help="Report accepted-iteration loss every N iterations (0 disables)",
    )

    label_group = parser.add_argument_group("labels and reconstruction")
    label_group.add_argument(
        "--soft-labels",
        action="store_true",
        help="Allow arbitrary result labels in [0,1] instead of only 0/0.5/1",
    )
    label_group.add_argument(
        "--require-engine-score",
        action="store_true",
        help="Require an engine_score CSV column and exact reconstruction check",
    )
    label_group.add_argument(
        "--allow-reconstruction-error",
        action="store_true",
        help="Warn instead of aborting when evaluation reconstruction fails",
    )
    label_group.add_argument(
        "--max-reconstruction-mae",
        type=float,
        default=0.1,
        help="Maximum allowed reconstruction mean absolute error in cp",
    )
    label_group.add_argument(
        "--max-reconstruction-error",
        type=float,
        default=2.0,
        help="Maximum allowed individual reconstruction error in cp",
    )

    parameter_group = parser.add_argument_group("parameter constraints")
    parameter_group.add_argument(
        "--pawn-value",
        type=float,
        default=100.0,
        help="Starting/fixed pawn value; default establishes a 100 cp scale",
    )
    parameter_group.add_argument(
        "--unfix-pawn",
        action="store_true",
        help="Allow pawn value to move inside its material bound",
    )
    parameter_group.add_argument(
        "--fix-piece-values",
        action="store_true",
        help="Freeze pawn, knight, bishop, rook and queen at starting values",
    )
    parameter_group.add_argument(
        "--keep-initial-material",
        action="store_true",
        help=(
            "Keep material values from the initial header instead of resetting "
            "to 100/320/330/500/950"
        ),
    )
    parameter_group.add_argument(
        "--no-material-bounds",
        action="store_true",
        help="Disable default reasonable material-value bounds",
    )
    parameter_group.add_argument(
        "--freeze-mobility-zero-buckets",
        action="store_true",
        help=(
            "Freeze mobility bucket 0 at its starting value for each piece/phase. "
            "This prevents drift but does not normalize the bucket to zero."
        ),
    )
    parameter_group.add_argument(
        "--fix-mobility-base",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    parameter_group.add_argument(
        "--min-feature-count",
        type=int,
        default=100,
        help="Freeze parameters appearing in fewer than this many positions",
    )

    regularization_group = parser.add_argument_group("regularization")
    regularization_group.add_argument(
        "--regularization",
        type=float,
        default=1e-4,
        help="L2 strength toward the starting evaluation (0 disables)",
    )
    regularization_group.add_argument(
        "--rarity-reference",
        type=int,
        default=10000,
        help="Coverage count at which regularization reaches its base strength",
    )

    args = parser.parse_args()

    if args.maxiter < 1:
        parser.error("--maxiter must be positive")
    if args.ftol <= 0.0 or args.gtol <= 0.0:
        parser.error("--ftol and --gtol must be positive")
    if args.regularization < 0.0:
        parser.error("--regularization cannot be negative")
    if args.min_feature_count < 0:
        parser.error("--min-feature-count cannot be negative")
    if args.fix_piece_values and args.unfix_pawn:
        parser.error("--fix-piece-values cannot be combined with --unfix-pawn")

    for path, label in (
        (args.input, "training input"),
        (args.validation, "validation input"),
    ):
        if path and not os.path.exists(path):
            parser.error(f"{label} '{path}' not found")

    header_weights = read_initial_weights(
        args.initial_header,
        args.allow_zero_init,
    )

    train = load_csv(args.input, soft_labels=args.soft_labels)
    validation = None
    if args.validation:
        validation = load_csv(args.validation, soft_labels=args.soft_labels)

    print("\nVerifying exporter reconstruction...")
    verify_reconstruction(train, header_weights, args, "Training")
    if validation is not None:
        verify_reconstruction(validation, header_weights, args, "Validation")

    nonzero_count = feature_diagnostics(
        train["features"],
        max(args.min_feature_count, 1),
    )

    w_initial, bounds = build_start_and_bounds(
        header_weights,
        nonzero_count,
        args,
    )
    regularization_center = w_initial.copy()
    regularization_inv_scale_sq = make_regularization_scales(
        nonzero_count,
        args.rarity_reference,
    )

    free_mask = free_parameter_mask(bounds)
    free_count = int(np.count_nonzero(free_mask))
    if free_count == 0 and args.min_feature_count > 1:
        fallback_min = max(1, len(train["features"]) // 20)
        print(f"\nWarning: --min-feature-count {args.min_feature_count} locked all parameters on dataset with {len(train['features'])} positions.")
        print(f"Automatically adjusting --min-feature-count to {fallback_min}.")
        args.min_feature_count = fallback_min
        w_initial, bounds = build_start_and_bounds(
            header_weights,
            nonzero_count,
            args,
        )
        free_mask = free_parameter_mask(bounds)
        free_count = int(np.count_nonzero(free_mask))

    if free_count == 0:
        raise ValueError("All parameters are locked; nothing can be tuned")

    print(f"\nTotal parameters: {NUM_PARAMS}")
    print(f"Free parameters:  {free_count}")
    print(
        f"Starting vector: min={w_initial.min():.0f}, "
        f"max={w_initial.max():.0f}, mean={w_initial.mean():.1f}"
    )

    print("\nStep 1: Finding optimal K for the starting weights...")
    K = optimal_K(train, w_initial)
    print(f"  Optimal K = {K:.8f}")

    initial_train_loss = prediction_loss(w_initial, train, K)
    initial_validation_loss = (
        prediction_loss(w_initial, validation, K)
        if validation is not None
        else None
    )
    print(f"  Initial training loss:   {initial_train_loss:.8f}")
    if initial_validation_loss is not None:
        print(f"  Initial validation loss: {initial_validation_loss:.8f}")

    initial_objective, initial_gradient = loss_func(
        w_initial,
        train["features"],
        train["const_scores"],
        train["results"],
        K,
        regularization_center,
        regularization_inv_scale_sq,
        args.regularization,
    )
    largest_gradient = float(np.max(np.abs(initial_gradient[free_mask])))
    print(f"  Initial regularized objective: {initial_objective:.8f}")
    print(f"  Largest free gradient:         {largest_gradient:.6e}")

    selection_name = "validation" if validation is not None else "training"
    initial_selection_loss = (
        initial_validation_loss
        if validation is not None
        else initial_train_loss
    )
    best_state = {
        "selection_loss": initial_selection_loss,
        "train_loss": initial_train_loss,
        "validation_loss": initial_validation_loss,
        "w": w_initial.copy(),
        "iteration": 0,
    }

    def objective(w):
        return loss_func(
            w,
            train["features"],
            train["const_scores"],
            train["results"],
            K,
            regularization_center,
            regularization_inv_scale_sq,
            args.regularization,
        )

    iteration = [0]

    def record_accepted_iterate(xk, force_report=False):
        iteration[0] += 1
        train_loss = prediction_loss(xk, train, K)
        validation_loss = (
            prediction_loss(xk, validation, K)
            if validation is not None
            else None
        )
        selection_loss = (
            validation_loss if validation is not None else train_loss
        )

        if np.isfinite(selection_loss) and selection_loss < best_state["selection_loss"]:
            best_state.update(
                selection_loss=selection_loss,
                train_loss=train_loss,
                validation_loss=validation_loss,
                w=xk.copy(),
                iteration=iteration[0],
            )

        should_report = (
            force_report
            or (
                args.report_every > 0
                and iteration[0] % args.report_every == 0
            )
        )
        if should_report:
            message = (
                f"  Iteration {iteration[0]}: train={train_loss:.8f}"
            )
            if validation_loss is not None:
                message += f", validation={validation_loss:.8f}"
            message += (
                f", best-{selection_name}="
                f"{best_state['selection_loss']:.8f}"
            )
            print(message)

    print(
        f"\nStep 2: Running L-BFGS-B (maxiter={args.maxiter})... "
        "Press Ctrl+C to save the best accepted iterate"
    )
    started = time.time()

    try:
        result = minimize(
            objective,
            w_initial,
            method="L-BFGS-B",
            jac=True,
            bounds=bounds,
            callback=record_accepted_iterate,
            options={
                "maxiter": args.maxiter,
                "ftol": args.ftol,
                "gtol": args.gtol,
                "maxls": args.maxls,
            },
        )

        elapsed = time.time() - started
        print(f"\n  Optimization completed in {elapsed:.1f}s")
        print(f"  Status: {result.message}")
        print(f"  Optimizer objective: {result.fun:.8f}")
        print(f"  Iterations: {result.nit}, evaluations: {result.nfev}")

        if not np.isfinite(result.fun):
            raise RuntimeError(
                f"Optimization produced a nonfinite objective: {result.message}"
            )
        if not result.success:
            print(f"  Warning: optimizer did not converge: {result.message}")

        final_train = prediction_loss(result.x, train, K)
        final_validation = (
            prediction_loss(result.x, validation, K)
            if validation is not None
            else None
        )
        final_selection = (
            final_validation if validation is not None else final_train
        )
        if np.isfinite(final_selection) and final_selection < best_state["selection_loss"]:
            best_state.update(
                selection_loss=final_selection,
                train_loss=final_train,
                validation_loss=final_validation,
                w=result.x.copy(),
                iteration=result.nit,
            )

    except KeyboardInterrupt:
        print("\n\n" + "=" * 60)
        print("  Ctrl+C detected; selecting the best accepted iterate")
        print("=" * 60)

    improvement = initial_selection_loss - best_state["selection_loss"]
    if improvement <= 1e-12:
        print(
            f"Warning: no meaningful {selection_name} improvement; "
            "retaining starting weights"
        )
        w_best = w_initial.copy()
    else:
        w_best = best_state["w"]
        print(
            f"  Selected iteration {best_state['iteration']} with "
            f"{selection_name} improvement {improvement:.8f}"
        )

    print("\nStep 3: Re-optimizing K for selected weights...")
    K2 = optimal_K(train, w_best)
    print(f"  New K = {K2:.8f} (was {K:.8f})")

    continuous_train_loss = prediction_loss(w_best, train, K2)
    continuous_validation_loss = (
        prediction_loss(w_best, validation, K2)
        if validation is not None
        else None
    )
    w_output = quantize_weights(w_best)
    integer_train_loss = prediction_loss(w_output, train, K2)
    integer_validation_loss = (
        prediction_loss(w_output, validation, K2)
        if validation is not None
        else None
    )

    print(f"  Continuous training loss: {continuous_train_loss:.8f}")
    print(f"  Integer training loss:    {integer_train_loss:.8f}")
    print(
        f"  Training rounding penalty: "
        f"{integer_train_loss - continuous_train_loss:+.8f}"
    )
    if validation is not None:
        print(f"  Continuous validation loss: {continuous_validation_loss:.8f}")
        print(f"  Integer validation loss:    {integer_validation_loss:.8f}")
        print(
            f"  Validation rounding penalty: "
            f"{integer_validation_loss - continuous_validation_loss:+.8f}"
        )

    print("\nStep 4: Writing eval_constants.h...")
    written_weights = write_eval_constants_header(w_best, args.output)

    initial_integer = quantize_weights(w_initial)
    changes = np.abs(written_weights - initial_integer)
    top_changes = np.argsort(changes)[::-1][:20]

    print("\nTop 20 parameter changes:")
    print(f"  {'Parameter':<35} {'Initial':>8} {'Tuned':>8} {'Change':>8}")
    print(f"  {'-' * 35} {'-' * 8} {'-' * 8} {'-' * 8}")
    for idx in top_changes:
        if changes[idx] == 0:
            break
        print(
            f"  {param_names[idx]:<35} "
            f"{initial_integer[idx]:>8} "
            f"{written_weights[idx]:>8} "
            f"{written_weights[idx] - initial_integer[idx]:>+8}"
        )

    print("\nDone. Rebuild the engine and validate strength with an SPRT/match test.")


if __name__ == "__main__":
    main()
