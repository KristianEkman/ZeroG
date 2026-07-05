#!/usr/bin/env python3
import argparse
import json
import math
import os
import random
import re
import subprocess
import sys

# Default search parameters to tune
# Format: { param_name: { "default": val, "min": val, "max": val, "c": perturbation_step, "a": learning_rate } }
DEFAULT_PARAMS = {
    "LMR_Base": {"default": 2.0, "min": 0, "max": 20, "c": 0.5, "a": 0.05},
    "Futility_Margin": {"default": 114.0, "min": 0, "max": 500, "c": 10.0, "a": 1.0},
    "RFP_Margin": {"default": 111.0, "min": 0, "max": 300, "c": 10.0, "a": 1.0},
    "NMP_Min_Depth": {"default": 2.0, "min": 1, "max": 10, "c": 0.5, "a": 0.05},
    "Singular_Margin": {"default": 2.0, "min": 0, "max": 10, "c": 0.5, "a": 0.05},
    "Aspiration_Window": {"default": 35.0, "min": 5, "max": 200, "c": 4.0, "a": 0.4},
    "LMR_Min_Depth": {"default": 2.0, "min": 1, "max": 15, "c": 0.5, "a": 0.05},
    "Futility_Max_Depth": {"default": 4.0, "min": 1, "max": 5, "c": 0.5, "a": 0.05},
    "LMR_History_Divisor": {"default": 2000.0, "min": 100, "max": 100000, "c": 500.0, "a": 50.0},
}

# SPSA Hyperparameters
ALPHA = 0.602
GAMMA = 0.101
A_STEP = 10.0

def run_match(candidate_a_opts, candidate_b_opts, games, tc, concurrency):
    """
    Launches cutechess-cli to play a match between Candidate A and Candidate B.
    Parses stdout in real-time to retrieve the final score.
    """
    cutechess_cli_path = "../cutechess/cutechess/build/cutechess-cli"
    if not os.path.exists(cutechess_cli_path):
        # Check if cutechess-cli is in path
        cutechess_cli_path = "cutechess-cli"

    cmd = [
        cutechess_cli_path,
        "-engine", "cmd=./builds/zerog", "proto=uci", "name=CandidateA"
    ]
    for name, val in candidate_a_opts.items():
        cmd.append(f"option.{name}={int(round(val))}")

    cmd += [
        "-engine", "cmd=./builds/zerog", "proto=uci", "name=CandidateB"
    ]
    for name, val in candidate_b_opts.items():
        cmd.append(f"option.{name}={int(round(val))}")

    cmd += [
        "-each", f"tc={tc}",
        "-games", str(games),
        "-openings", "file=./grand_master_openings.epd", "format=epd", "order=random",
        "-repeat",
        "-concurrency", str(concurrency)
    ]

    print(f"Running match: {' '.join(cmd)}")
    
    # Run subprocess and parse output in real-time
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    
    wins_a, wins_b, draws = 0, 0, 0
    score_pattern = re.compile(r"Score of (\w+) vs (\w+):\s+(\d+)\s+-\s+(\d+)\s+-\s+(\d+)")

    for line in process.stdout:
        sys.stdout.write(line)
        sys.stdout.flush()
        
        match = score_pattern.search(line)
        if match:
            eng1, eng2, w1, w2, dr = match.group(1), match.group(2), int(match.group(3)), int(match.group(4)), int(match.group(5))
            if eng1 == "CandidateA":
                wins_a, wins_b, draws = w1, w2, dr
            elif eng1 == "CandidateB":
                wins_a, wins_b, draws = w2, w1, dr

    process.wait()
    
    total_games_played = wins_a + wins_b + draws
    if process.returncode != 0 and total_games_played == 0:
        raise RuntimeError(f"cutechess-cli failed with return code {process.returncode}")
        
    return wins_a, wins_b, draws

def main():
    parser = argparse.ArgumentParser(description="SPSA Search Parameter Tuner for ZeroG")
    parser.add_argument("--games", type=int, default=100, help="Number of games per SPSA iteration")
    parser.add_argument("--iterations", type=int, default=50, help="Number of SPSA iterations to run")
    parser.add_argument("--concurrency", type=int, default=4, help="Number of concurrent games")
    parser.add_argument("--tc", type=type(""), default="10+0.01", help="Time control for cutechess-cli (e.g. 10+0.01)")
    parser.add_argument("--lr-factor", type=float, default=1.0, help="Multiplier factor for parameter learning rates")
    parser.add_argument("--c-factor", type=float, default=1.0, help="Multiplier factor for parameter perturbation sizes")
    parser.add_argument("--resume", action="store_true", help="Resume tuning from saved state")
    parser.add_argument("--state-file", type=type(""), default="spsa_state.json", help="Path to JSON state file")
    args = parser.parse_args()

    # Verify that the zerog executable exists
    if not os.path.exists("./builds/zerog"):
        print("Error: ./builds/zerog not found. Please compile the engine first.")
        sys.exit(1)

    # Initialize parameters with defaults
    parameters = {name: info["default"] for name, info in DEFAULT_PARAMS.items()}

    # Initialize or load state
    if args.resume and os.path.exists(args.state_file):
        print(f"Resuming from state file: {args.state_file}")
        with open(args.state_file, "r") as f:
            state = json.load(f)
        current_iteration = state["iteration"]
        # Merge loaded parameters with defaults for new parameter support
        for name, val in state["parameters"].items():
            if name in parameters:
                parameters[name] = val
        history = state["history"]
        print(f"Resuming at iteration {current_iteration + 1} of {args.iterations}")
    else:
        print("Initializing SPSA parameters...")
        current_iteration = 0
        history = []

    # Run SPSA loop
    while current_iteration < args.iterations:
        k = current_iteration
        print(f"\n========================================")
        print(f"SPSA Iteration {k + 1} / {args.iterations}")
        print(f"========================================")
        
        # 1. Print current parameter values
        print("Current Parameter Values:")
        for name, val in parameters.items():
            print(f"  {name:<20}: {val:.4f} (rounded: {int(round(val))})")
            
        # 2. Compute perturbation sizes (c_k) and learning rates (a_k)
        c_k = {}
        a_k = {}
        delta = {}
        
        candidate_a_vals = {}
        candidate_b_vals = {}
        
        for name, info in DEFAULT_PARAMS.items():
            # c_k = c_i * args.c_factor / (k + 1)^gamma
            c_k[name] = (info["c"] * args.c_factor) / math.pow(k + 1, GAMMA)
            # a_k = a_i * args.lr_factor / (A_STEP + k + 1)^alpha
            a_k[name] = (info["a"] * args.lr_factor) / math.pow(A_STEP + k + 1, ALPHA)
            
            # Generate random perturbation (+1 or -1)
            delta[name] = random.choice([-1.0, 1.0])
            
            # Compute Candidate A and B values, clamped to bounds
            candidate_a_vals[name] = max(info["min"], min(info["max"], parameters[name] + c_k[name] * delta[name]))
            candidate_b_vals[name] = max(info["min"], min(info["max"], parameters[name] - c_k[name] * delta[name]))

        print("\nPerturbation Details:")
        for name in DEFAULT_PARAMS:
            print(f"  {name:<20}: c_k={c_k[name]:.4f}, a_k={a_k[name]:.6f}, delta={delta[name]}, A_val={int(round(candidate_a_vals[name]))}, B_val={int(round(candidate_b_vals[name]))}")

        # 3. Play Match
        try:
            wins_a, wins_b, draws = run_match(
                candidate_a_vals, 
                candidate_b_vals, 
                args.games, 
                args.tc, 
                args.concurrency
            )
        except Exception as e:
            print(f"\nError running match: {e}")
            print("Saving state and exiting.")
            # Save state before exiting
            state = {
                "iteration": k,
                "parameters": parameters,
                "history": history
            }
            with open(args.state_file, "w") as f:
                json.dump(state, f, indent=4)
            sys.exit(1)

        # 4. SPSA Parameter Update
        # wins_a - wins_b represents the performance difference (S_A - S_B)
        perf_diff = wins_a - wins_b
        print(f"\nMatch Result: CandidateA wins={wins_a}, CandidateB wins={wins_b}, draws={draws} (Perf Diff={perf_diff})")
        
        updated_parameters = {}
        for name, info in DEFAULT_PARAMS.items():
            # Update formula: w_new = w_old + a_k * (perf_diff / (2.0 * c_k)) * delta
            change = a_k[name] * (perf_diff / (2.0 * c_k[name])) * delta[name]
            new_val = parameters[name] + change
            # Clamp to parameter bounds
            updated_parameters[name] = max(info["min"], min(info["max"], new_val))

        # Update current parameters
        parameters = updated_parameters
        
        # Record history
        iter_log = {
            "iteration": k + 1,
            "parameters": parameters.copy(),
            "wins_a": wins_a,
            "wins_b": wins_b,
            "draws": draws,
            "perf_diff": perf_diff,
            "deltas": delta,
            "c_k": c_k,
            "a_k": a_k
        }
        history.append(iter_log)
        
        # Save state
        state = {
            "iteration": k + 1,
            "parameters": parameters,
            "history": history
        }
        with open(args.state_file, "w") as f:
            json.dump(state, f, indent=4)
        print(f"Saved state to {args.state_file}")
            
        current_iteration += 1

    print("\n========================================")
    print("SPSA Optimization Finished Successfully!")
    print("========================================")
    print("Final Tuned Parameters:")
    for name, val in parameters.items():
        print(f"  {name:<20}: {val:.4f} (rounded: {int(round(val))})")

if __name__ == "__main__":
    main()
