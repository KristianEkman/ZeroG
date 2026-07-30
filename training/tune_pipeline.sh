#!/bin/bash
# ============================================================
# ZeroG End-to-End Tuning Pipeline
# ============================================================
#
# Usage: ./tune_pipeline.sh [options]
#
# This script automates the full Texel tuning workflow:
#   1. Build the engine
#   2. Generate selfplay games (if no positions file exists)
#   3. Filter quiet positions
#   4. Evaluate positions with Stockfish
#   5. Export features to CSV
#   6. Run L-BFGS-B optimization
#   7. Rebuild with new constants
#   8. (Optional) Run regression selfplay match
#
# Prerequisites:
#   - Stockfish installed and accessible
#   - Python 3 with numpy and scipy
#   - ZeroG source code in current directory
# ============================================================

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default configuration
SELFPLAY_GAMES=5000
SELFPLAY_THREADS=2
SELFPLAY_TC="15+0.01"
STOCKFISH_DEPTH=14
CONCURRENCY=$(sysctl -n hw.logicalcpu 2>/dev/null || nproc 2>/dev/null || echo 4)
POSITIONS_FILE="$SCRIPT_DIR/data/selfplay_positions.epd"
QUIET_FILE="$SCRIPT_DIR/data/quiet_training_positions.epd"
EVALUATED_FILE="$SCRIPT_DIR/data/quiet_training_positions_evaluated.epd"
FEATURES_CSV="$SCRIPT_DIR/data/tune_features.csv"
EVAL_CONSTANTS="$PROJECT_ROOT/src/eval/eval_constants.h"
SKIP_SELFPLAY=false
SKIP_STOCKFISH=false
SKIP_REGRESSION=false
REGRESSION_GAMES=200
TUNE_MAXITER=500
FIX_PIECE_VALUES=false
FIX_MOBILITY_BASE=false
UNFIX_PAWN=false
TUNE_PASSES=3
SOFT_LABELS=true
KEEP_INITIAL_MATERIAL=true
PAWN_VALUE=90
FTOL="1e-15"
GTOL="1e-12"
MIN_FEATURE_COUNT=100

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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
    echo "  --games N           Number of selfplay games (default: $SELFPLAY_GAMES)"
    echo "  --threads N         Number of search threads per engine (default: $SELFPLAY_THREADS)"
    echo "  --tc STR            Time control for selfplay (default: $SELFPLAY_TC)"
    echo "  --depth N           Stockfish evaluation depth (default: $STOCKFISH_DEPTH)"
    echo "  --concurrency N     Parallel Stockfish processes (default: $CONCURRENCY)"
    echo "  --maxiter N         L-BFGS-B max iterations per pass (default: $TUNE_MAXITER)"
    echo "  --passes N          Number of tuning passes (default: $TUNE_PASSES)"
    echo "  --pawn-value N      Anchor pawn value (default: $PAWN_VALUE)"
    echo "  --no-soft-labels    Use discrete game outcomes (0/0.5/1) instead of probabilities"
    echo "  --reset-material    Reset piece starting values to standard defaults"
    echo "  --fix-piece-values  Freeze piece values at initial values"
    echo "  --fix-mobility-base Freeze mobility index 0 at initial values"
    echo "  --unfix-pawn        Allow pawn piece value to float"
    echo "  --positions FILE    Use existing positions file (skip selfplay)"
    echo "  --evaluated FILE    Use existing evaluated file (skip Stockfish)"
    echo "  --skip-regression   Skip the regression selfplay match"
    echo "  --regression-games N  Number of regression test games (default: $REGRESSION_GAMES)"
    echo "  -h, --help          Show this help"
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --games) SELFPLAY_GAMES="$2"; shift 2 ;;
        --threads) SELFPLAY_THREADS="$2"; shift 2 ;;
        --tc) SELFPLAY_TC="$2"; shift 2 ;;
        --depth) STOCKFISH_DEPTH="$2"; shift 2 ;;
        --concurrency) CONCURRENCY="$2"; shift 2 ;;
        --maxiter) TUNE_MAXITER="$2"; shift 2 ;;
        --passes) TUNE_PASSES="$2"; shift 2 ;;
        --pawn-value) PAWN_VALUE="$2"; shift 2 ;;
        --no-soft-labels) SOFT_LABELS=false; shift ;;
        --reset-material) KEEP_INITIAL_MATERIAL=false; shift ;;
        --fix-piece-values) FIX_PIECE_VALUES=true; shift ;;
        --fix-mobility-base) FIX_MOBILITY_BASE=true; shift ;;
        --unfix-pawn) UNFIX_PAWN=true; shift ;;
        --min-feature-count) MIN_FEATURE_COUNT="$2"; shift 2 ;;
        --positions) POSITIONS_FILE="$2"; SKIP_SELFPLAY=true; shift 2 ;;
        --evaluated) EVALUATED_FILE="$2"; SKIP_STOCKFISH=true; SKIP_SELFPLAY=true; shift 2 ;;
        --skip-regression) SKIP_REGRESSION=true; shift ;;
        --regression-games) REGRESSION_GAMES="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

# Record start time
START_TIME=$(date +%s)

echo -e "${BLUE}"
echo "╔═══════════════════════════════════════════════════╗"
echo "║       ZeroG Tuning Pipeline                      ║"
echo "╚═══════════════════════════════════════════════════╝"
echo -e "${NC}"
echo "Configuration:"
echo "  Selfplay games:      $SELFPLAY_GAMES"
echo "  Selfplay threads:    $SELFPLAY_THREADS"
echo "  Selfplay TC:         $SELFPLAY_TC"
echo "  Stockfish depth:     $STOCKFISH_DEPTH"
echo "  Concurrency:         $CONCURRENCY"
echo "  L-BFGS-B max iter:   $TUNE_MAXITER"
echo "  Skip selfplay:       $SKIP_SELFPLAY"
echo "  Skip Stockfish:      $SKIP_STOCKFISH"
echo "  Skip regression:     $SKIP_REGRESSION"
echo ""

# ============================================================
# Step 1: Build the engine
# ============================================================
print_step 1 "Building ZeroG engine"

# Save a backup of current eval_constants.h
if [ -f "$EVAL_CONSTANTS" ]; then
    cp "$EVAL_CONSTANTS" "${EVAL_CONSTANTS}.bak"
    print_ok "Backed up $EVAL_CONSTANTS"
fi

make clean > /dev/null 2>&1 || true
make release
print_ok "Engine built successfully"

# ============================================================
# Step 2: Generate selfplay positions
# ============================================================
if [ "$SKIP_SELFPLAY" = false ]; then
    print_step 2 "Generating selfplay positions"

    if [ -f "$SCRIPT_DIR/selfplay.py" ]; then
        python3 "$SCRIPT_DIR/selfplay.py" --games "$SELFPLAY_GAMES" --savefen "$POSITIONS_FILE" --threads "$SELFPLAY_THREADS" --tc "$SELFPLAY_TC"
        print_ok "Generated positions from $SELFPLAY_GAMES games -> $POSITIONS_FILE"
    else
        print_warn "selfplay.py not found. Checking for existing positions..."
        if [ ! -f "$POSITIONS_FILE" ]; then
            print_err "No positions file found. Please provide one with --positions."
            exit 1
        fi
    fi
else
    print_step 2 "Skipping selfplay (using $POSITIONS_FILE)"
fi

if [ ! -f "$POSITIONS_FILE" ]; then
    print_err "Positions file '$POSITIONS_FILE' not found!"
    exit 1
fi
POSITION_COUNT=$(wc -l < "$POSITIONS_FILE" | tr -d ' ')
print_ok "Positions file: $POSITIONS_FILE ($POSITION_COUNT positions)"

# ============================================================
# Step 3: Filter quiet positions
# ============================================================
if [ "$SKIP_STOCKFISH" = false ]; then
    print_step 3 "Filtering quiet positions"

    "$PROJECT_ROOT/builds/zerog" --tune-filter "$POSITIONS_FILE" "$QUIET_FILE"
    QUIET_COUNT=$(wc -l < "$QUIET_FILE" | tr -d ' ')
    print_ok "Filtered to $QUIET_COUNT quiet positions -> $QUIET_FILE"
fi

# ============================================================
# Step 4: Evaluate with Stockfish
# ============================================================
if [ "$SKIP_STOCKFISH" = false ]; then
    print_step 4 "Evaluating positions with Stockfish (depth $STOCKFISH_DEPTH)"

    python3 "$SCRIPT_DIR/evaluate_epd.py" \
        -i "$QUIET_FILE" \
        -o "$EVALUATED_FILE" \
        -d "$STOCKFISH_DEPTH" \
        -c "$CONCURRENCY" \
        -p white
    EVAL_COUNT=$(wc -l < "$EVALUATED_FILE" | tr -d ' ')
    print_ok "Evaluated $EVAL_COUNT positions -> $EVALUATED_FILE"
else
    print_step 4 "Skipping Stockfish evaluation (using $EVALUATED_FILE)"
fi

if [ ! -f "$EVALUATED_FILE" ]; then
    print_err "Evaluated file '$EVALUATED_FILE' not found!"
    exit 1
fi

# ============================================================
# Step 5: Export features to CSV
# ============================================================
print_step 5 "Exporting features to CSV"

"$PROJECT_ROOT/builds/zerog" --tune-export "$EVALUATED_FILE" "$FEATURES_CSV"
print_ok "Exported features -> $FEATURES_CSV"

# ============================================================
# Step 6: Run L-BFGS-B optimization
# ============================================================
TUNE_FLAGS=""
if [ "$FIX_PIECE_VALUES" = true ]; then TUNE_FLAGS="$TUNE_FLAGS --fix-piece-values"; fi
if [ "$FIX_MOBILITY_BASE" = true ]; then TUNE_FLAGS="$TUNE_FLAGS --freeze-mobility-zero-buckets"; fi
if [ "$UNFIX_PAWN" = true ]; then TUNE_FLAGS="$TUNE_FLAGS --unfix-pawn"; fi
if [ "$SOFT_LABELS" = true ]; then TUNE_FLAGS="$TUNE_FLAGS --soft-labels"; fi
if [ "$KEEP_INITIAL_MATERIAL" = true ]; then TUNE_FLAGS="$TUNE_FLAGS --keep-initial-material"; fi
TUNE_FLAGS="$TUNE_FLAGS --pawn-value $PAWN_VALUE --ftol $FTOL --gtol $GTOL --min-feature-count $MIN_FEATURE_COUNT"

for (( pass=1; pass<=TUNE_PASSES; pass++ )); do
    print_step 6 "Running Texel tuning pass $pass of $TUNE_PASSES (L-BFGS-B)"
    python3 "$SCRIPT_DIR/tune.py" -i "$FEATURES_CSV" -o "$EVAL_CONSTANTS" --initial-header "$EVAL_CONSTANTS" --maxiter "$TUNE_MAXITER" $TUNE_FLAGS
    print_ok "Pass $pass of $TUNE_PASSES complete"
done
print_ok "Optimization complete ($TUNE_PASSES passes)"

# ============================================================
# Step 7: Rebuild with new constants
# ============================================================
print_step 7 "Rebuilding engine with tuned constants"

make clean > /dev/null 2>&1 || true
make release
print_ok "Engine rebuilt with new constants"

# ============================================================
# Step 8: Regression selfplay match (optional)
# ============================================================
if [ "$SKIP_REGRESSION" = false ] && [ -f "$SCRIPT_DIR/selfplay.py" ]; then
    print_step 8 "Running regression selfplay match ($REGRESSION_GAMES games)"
    if [ -f "zerog_prev" ]; then
        print_ok "Using ./zerog_prev as baseline"
        python3 "$SCRIPT_DIR/selfplay.py" --games "$REGRESSION_GAMES" --concurrency "$CONCURRENCY" --tc "$SELFPLAY_TC"
    else
        print_warn "No zerog_prev baseline found for regression match."
        print_warn "Copy the previous engine to ./zerog_prev to run verification match automatically."
    fi
else
    print_step 8 "Skipping regression match"
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
echo -e "${GREEN}║       Tuning Pipeline Complete!                  ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════╝${NC}"
echo ""
echo "  Total time: ${MINUTES}m ${SECONDS}s"
echo "  Output:     $EVAL_CONSTANTS"
echo "  Backup:     ${EVAL_CONSTANTS}.bak"
echo ""
echo "  Next steps:"
echo "    1. Test the new engine: ./builds/zerog"
echo "    2. Compare vs old: diff ${EVAL_CONSTANTS}.bak $EVAL_CONSTANTS"
echo "    3. Run a tournament to measure Elo gain"
echo ""
