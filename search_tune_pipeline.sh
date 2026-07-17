#!/bin/bash
# ============================================================
# ZeroG Search Parameter Tuning Pipeline (SPSA)
# ============================================================
#
# Usage: ./search_tune_pipeline.sh [options]
#
# This script automates SPSA tuning of search parameters:
#   1. Build the engine
#   2. Run SPSA optimization via cutechess-cli selfplay
#   3. Apply best parameters to search_globals.c
#   4. Rebuild with tuned parameters
#   5. (Optional) Run a verification match
#
# Tuned parameters:
#   LMR_Base, Futility_Margin, RFP_Margin, NMP_Min_Depth,
#   Singular_Margin, Aspiration_Window, LMR_Min_Depth,
#   Futility_Max_Depth, LMR_History_Divisor
#
# Prerequisites:
#   - cutechess-cli installed (../cutechess/ or on PATH)
#   - Python 3
#   - ZeroG source code in current directory
#   - Opening book: games/top_engine_games.pgn
# ============================================================

set -e  # Exit on error

# Default configuration
SPSA_ITERATIONS=50
SPSA_GAMES=100
CONCURRENCY=$(sysctl -n hw.logicalcpu 2>/dev/null || nproc 2>/dev/null || echo 4)
TC="10+0.01"
LR_FACTOR=1.0
C_FACTOR=1.0
STATE_FILE="spsa_state.json"
SEARCH_GLOBALS="src/search/search_globals.c"
RESUME=false
SKIP_BUILD=false
SKIP_VERIFY=false
VERIFY_GAMES=200

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

print_step() {
    echo -e "\n${BLUE}═══════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  Step $1: $2${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════${NC}\n"
}

print_ok() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warn() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_err() {
    echo -e "${RED}✗ $1${NC}"
}

usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --iterations N      Number of SPSA iterations (default: $SPSA_ITERATIONS)"
    echo "  --games N           Games per SPSA iteration (default: $SPSA_GAMES)"
    echo "  --concurrency N     Concurrent games (default: $CONCURRENCY)"
    echo "  --tc TC             Time control string (default: $TC)"
    echo "  --lr-factor F       Learning rate multiplier (default: $LR_FACTOR)"
    echo "  --c-factor F        Perturbation size multiplier (default: $C_FACTOR)"
    echo "  --state-file FILE   SPSA state file (default: $STATE_FILE)"
    echo "  --resume            Resume from saved SPSA state"
    echo "  --skip-build        Skip initial engine build"
    echo "  --skip-verify       Skip the verification match"
    echo "  --verify-games N    Games for verification match (default: $VERIFY_GAMES)"
    echo "  -h, --help          Show this help"
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --iterations) SPSA_ITERATIONS="$2"; shift 2 ;;
        --games) SPSA_GAMES="$2"; shift 2 ;;
        --concurrency) CONCURRENCY="$2"; shift 2 ;;
        --tc) TC="$2"; shift 2 ;;
        --lr-factor) LR_FACTOR="$2"; shift 2 ;;
        --c-factor) C_FACTOR="$2"; shift 2 ;;
        --state-file) STATE_FILE="$2"; shift 2 ;;
        --resume) RESUME=true; shift ;;
        --skip-build) SKIP_BUILD=true; shift ;;
        --skip-verify) SKIP_VERIFY=true; shift ;;
        --verify-games) VERIFY_GAMES="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

# Record start time
START_TIME=$(date +%s)

echo -e "${CYAN}"
echo "╔═══════════════════════════════════════════════════╗"
echo "║    ZeroG Search Parameter Tuning Pipeline (SPSA) ║"
echo "╚═══════════════════════════════════════════════════╝"
echo -e "${NC}"
echo "Configuration:"
echo "  SPSA iterations:   $SPSA_ITERATIONS"
echo "  Games/iteration:   $SPSA_GAMES"
echo "  Concurrency:       $CONCURRENCY"
echo "  Time control:      $TC"
echo "  LR factor:         $LR_FACTOR"
echo "  C factor:          $C_FACTOR"
echo "  State file:        $STATE_FILE"
echo "  Resume:            $RESUME"
echo "  Skip build:        $SKIP_BUILD"
echo "  Skip verify:       $SKIP_VERIFY"
echo ""

# ============================================================
# Step 1: Build the engine
# ============================================================
if [ "$SKIP_BUILD" = false ]; then
    print_step 1 "Building ZeroG engine"

    # Save a backup of current search_globals.c
    if [ -f "$SEARCH_GLOBALS" ]; then
        cp "$SEARCH_GLOBALS" "${SEARCH_GLOBALS}.bak"
        print_ok "Backed up $SEARCH_GLOBALS"
    fi

    make clean > /dev/null 2>&1 || true
    make release
    print_ok "Engine built successfully"
else
    print_step 1 "Skipping build (using existing engine)"
    if [ ! -f "./builds/zerog" ]; then
        print_err "No engine binary found at ./builds/zerog"
        exit 1
    fi
    print_ok "Using existing ./builds/zerog"
fi

# ============================================================
# Step 2: Verify prerequisites
# ============================================================
print_step 2 "Checking prerequisites"

# Check for Python 3
if ! command -v python3 &> /dev/null; then
    print_err "python3 not found. Please install Python 3."
    exit 1
fi
print_ok "Python 3 found"

# Check for spsa.py
if [ ! -f "spsa.py" ]; then
    print_err "spsa.py not found in current directory."
    exit 1
fi
print_ok "spsa.py found"

# Check for opening book
if [ ! -f "games/top_engine_games.pgn" ]; then
    print_err "Opening book 'games/top_engine_games.pgn' not found."
    exit 1
fi
print_ok "Opening book found"

# Check for cutechess-cli
CUTECHESS_FOUND=false
if [ -x "../cutechess/cutechess/build/cutechess-cli" ]; then
    CUTECHESS_FOUND=true
    print_ok "cutechess-cli found at ../cutechess/cutechess/build/cutechess-cli"
elif command -v cutechess-cli &> /dev/null; then
    CUTECHESS_FOUND=true
    print_ok "cutechess-cli found on PATH"
fi

if [ "$CUTECHESS_FOUND" = false ]; then
    print_err "cutechess-cli not found. Please build it or add to PATH."
    exit 1
fi

# Print current search parameter values from source
echo ""
echo "Current search parameters (from $SEARCH_GLOBALS):"
grep -E "^int (lmr_base|futility_margin|rfp_margin_base|nmp_min_depth|singular_margin|aspiration_window|lmr_min_depth|futility_max_depth|lmr_history_divisor) =" "$SEARCH_GLOBALS" | \
    while read -r line; do
        echo "  $line"
    done

# ============================================================
# Step 3: Run SPSA optimization
# ============================================================
print_step 3 "Running SPSA optimization ($SPSA_ITERATIONS iterations × $SPSA_GAMES games)"

SPSA_CMD="python3 spsa.py \
    --iterations $SPSA_ITERATIONS \
    --games $SPSA_GAMES \
    --concurrency $CONCURRENCY \
    --tc $TC \
    --lr-factor $LR_FACTOR \
    --c-factor $C_FACTOR \
    --state-file $STATE_FILE"

if [ "$RESUME" = true ]; then
    SPSA_CMD="$SPSA_CMD --resume"
fi

echo "Command: $SPSA_CMD"
echo ""

eval $SPSA_CMD

if [ $? -ne 0 ]; then
    print_err "SPSA optimization failed or was interrupted."
    print_warn "State saved to $STATE_FILE — use --resume to continue."
    exit 1
fi

print_ok "SPSA optimization complete"

# ============================================================
# Step 4: Apply tuned parameters to source
# ============================================================
print_step 4 "Applying tuned parameters to $SEARCH_GLOBALS"

if [ ! -f "$STATE_FILE" ]; then
    print_err "State file '$STATE_FILE' not found!"
    exit 1
fi

# Extract final parameters from the SPSA state JSON
echo "Reading tuned values from $STATE_FILE..."

# Use Python to extract and apply the parameters
python3 - "$STATE_FILE" "$SEARCH_GLOBALS" << 'APPLY_SCRIPT'
import json
import re
import sys

state_file = sys.argv[1]
globals_file = sys.argv[2]

with open(state_file, "r") as f:
    state = json.load(f)

params = state["parameters"]

# Map SPSA parameter names to C variable names
param_map = {
    "LMR_Base": "lmr_base",
    "Futility_Margin": "futility_margin",
    "RFP_Margin": "rfp_margin_base",
    "NMP_Min_Depth": "nmp_min_depth",
    "Singular_Margin": "singular_margin",
    "Aspiration_Window": "aspiration_window",
    "LMR_Min_Depth": "lmr_min_depth",
    "Futility_Max_Depth": "futility_max_depth",
    "LMR_History_Divisor": "lmr_history_divisor",
}

with open(globals_file, "r") as f:
    content = f.read()

changes = 0
for spsa_name, c_name in param_map.items():
    if spsa_name in params:
        rounded_val = int(round(params[spsa_name]))
        # Match the C variable declaration line
        pattern = rf"(int {c_name} = )\d+;"
        new_line = rf"\g<1>{rounded_val};"
        new_content, n = re.subn(pattern, new_line, content)
        if n > 0:
            content = new_content
            changes += 1
            print(f"  {c_name:25s} = {rounded_val} (from {params[spsa_name]:.4f})")
        else:
            print(f"  WARNING: Could not find '{c_name}' in {globals_file}")

with open(globals_file, "w") as f:
    f.write(content)

print(f"\nApplied {changes} parameter updates to {globals_file}")
APPLY_SCRIPT

if [ $? -ne 0 ]; then
    print_err "Failed to apply parameters."
    exit 1
fi

print_ok "Parameters applied to source"

# ============================================================
# Step 5: Rebuild with tuned parameters
# ============================================================
print_step 5 "Rebuilding engine with tuned parameters"

make clean > /dev/null 2>&1 || true
make release
print_ok "Engine rebuilt with tuned parameters"

# ============================================================
# Step 6: Verification match (optional)
# ============================================================
if [ "$SKIP_VERIFY" = false ]; then
    print_step 6 "Running verification match ($VERIFY_GAMES games)"

    # The new engine (tuned) vs the old backup
    # Copy the backed-up binary as reference
    if [ -f "zerog_prev" ]; then
        print_ok "Using ./zerog_prev as baseline"
        python3 selfplay.py \
            --games "$VERIFY_GAMES" \
            --concurrency "$CONCURRENCY" \
            --tc "$TC"
    else
        print_warn "No zerog_prev found for verification match."
        print_warn "Copy the old engine as zerog_prev and run selfplay.py manually."
    fi
else
    print_step 6 "Skipping verification match"
fi

# ============================================================
# Summary
# ============================================================
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
MINUTES=$((ELAPSED / 60))
SECONDS=$((ELAPSED % 60))

echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   Search Tuning Pipeline Complete!                ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════╝${NC}"
echo ""
echo "  Total time:   ${MINUTES}m ${SECONDS}s"
echo "  Source:        $SEARCH_GLOBALS"
echo "  Backup:        ${SEARCH_GLOBALS}.bak"
echo "  SPSA state:    $STATE_FILE"
echo ""
echo "  Final tuned values:"
grep -E "^int (lmr_base|futility_margin|rfp_margin_base|nmp_min_depth|singular_margin|aspiration_window|lmr_min_depth|futility_max_depth|lmr_history_divisor) =" "$SEARCH_GLOBALS" | \
    while read -r line; do
        echo "    $line"
    done
echo ""
echo "  Next steps:"
echo "    1. Test the new engine: ./builds/zerog"
echo "    2. Compare vs old: diff ${SEARCH_GLOBALS}.bak $SEARCH_GLOBALS"
echo "    3. Run a longer tournament to measure Elo gain"
echo "    4. If results regress, restore: cp ${SEARCH_GLOBALS}.bak $SEARCH_GLOBALS && make release"
echo ""
