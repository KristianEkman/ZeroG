#!/usr/bin/env python3
"""SPSA search-parameter tuner for the ZeroG chess engine.

This revision fixes integer perturbations, normalizes differently scaled
parameters, validates command-line arguments, makes checkpoints atomic, and
stores the Python RNG state so candidate perturbations survive a resume.
"""

import argparse
import datetime
import json
import math
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time


# Format:
# {
#     parameter_name: {
#         "default": value,
#         "min": value,
#         "max": value,
#         "c": natural_parameter_scale,
#         "a": learning_rate_in_parameter_units,
#         "is_int": bool,
#     }
# }
DEFAULT_PARAMS = {
    "LMR_Base": {
        "default": 2.0,
        "min": 0,
        "max": 20,
        "c": 1.0,
        "a": 5.0,
        "is_int": True,
    },
    "Futility_Margin": {
        "default": 114.0,
        "min": 0,
        "max": 500,
        "c": 10.0,
        "a": 200.0,
        "is_int": True,
    },
    "RFP_Margin": {
        "default": 111.0,
        "min": 0,
        "max": 300,
        "c": 10.0,
        "a": 200.0,
        "is_int": True,
    },
    "NMP_Min_Depth": {
        "default": 2.0,
        "min": 1,
        "max": 10,
        "c": 1.0,
        "a": 4.0,
        "is_int": True,
    },
    "Singular_Margin": {
        "default": 2.0,
        "min": 0,
        "max": 10,
        "c": 1.0,
        "a": 4.0,
        "is_int": True,
    },
    "Aspiration_Window": {
        "default": 35.0,
        "min": 5,
        "max": 200,
        "c": 4.0,
        "a": 40.0,
        "is_int": True,
    },
    "LMR_Min_Depth": {
        "default": 2.0,
        "min": 1,
        "max": 15,
        "c": 1.0,
        "a": 4.0,
        "is_int": True,
    },
    "Futility_Max_Depth": {
        "default": 4.0,
        "min": 1,
        "max": 5,
        "c": 1.0,
        "a": 4.0,
        "is_int": True,
    },
    "LMR_History_Divisor": {
        "default": 2000.0,
        "min": 100,
        "max": 100000,
        "c": 500.0,
        "a": 10000.0,
        "is_int": True,
    },
}


# Standard SPSA decay exponents.
ALPHA = 0.602
GAMMA = 0.101

# Stability constant in the learning-rate denominator.
A_STABILITY = 10.0

# Checkpoint format version.
STATE_FORMAT_VERSION = 2


def positive_int(value):
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return parsed


def positive_even_int(value):
    parsed = positive_int(value)
    if parsed % 2:
        raise argparse.ArgumentTypeError(
            "must be even so every repeated opening has both colors"
        )
    return parsed


def positive_float(value):
    parsed = float(value)
    if not math.isfinite(parsed) or parsed <= 0.0:
        raise argparse.ArgumentTypeError("must be a finite value greater than zero")
    return parsed


def round_half_away_from_zero(value):
    """Round without Python's half-to-even behavior."""
    if value >= 0:
        return math.floor(value + 0.5)
    return math.ceil(value - 0.5)


def nested_lists_to_tuples(value):
    """Convert a JSON-loaded RNG state back to the tuple structure Python expects."""
    if isinstance(value, list):
        return tuple(nested_lists_to_tuples(item) for item in value)
    return value


def validate_parameter_configuration():
    for name, info in DEFAULT_PARAMS.items():
        required = {"default", "min", "max", "c", "a", "is_int"}
        missing = required.difference(info)
        if missing:
            raise ValueError(
                f"Parameter '{name}' is missing fields: {', '.join(sorted(missing))}"
            )

        minimum = info["min"]
        maximum = info["max"]
        default = info["default"]
        natural_scale = info["c"]
        learning_rate = info["a"]

        values = (minimum, maximum, default, natural_scale, learning_rate)
        if any(
            isinstance(value, bool)
            or not isinstance(value, (int, float))
            or not math.isfinite(float(value))
            for value in values
        ):
            raise ValueError(f"Parameter '{name}' contains a non-finite number")

        if minimum >= maximum:
            raise ValueError(f"Parameter '{name}' must have min < max")
        if not minimum <= default <= maximum:
            raise ValueError(f"Default value for '{name}' is outside its bounds")
        if natural_scale <= 0 or learning_rate <= 0:
            raise ValueError(f"Parameter '{name}' must have positive c and a values")

        if info["is_int"] and any(
            float(value) != int(value) for value in (minimum, maximum, default)
        ):
            raise ValueError(
                f"Integer parameter '{name}' needs integer min, max, and default values"
            )


def print_right_aligned_eta(line, eta_h, eta_m, finish_str):
    """Print a Cute Chess line with a right-aligned ETA tag."""
    cols = shutil.get_terminal_size((100, 24)).columns
    clean_line = line.rstrip("\r\n")
    eta_text = f"ETA {eta_h}h {eta_m}m | Finish {finish_str}"
    eta_tag = f"\033[90m[{eta_text}]\033[0m"
    visible_len = len(eta_text) + 2

    if len(clean_line) + visible_len + 1 < cols:
        spaces = cols - len(clean_line) - visible_len
        sys.stdout.write(clean_line + " " * spaces + eta_tag + "\n")
    else:
        sys.stdout.write(clean_line + "  " + eta_tag + "\n")
    sys.stdout.flush()


def resolve_cutechess_path(requested_path):
    """Resolve either an explicit Cute Chess path or a PATH entry."""
    if os.path.isfile(requested_path) and os.access(requested_path, os.X_OK):
        return requested_path

    resolved = shutil.which(requested_path)
    if resolved:
        return resolved

    # Preserve the original script's convenient fallback.
    if requested_path != "cutechess-cli":
        resolved = shutil.which("cutechess-cli")
        if resolved:
            return resolved

    raise FileNotFoundError(
        f"Cute Chess executable not found: {requested_path!r}. "
        "Pass its path with --cutechess."
    )


def terminate_process(process):
    """Terminate a child process, escalating to kill if necessary."""
    if process.poll() is not None:
        return

    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def run_match(
    candidate_a_opts,
    candidate_b_opts,
    games,
    tc,
    concurrency,
    engine_path,
    cutechess_path,
    openings_path,
    opening_plies,
    current_iteration=0,
    total_iterations=1,
    start_iteration=0,
    session_start_time=None,
):
    """Run one complete paired match and return wins A, wins B, and draws."""
    if session_start_time is None:
        session_start_time = time.time()

    cmd = [
        cutechess_path,
        "-engine",
        f"cmd={engine_path}",
        "proto=uci",
        "name=CandidateA",
    ]
    for name, value in candidate_a_opts.items():
        cmd.append(f"option.{name}={value}")

    cmd += [
        "-engine",
        f"cmd={engine_path}",
        "proto=uci",
        "name=CandidateB",
    ]
    for name, value in candidate_b_opts.items():
        cmd.append(f"option.{name}={value}")

    cmd += [
        "-each",
        f"tc={tc}",
        "-games",
        str(games),
        "-openings",
        f"file={openings_path}",
        "format=pgn",
        f"plies={opening_plies}",
        "order=random",
        "-repeat",
        "-concurrency",
        str(concurrency),
    ]

    print(f"Running match: {subprocess.list2cmdline(cmd)}")

    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    wins_a, wins_b, draws = 0, 0, 0
    score_pattern = re.compile(
        r"Score of (\w+) vs (\w+):\s+(\d+)\s+-\s+(\d+)\s+-\s+(\d+)"
    )

    try:
        if process.stdout is None:
            raise RuntimeError("Failed to capture Cute Chess output")

        for line in process.stdout:
            match = score_pattern.search(line)
            if match:
                engine_1 = match.group(1)
                wins_1 = int(match.group(3))
                wins_2 = int(match.group(4))
                current_draws = int(match.group(5))

                if engine_1 == "CandidateA":
                    wins_a, wins_b, draws = wins_1, wins_2, current_draws
                elif engine_1 == "CandidateB":
                    wins_a, wins_b, draws = wins_2, wins_1, current_draws

            games_in_current_match = wins_a + wins_b + draws
            completed_games = (
                (current_iteration - start_iteration) * games
                + games_in_current_match
            )
            total_session_games = (total_iterations - start_iteration) * games
            remaining_games = max(0, total_session_games - completed_games)
            elapsed_seconds = time.time() - session_start_time

            if completed_games > 0 and elapsed_seconds > 0:
                seconds_per_game = elapsed_seconds / completed_games
                eta_seconds = remaining_games * seconds_per_game
                eta_hours = int(eta_seconds // 3600)
                eta_minutes = int((eta_seconds % 3600) // 60)
                finish_time = datetime.datetime.now() + datetime.timedelta(
                    seconds=eta_seconds
                )
                print_right_aligned_eta(
                    line,
                    eta_hours,
                    eta_minutes,
                    finish_time.strftime("%H:%M"),
                )
            else:
                sys.stdout.write(line)
                sys.stdout.flush()

        process.wait()
    except BaseException:
        terminate_process(process)
        raise
    finally:
        if process.stdout is not None:
            process.stdout.close()

    total_games_played = wins_a + wins_b + draws
    if process.returncode != 0 or total_games_played != games:
        raise RuntimeError(
            f"Match failed or incomplete: process exit code {process.returncode}, "
            f"played {total_games_played}/{games} games"
        )

    return wins_a, wins_b, draws


def atomic_save_json(path, value):
    """Atomically replace a JSON checkpoint in the destination directory."""
    absolute_path = os.path.abspath(path)
    directory = os.path.dirname(absolute_path)
    if not os.path.isdir(directory):
        raise FileNotFoundError(
            f"State-file directory does not exist: {directory}"
        )

    descriptor, temporary_path = tempfile.mkstemp(
        prefix=f".{os.path.basename(absolute_path)}.",
        suffix=".tmp",
        dir=directory,
        text=True,
    )

    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as output:
            json.dump(value, output, indent=4)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary_path, absolute_path)
    except BaseException:
        try:
            os.unlink(temporary_path)
        except FileNotFoundError:
            pass
        raise


def make_state(iteration, parameters, history, rng, seed, run_config):
    return {
        "format_version": STATE_FORMAT_VERSION,
        "iteration": iteration,
        "parameters": parameters,
        "history": history,
        "seed": seed,
        # json.dump converts the nested tuples to lists.
        "rng_state": rng.getstate(),
        "run_config": run_config,
    }


def load_state(path):
    with open(path, "r", encoding="utf-8") as source:
        state = json.load(source)

    if not isinstance(state, dict):
        raise ValueError("State file must contain a JSON object")

    iteration = state.get("iteration")
    if isinstance(iteration, bool) or not isinstance(iteration, int) or iteration < 0:
        raise ValueError("State iteration must be a non-negative integer")

    saved_parameters = state.get("parameters")
    if not isinstance(saved_parameters, dict):
        raise ValueError("State parameters must be a JSON object")

    parameters = {
        name: float(info["default"]) for name, info in DEFAULT_PARAMS.items()
    }
    for name, value in saved_parameters.items():
        if name not in DEFAULT_PARAMS:
            continue
        if (
            isinstance(value, bool)
            or not isinstance(value, (int, float))
            or not math.isfinite(float(value))
        ):
            raise ValueError(f"State parameter '{name}' is not a finite number")

        info = DEFAULT_PARAMS[name]
        numeric_value = float(value)
        if not info["min"] <= numeric_value <= info["max"]:
            raise ValueError(
                f"State parameter '{name}'={numeric_value} is outside "
                f"[{info['min']}, {info['max']}]"
            )
        parameters[name] = numeric_value

    history = state.get("history", [])
    if not isinstance(history, list):
        raise ValueError("State history must be a JSON array")

    return state, iteration, parameters, history


def build_argument_parser():
    parser = argparse.ArgumentParser(
        description="SPSA search-parameter tuner for ZeroG"
    )
    parser.add_argument(
        "--games",
        type=positive_even_int,
        default=100,
        help="Even number of games per SPSA iteration (default: 100)",
    )
    parser.add_argument(
        "--iterations",
        type=positive_int,
        default=200,
        help="Number of SPSA iterations (default: 200)",
    )
    parser.add_argument(
        "--concurrency",
        type=positive_int,
        default=4,
        help="Number of concurrent games (default: 4)",
    )
    parser.add_argument(
        "--tc",
        type=str,
        default="10+0.01",
        help="Cute Chess time control, for example 10+0.01",
    )
    parser.add_argument(
        "--lr-factor",
        type=positive_float,
        default=1.0,
        help="Positive multiplier for all learning rates",
    )
    parser.add_argument(
        "--c-factor",
        type=positive_float,
        default=1.0,
        help="Positive multiplier for all perturbation sizes",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=None,
        help="Seed for reproducible SPSA perturbations; generated and saved if omitted",
    )
    parser.add_argument(
        "--resume",
        action="store_true",
        help="Resume from --state-file; missing or invalid state is an error",
    )
    parser.add_argument(
        "--state-file",
        type=str,
        default="spsa_state.json",
        help="JSON checkpoint path",
    )
    parser.add_argument(
        "--engine",
        type=str,
        default="./builds/zerog",
        help="Path to the ZeroG executable",
    )
    parser.add_argument(
        "--cutechess",
        type=str,
        default="../cutechess/cutechess/build/cutechess-cli",
        help="Path or command name for cutechess-cli",
    )
    parser.add_argument(
        "--openings",
        type=str,
        default="games/top_engine_games.pgn",
        help="Opening-suite PGN path",
    )
    parser.add_argument(
        "--opening-plies",
        type=positive_int,
        default=16,
        help="Number of opening plies supplied to each game",
    )
    return parser


def make_run_config(args):
    return {
        "games": args.games,
        "iterations": args.iterations,
        "concurrency": args.concurrency,
        "tc": args.tc,
        "lr_factor": args.lr_factor,
        "c_factor": args.c_factor,
        "engine": args.engine,
        "cutechess": args.cutechess,
        "openings": args.openings,
        "opening_plies": args.opening_plies,
    }


def main():
    validate_parameter_configuration()
    parser = build_argument_parser()
    args = parser.parse_args()

    if not args.tc.strip():
        parser.error("--tc cannot be empty")
    if not args.state_file.strip():
        parser.error("--state-file cannot be empty")
    if not os.path.isfile(args.engine) or not os.access(args.engine, os.X_OK):
        parser.error(
            f"engine executable not found or not executable: {args.engine}"
        )
    if not os.path.isfile(args.openings):
        parser.error(f"opening suite not found: {args.openings}")

    try:
        cutechess_path = resolve_cutechess_path(args.cutechess)
    except FileNotFoundError as error:
        parser.error(str(error))

    parameters = {
        name: float(info["default"]) for name, info in DEFAULT_PARAMS.items()
    }
    history = []
    current_iteration = 0
    run_config = make_run_config(args)

    if args.resume:
        if not os.path.isfile(args.state_file):
            parser.error(f"cannot resume: state file not found: {args.state_file}")

        try:
            state, current_iteration, parameters, history = load_state(
                args.state_file
            )
        except (OSError, ValueError, json.JSONDecodeError) as error:
            parser.error(f"cannot resume from {args.state_file}: {error}")

        if current_iteration > args.iterations:
            parser.error(
                f"state is at iteration {current_iteration}, which exceeds "
                f"--iterations={args.iterations}"
            )

        saved_seed = state.get("seed")
        if isinstance(saved_seed, bool) or not isinstance(saved_seed, int):
            saved_seed = (
                args.seed
                if args.seed is not None
                else random.SystemRandom().randrange(0, 2**63)
            )
            print(
                "Warning: legacy state has no valid RNG seed; exact perturbation "
                "reproduction is unavailable."
            )

        if args.seed is not None and args.seed != saved_seed:
            print(
                f"Warning: ignoring --seed={args.seed}; resumed state uses "
                f"seed {saved_seed}."
            )

        seed = saved_seed
        rng = random.Random(seed)
        saved_rng_state = state.get("rng_state")
        if saved_rng_state is not None:
            try:
                rng.setstate(nested_lists_to_tuples(saved_rng_state))
            except (TypeError, ValueError) as error:
                parser.error(f"state contains an invalid RNG state: {error}")
        else:
            print(
                "Warning: legacy state has no RNG state; continuation is valid "
                "but not bit-for-bit reproducible."
            )

        previous_config = state.get("run_config")
        if isinstance(previous_config, dict):
            changed = [
                key
                for key, value in run_config.items()
                if key in previous_config and previous_config[key] != value
            ]
            if changed:
                print(
                    "Warning: these run settings differ from the checkpoint: "
                    + ", ".join(changed)
                )

        print(f"Resuming from state file: {args.state_file}")
        print(
            f"Resuming at iteration {current_iteration + 1} "
            f"of {args.iterations}"
        )
    else:
        seed = (
            args.seed
            if args.seed is not None
            else random.SystemRandom().randrange(0, 2**63)
        )
        rng = random.Random(seed)
        print("Initializing SPSA parameters...")

    print(f"SPSA perturbation seed: {seed}")

    start_time = time.time()
    start_iteration = current_iteration

    while current_iteration < args.iterations:
        k = current_iteration
        # If this iteration fails, restore this state so --resume retries the
        # same perturbation vector instead of silently skipping it.
        rng_state_before_iteration = rng.getstate()

        print("\n========================================")
        print(f"SPSA Iteration {k + 1} / {args.iterations}")
        print("========================================")

        print("Current Parameter Values:")
        for name, value in parameters.items():
            print(
                f"  {name:<20}: {value:.4f} "
                f"(integer: {round_half_away_from_zero(value)})"
            )

        c_k = {}
        a_k = {}
        deltas = {}
        sent_a_vals = {}
        sent_b_vals = {}
        actual_diffs = {}
        normalized_diffs = {}

        for name, info in DEFAULT_PARAMS.items():
            raw_c = (
                info["c"]
                * args.c_factor
                / math.pow(k + 1, GAMMA)
            )
            c_k[name] = raw_c
            a_k[name] = (
                info["a"]
                * args.lr_factor
                / math.pow(A_STABILITY + k + 1, ALPHA)
            )
            deltas[name] = rng.choice((-1.0, 1.0))

            if info["is_int"]:
                step = max(1, round_half_away_from_zero(raw_c))
                center = round_half_away_from_zero(parameters[name])
                lower = max(int(info["min"]), center - step)
                upper = min(int(info["max"]), center + step)

                if deltas[name] > 0:
                    sent_a_vals[name], sent_b_vals[name] = upper, lower
                else:
                    sent_a_vals[name], sent_b_vals[name] = lower, upper
            else:
                value_a = max(
                    info["min"],
                    min(
                        info["max"],
                        parameters[name] + raw_c * deltas[name],
                    ),
                )
                value_b = max(
                    info["min"],
                    min(
                        info["max"],
                        parameters[name] - raw_c * deltas[name],
                    ),
                )
                sent_a_vals[name] = value_a
                sent_b_vals[name] = value_b

            actual_diffs[name] = sent_a_vals[name] - sent_b_vals[name]
            normalized_diffs[name] = actual_diffs[name] / float(info["c"])

        print("\nPerturbation Details:")
        for name in DEFAULT_PARAMS:
            print(
                f"  {name:<20}: c_k={c_k[name]:.4f}, "
                f"a_k={a_k[name]:.4f}, delta={deltas[name]:+.0f}, "
                f"SentA={sent_a_vals[name]}, SentB={sent_b_vals[name]}, "
                f"Diff={actual_diffs[name]:+g}, "
                f"NormalizedDiff={normalized_diffs[name]:+g}"
            )

        try:
            wins_a, wins_b, draws = run_match(
                sent_a_vals,
                sent_b_vals,
                args.games,
                args.tc,
                args.concurrency,
                args.engine,
                cutechess_path,
                args.openings,
                args.opening_plies,
                current_iteration=k,
                total_iterations=args.iterations,
                start_iteration=start_iteration,
                session_start_time=start_time,
            )
        except KeyboardInterrupt:
            print("\nTuning interrupted. Saving the last completed iteration.")
            rng.setstate(rng_state_before_iteration)
            state = make_state(
                k,
                parameters,
                history,
                rng,
                seed,
                run_config,
            )
            atomic_save_json(args.state_file, state)
            return 130
        except Exception as error:
            print(f"\nError running match: {error}")
            print("Saving the last completed iteration and exiting.")
            rng.setstate(rng_state_before_iteration)
            state = make_state(
                k,
                parameters,
                history,
                rng,
                seed,
                run_config,
            )
            atomic_save_json(args.state_file, state)
            return 1

        total_games_played = wins_a + wins_b + draws
        score_diff = (wins_a - wins_b) / float(total_games_played)
        print(
            f"\nMatch Result: CandidateA wins={wins_a}, "
            f"CandidateB wins={wins_b}, draws={draws} "
            f"(Score Diff={score_diff:+.4f})"
        )

        elapsed_seconds = time.time() - start_time
        completed_iterations = (k + 1) - start_iteration
        average_seconds = elapsed_seconds / completed_iterations
        remaining_iterations = args.iterations - (k + 1)
        eta_seconds = remaining_iterations * average_seconds
        eta_hours = int(eta_seconds // 3600)
        eta_minutes = int((eta_seconds % 3600) // 60)
        finish_time = datetime.datetime.now() + datetime.timedelta(
            seconds=eta_seconds
        )
        print(
            f"[ETA] {remaining_iterations} iterations left "
            f"(~{eta_hours}h {eta_minutes}m remaining). "
            f"Estimated completion: "
            f"{finish_time.strftime('%H:%M (%Y-%m-%d)')}"
        )

        updated_parameters = {}
        changes = {}
        print("\nParameter Updates:")
        for name, info in DEFAULT_PARAMS.items():
            normalized_diff = normalized_diffs[name]
            if normalized_diff != 0:
                # Work in coordinates normalized by each parameter's natural
                # scale. This prevents large-unit parameters, such as
                # LMR_History_Divisor, from receiving vanishingly small moves.
                gradient = score_diff / normalized_diff
                change = a_k[name] * gradient
                new_value = parameters[name] + change
                new_value = max(
                    info["min"],
                    min(info["max"], new_value),
                )
                changes[name] = new_value - parameters[name]
                updated_parameters[name] = new_value
            else:
                print(
                    f"  Note: '{name}' sent identical values "
                    f"({sent_a_vals[name]}); skipping its update."
                )
                changes[name] = 0.0
                updated_parameters[name] = parameters[name]

            print(
                f"  {name:<20}: {parameters[name]:.4f} "
                f"{changes[name]:+.4f} -> {updated_parameters[name]:.4f}"
            )

        parameters = updated_parameters

        history.append(
            {
                "iteration": k + 1,
                "parameters": parameters.copy(),
                "changes": changes,
                "wins_a": wins_a,
                "wins_b": wins_b,
                "draws": draws,
                "score_diff": score_diff,
                "sent_a": sent_a_vals,
                "sent_b": sent_b_vals,
                "actual_diffs": actual_diffs,
                "normalized_diffs": normalized_diffs,
                "c_k": c_k,
                "a_k": a_k,
                "deltas": deltas,
            }
        )

        current_iteration += 1
        state = make_state(
            current_iteration,
            parameters,
            history,
            rng,
            seed,
            run_config,
        )
        atomic_save_json(args.state_file, state)
        print(f"Saved state to {args.state_file}")

    print("\n========================================")
    print("SPSA Optimization Finished Successfully!")
    print("========================================")
    print("Final Tuned Parameters:")
    for name, value in parameters.items():
        print(
            f"  {name:<20}: {value:.4f} "
            f"(integer: {round_half_away_from_zero(value)})"
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
