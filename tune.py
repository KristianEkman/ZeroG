import csv
import numpy as np
from scipy.optimize import minimize_scalar, minimize

# Parameter names and initial values matching tune_export.c exactly
param_names = [
    # Piece values
    "piece_pawn", "piece_knight", "piece_bishop", "piece_rook", "piece_queen",
    # Rook open file
    "rook_open_file_mg", "rook_open_file_eg", "rook_semi_open_file_mg", "rook_semi_open_file_eg",
    # Bishop pair
    "bishop_pair_bonus",
    # Pawn structure
    "double_pawn_mg", "double_pawn_eg",
    "isolated_pawn_mg", "isolated_pawn_eg",
    "isolated_pawn_semi_open_mg", "isolated_pawn_semi_open_eg",
    # Passed pawns MG
    "passed_pawn_mg_r1", "passed_pawn_mg_r2", "passed_pawn_mg_r3", "passed_pawn_mg_r4", "passed_pawn_mg_r5", "passed_pawn_mg_r6",
    # Passed pawns EG
    "passed_pawn_eg_r1", "passed_pawn_eg_r2", "passed_pawn_eg_r3", "passed_pawn_eg_r4", "passed_pawn_eg_r5", "passed_pawn_eg_r6",
    # Passed pawns extra
    "passed_pawn_defended_mg", "passed_pawn_defended_eg",
    "passed_pawn_friendly_behind_mg", "passed_pawn_friendly_behind_eg",
    "passed_pawn_enemy_behind_mg", "passed_pawn_enemy_behind_eg",
    # Knight mobility MG
    "knight_mobility_mg_0", "knight_mobility_mg_1", "knight_mobility_mg_2", "knight_mobility_mg_3", "knight_mobility_mg_4", "knight_mobility_mg_5", "knight_mobility_mg_6", "knight_mobility_mg_7", "knight_mobility_mg_8",
    # Knight mobility EG
    "knight_mobility_eg_0", "knight_mobility_eg_1", "knight_mobility_eg_2", "knight_mobility_eg_3", "knight_mobility_eg_4", "knight_mobility_eg_5", "knight_mobility_eg_6", "knight_mobility_eg_7", "knight_mobility_eg_8",
    # Bishop mobility MG
    "bishop_mobility_mg_0", "bishop_mobility_mg_1", "bishop_mobility_mg_2", "bishop_mobility_mg_3", "bishop_mobility_mg_4", "bishop_mobility_mg_5", "bishop_mobility_mg_6", "bishop_mobility_mg_7", "bishop_mobility_mg_8", "bishop_mobility_mg_9", "bishop_mobility_mg_10", "bishop_mobility_mg_11", "bishop_mobility_mg_12", "bishop_mobility_mg_13",
    # Bishop mobility EG
    "bishop_mobility_eg_0", "bishop_mobility_eg_1", "bishop_mobility_eg_2", "bishop_mobility_eg_3", "bishop_mobility_eg_4", "bishop_mobility_eg_5", "bishop_mobility_eg_6", "bishop_mobility_eg_7", "bishop_mobility_eg_8", "bishop_mobility_eg_9", "bishop_mobility_eg_10", "bishop_mobility_eg_11", "bishop_mobility_eg_12", "bishop_mobility_eg_13",
    # Rook mobility MG
    "rook_mobility_mg_0", "rook_mobility_mg_1", "rook_mobility_mg_2", "rook_mobility_mg_3", "rook_mobility_mg_4", "rook_mobility_mg_5", "rook_mobility_mg_6", "rook_mobility_mg_7", "rook_mobility_mg_8", "rook_mobility_mg_9", "rook_mobility_mg_10", "rook_mobility_mg_11", "rook_mobility_mg_12", "rook_mobility_mg_13", "rook_mobility_mg_14",
    # Rook mobility EG
    "rook_mobility_eg_0", "rook_mobility_eg_1", "rook_mobility_eg_2", "rook_mobility_eg_3", "rook_mobility_eg_4", "rook_mobility_eg_5", "rook_mobility_eg_6", "rook_mobility_eg_7", "rook_mobility_eg_8", "rook_mobility_eg_9", "rook_mobility_eg_10", "rook_mobility_eg_11", "rook_mobility_eg_12", "rook_mobility_eg_13", "rook_mobility_eg_14",
    # Queen mobility MG
    "queen_mobility_mg_0", "queen_mobility_mg_1", "queen_mobility_mg_2", "queen_mobility_mg_3", "queen_mobility_mg_4", "queen_mobility_mg_5", "queen_mobility_mg_6", "queen_mobility_mg_7", "queen_mobility_mg_8", "queen_mobility_mg_9", "queen_mobility_mg_10", "queen_mobility_mg_11", "queen_mobility_mg_12", "queen_mobility_mg_13", "queen_mobility_mg_14", "queen_mobility_mg_15", "queen_mobility_mg_16", "queen_mobility_mg_17", "queen_mobility_mg_18", "queen_mobility_mg_19", "queen_mobility_mg_20", "queen_mobility_mg_21", "queen_mobility_mg_22", "queen_mobility_mg_23", "queen_mobility_mg_24", "queen_mobility_mg_25", "queen_mobility_mg_26", "queen_mobility_mg_27",
    # Queen mobility EG
    "queen_mobility_eg_0", "queen_mobility_eg_1", "queen_mobility_eg_2", "queen_mobility_eg_3", "queen_mobility_eg_4", "queen_mobility_eg_5", "queen_mobility_eg_6", "queen_mobility_eg_7", "queen_mobility_eg_8", "queen_mobility_eg_9", "queen_mobility_eg_10", "queen_mobility_eg_11", "queen_mobility_eg_12", "queen_mobility_eg_13", "queen_mobility_eg_14", "queen_mobility_eg_15", "queen_mobility_eg_16", "queen_mobility_eg_17", "queen_mobility_eg_18", "queen_mobility_eg_19", "queen_mobility_eg_20", "queen_mobility_eg_21", "queen_mobility_eg_22", "queen_mobility_eg_23", "queen_mobility_eg_24", "queen_mobility_eg_25", "queen_mobility_eg_26", "queen_mobility_eg_27"
]

w_initial = np.array([
    # Piece values
    100, 320, 330, 500, 900,
    # Rook open file
    20, 15, 10, 7,
    # Bishop pair
    50,
    # Pawn structure
    8, 12, 5, 8, 10, 15,
    # Passed pawns MG
    5, 10, 20, 35, 60, 100,
    # Passed pawns EG
    8, 15, 30, 60, 110, 180,
    # Passed pawns extra
    15, 25, 20, 40, -15, -30,
    # Knight mobility MG
    -20, -10, -5, 0, 5, 10, 15, 20, 22,
    # Knight mobility EG
    -20, -10, -5, 0, 5, 10, 15, 20, 22,
    # Bishop mobility MG
    -20, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50,
    # Bishop mobility EG
    -20, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50,
    # Rook mobility MG
    -20, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55,
    # Rook mobility EG
    -25, -15, -5, 0, 10, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65,
    # Queen mobility MG
    -10, -5, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50,
    # Queen mobility EG
    -20, -15, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115
], dtype=np.float64)

def load_data(filename):
    print(f"First pass: counting rows in {filename}...")
    with open(filename, "r") as f:
        header = f.readline().strip().split(",")
        num_rows = sum(1 for line in f)

    csv_param_names = header[2:]
    num_params = len(csv_param_names)
    assert num_params == len(param_names), f"CSV has {num_params} columns but script expects {len(param_names)}."

    print(f"Allocating memory for {num_rows} entries...")
    results = np.zeros(num_rows, dtype=np.float64)
    const_scores = np.zeros(num_rows, dtype=np.float64)
    features = np.zeros((num_rows, num_params), dtype=np.float64)

    print("Second pass: reading dataset...")
    with open(filename, "r") as f:
        f.readline()  # skip header
        for idx, line in enumerate(f):
            parts = line.strip().split(",")
            results[idx] = float(parts[0])
            const_scores[idx] = float(parts[1])
            features[idx, :] = [float(x) for x in parts[2:]]

    print(f"Loaded {num_rows} positions successfully.")
    return results, const_scores, features

def k_loss(K_val, scores, results):
    sig = K_val * scores / 400.0
    E = 1.0 / (1.0 + 10.0**(-sig))
    return np.mean((results - E)**2)

def loss_func(w, features, const_scores, results, K):
    scores = const_scores + features @ w
    sig = K * scores / 400.0
    E = 1.0 / (1.0 + 10.0**(-sig))
    loss = np.mean((results - E)**2)

    # Analytical gradient:
    # d_i = (results - E) * E * (1 - E)
    # grad = - (2 * ln(10) * K / (400 * N)) * F^T @ d
    d = (results - E) * E * (1.0 - E)
    grad = - (2.0 * np.log(10) * K / (400.0 * len(results))) * (features.T @ d)

    return loss, grad

def write_eval_constants_header(w):
    filename = "src/eval/eval_constants.h"
    print(f"Writing tuned parameters directly to {filename}...")
    with open(filename, "w") as f:
        f.write("// Generated automatically by tune.py\n")
        f.write("#ifndef EVAL_CONSTANTS_H\n")
        f.write("#define EVAL_CONSTANTS_H\n\n")

        # 1. Piece values
        f.write("// Piece values\n")
        f.write(f"#define PIECE_PAWN_VAL {w[0]}\n")
        f.write(f"#define PIECE_KNIGHT_VAL {w[1]}\n")
        f.write(f"#define PIECE_BISHOP_VAL {w[2]}\n")
        f.write(f"#define PIECE_ROOK_VAL {w[3]}\n")
        f.write(f"#define PIECE_QUEEN_VAL {w[4]}\n\n")

        # 2. Rook open file
        f.write("// Rook open file\n")
        f.write(f"#define ROOK_OPEN_FILE_MG_VAL {w[5]}\n")
        f.write(f"#define ROOK_OPEN_FILE_EG_VAL {w[6]}\n")
        f.write(f"#define ROOK_SEMI_OPEN_FILE_MG_VAL {w[7]}\n")
        f.write(f"#define ROOK_SEMI_OPEN_FILE_EG_VAL {w[8]}\n\n")

        # 3. Bishop pair
        f.write("// Bishop pair\n")
        f.write(f"#define BISHOP_PAIR_BONUS_VAL {w[9]}\n\n")

        # 4. Pawn structure
        f.write("// Pawn structure\n")
        f.write(f"#define DOUBLE_PAWN_MG_VAL {w[10]}\n")
        f.write(f"#define DOUBLE_PAWN_EG_VAL {w[11]}\n")
        f.write(f"#define ISOLATED_PAWN_MG_VAL {w[12]}\n")
        f.write(f"#define ISOLATED_PAWN_EG_VAL {w[13]}\n")
        f.write(f"#define ISOLATED_PAWN_SEMI_OPEN_MG_VAL {w[14]}\n")
        f.write(f"#define ISOLATED_PAWN_SEMI_OPEN_EG_VAL {w[15]}\n\n")

        # 5. Passed pawns MG
        f.write("// Passed pawns MG\n")
        for i in range(6):
            f.write(f"#define PASSED_PAWN_MG_R{i+1}_VAL {w[16+i]}\n")
        f.write("\n")

        # 6. Passed pawns EG
        f.write("// Passed pawns EG\n")
        for i in range(6):
            f.write(f"#define PASSED_PAWN_EG_R{i+1}_VAL {w[22+i]}\n")
        f.write("\n")

        # 7. Passed pawns extra
        f.write("// Passed pawns extra\n")
        f.write(f"#define PASSED_PAWN_DEFENDED_MG_VAL {w[28]}\n")
        f.write(f"#define PASSED_PAWN_DEFENDED_EG_VAL {w[29]}\n")
        f.write(f"#define PASSED_PAWN_FRIENDLY_BEHIND_MG_VAL {w[30]}\n")
        f.write(f"#define PASSED_PAWN_FRIENDLY_BEHIND_EG_VAL {w[31]}\n")
        f.write(f"#define PASSED_PAWN_ENEMY_BEHIND_MG_VAL {w[32]}\n")
        f.write(f"#define PASSED_PAWN_ENEMY_BEHIND_EG_VAL {w[33]}\n\n")

        # 8. Knight mobility
        f.write("// Knight mobility MG\n")
        for i in range(9):
            f.write(f"#define KNIGHT_MOBILITY_MG_{i}_VAL {w[34+i]}\n")
        f.write("\n// Knight mobility EG\n")
        for i in range(9):
            f.write(f"#define KNIGHT_MOBILITY_EG_{i}_VAL {w[43+i]}\n")
        f.write("\n")

        # 9. Bishop mobility
        f.write("// Bishop mobility MG\n")
        for i in range(14):
            f.write(f"#define BISHOP_MOBILITY_MG_{i}_VAL {w[52+i]}\n")
        f.write("\n// Bishop mobility EG\n")
        for i in range(14):
            f.write(f"#define BISHOP_MOBILITY_EG_{i}_VAL {w[66+i]}\n")
        f.write("\n")

        # 10. Rook mobility
        f.write("// Rook mobility MG\n")
        for i in range(15):
            f.write(f"#define ROOK_MOBILITY_MG_{i}_VAL {w[80+i]}\n")
        f.write("\n// Rook mobility EG\n")
        for i in range(15):
            f.write(f"#define ROOK_MOBILITY_EG_{i}_VAL {w[95+i]}\n")
        f.write("\n")

        # 11. Queen mobility
        f.write("// Queen mobility MG\n")
        for i in range(28):
            f.write(f"#define QUEEN_MOBILITY_MG_{i}_VAL {w[110+i]}\n")
        f.write("\n// Queen mobility EG\n")
        for i in range(28):
            f.write(f"#define QUEEN_MOBILITY_EG_{i}_VAL {w[138+i]}\n")
        f.write("\n")

        f.write("#endif // EVAL_CONSTANTS_H\n")
    print(f"Header file successfully created.")

def main():
    csv_file = "quiet_traindata_features.csv"
    results, const_scores, features = load_data(csv_file)

    # 1. Optimize K on initial parameters
    print("Finding initial optimal scaling factor K...")
    scores_initial = const_scores + features @ w_initial
    res_k = minimize_scalar(k_loss, bounds=(0.1, 10.0), args=(scores_initial, results), method='bounded')
    K = res_k.x
    print(f"Initial scaling factor K = {K:.6f}")
    
    initial_mse = k_loss(K, scores_initial, results)
    print(f"Initial MSE loss = {initial_mse:.8f}")

    # 2. Run numerical optimizer (L-BFGS-B) with analytical gradient
    print("Running parameter optimization using L-BFGS-B...")
    
    # Optional parameter bounds (e.g. piece values must be positive, etc.)
    # We will use unconstrained L-BFGS-B first, as it's standard and robust
    res = minimize(
        loss_func, 
        w_initial, 
        args=(features, const_scores, results, K), 
        jac=True, 
        method='L-BFGS-B', 
        options={'maxiter': 500, 'disp': True}
    )
    
    w_opt = res.x
    final_mse = res.fun
    print(f"Optimization complete. Final MSE loss = {final_mse:.8f}")
    print(f"MSE improvement: {initial_mse - final_mse:.8f} ({(initial_mse - final_mse)/initial_mse*100:.2f}%)")

    # 3. Normalize parameters so that Piece Value of Pawn is exactly 100
    pawn_opt = w_opt[0]
    print(f"Optimized Pawn piece value (before scaling): {pawn_opt:.4f}")
    
    scale_factor = 100.0 / pawn_opt
    w_scaled = w_opt * scale_factor
    w_final = np.round(w_scaled).astype(int)
    
    print("\nParameter Tuning Results:")
    print(f"{'Parameter Name':<32} {'Initial':<10} {'Optimized':<10} {'Final (Scaled)':<15}")
    print("-" * 72)
    for idx, name in enumerate(param_names):
        print(f"{name:<32} {int(w_initial[idx]):<10} {w_opt[idx]:<10.2f} {w_final[idx]:<15}")
    print("-" * 72)

    # 4. Export to C Header
    write_eval_constants_header(w_final)

if __name__ == "__main__":
    main()
