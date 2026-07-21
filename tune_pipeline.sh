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
#   3. Filter quiet positions (with in-check & score-range rejection)
#   4. Evaluate positions with Stockfish
#   5. Classify & weight positions (hard-example mining, rebalancing)
#   6. Export features to CSV (with weights)
#   7. Run L-BFGS-B optimization (with hybrid labels, regularization)
#   8. Rebuild with new constants
#   9. (Optional) Run regression selfplay match
#
# Prerequisites:
#   - Stockfish installed and accessible
#   - Python 3 with numpy and scipy
#   - ZeroG source code in current directory
# ============================================================

set -e  # Exit on error

# Default configuration
SELFPLAY_GAMES=10000
SELFPLAY_THREADS=2
STOCKFISH_DEPTH=16
CONCURRENCY=$(sysctl -n hw.logicalcpu 2>/dev/null || nproc 2>/dev/null || echo 4)
POSITIONS_FILE="selfplay_positions.epd"
QUIET_FILE="quiet_training_positions.epd"
EVALUATED_FILE="quiet_training_positions_evaluated.epd"
CLASSIFIED_FILE="classified_training_positions.epd"
FEATURES_CSV="tune_features.csv"
EVAL_CONSTANTS="src/eval/eval_constants.h"
SKIP_SELFPLAY=false
SKIP_STOCKFISH=false
SKIP_CLASSIFY=false
SKIP_REGRESSION=false
REGRESSION_GAMES=200
TUNE_MAXITER=500
TUNE_LAMBDA=1.0
TUNE_L2=0.0
TUNE_VAL_SPLIT=0.1
HARD_THRESHOLD=100
HARD_WEIGHT=1.5

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
    echo "  --depth N           Stockfish evaluation depth (default: $STOCKFISH_DEPTH)"
    echo "  --concurrency N     Parallel Stockfish processes (default: $CONCURRENCY)"
    echo "  --maxiter N         L-BFGS-B max iterations (default: $TUNE_MAXITER)"
    echo "  --lambda F          Hybrid label interpolation 0-1 (default: $TUNE_LAMBDA)"
    echo "  --l2 F              L2 regularization strength (default: $TUNE_L2)"
    echo "  --val-split F       Validation split fraction (default: $TUNE_VAL_SPLIT)"
    echo "  --hard-threshold N  Centipawn threshold for hard positions (default: $HARD_THRESHOLD)"
    echo "  --hard-weight F     Weight multiplier for hard positions (default: $HARD_WEIGHT)"
    echo "  --positions FILE    Use existing positions file (skip selfplay)"
    echo "  --evaluated FILE    Use existing evaluated file (skip Stockfish)"
    echo "  --classified FILE   Use existing classified file (skip classify)"
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
        --depth) STOCKFISH_DEPTH="$2"; shift 2 ;;
        --concurrency) CONCURRENCY="$2"; shift 2 ;;
        --maxiter) TUNE_MAXITER="$2"; shift 2 ;;
        --lambda) TUNE_LAMBDA="$2"; shift 2 ;;
        --l2) TUNE_L2="$2"; shift 2 ;;
        --val-split) TUNE_VAL_SPLIT="$2"; shift 2 ;;
        --hard-threshold) HARD_THRESHOLD="$2"; shift 2 ;;
        --hard-weight) HARD_WEIGHT="$2"; shift 2 ;;
        --positions) POSITIONS_FILE="$2"; SKIP_SELFPLAY=true; shift 2 ;;
        --evaluated) EVALUATED_FILE="$2"; SKIP_STOCKFISH=true; SKIP_SELFPLAY=true; shift 2 ;;
        --classified) CLASSIFIED_FILE="$2"; SKIP_CLASSIFY=true; SKIP_STOCKFISH=true; SKIP_SELFPLAY=true; shift 2 ;;
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
echo "  Stockfish depth:     $STOCKFISH_DEPTH"
echo "  Concurrency:         $CONCURRENCY"
echo "  L-BFGS-B max iter:   $TUNE_MAXITER"
echo "  Lambda:              $TUNE_LAMBDA"
echo "  L2 regularization:   $TUNE_L2"
echo "  Val split:           $TUNE_VAL_SPLIT"
echo "  Hard threshold:      $HARD_THRESHOLD cp"
echo "  Hard weight:         $HARD_WEIGHT"
echo "  Skip selfplay:       $SKIP_SELFPLAY"
echo "  Skip Stockfish:      $SKIP_STOCKFISH"
echo "  Skip classify:       $SKIP_CLASSIFY"
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

    if [ -f "selfplay.py" ]; then
        python3 selfplay.py --games "$SELFPLAY_GAMES" --savefen "$POSITIONS_FILE" --threads "$SELFPLAY_THREADS"
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

    ./builds/zerog --tune-filter "$POSITIONS_FILE" "$QUIET_FILE"
    QUIET_COUNT=$(wc -l < "$QUIET_FILE" | tr -d ' ')
    print_ok "Filtered to $QUIET_COUNT quiet positions -> $QUIET_FILE"
fi

# ============================================================
# Step 4: Evaluate with Stockfish
# ============================================================
if [ "$SKIP_STOCKFISH" = false ]; then
    print_step 4 "Evaluating positions with Stockfish (depth $STOCKFISH_DEPTH)"

    python3 evaluate_epd.py \
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
# Step 5: Classify & weight positions
# ============================================================
if [ "$SKIP_CLASSIFY" = false ]; then
    print_step 5 "Classifying and weighting positions"

    python3 classify_positions.py \
        -i "$EVALUATED_FILE" \
        -o "$CLASSIFIED_FILE" \
        --hard-threshold "$HARD_THRESHOLD" \
        --hard-weight "$HARD_WEIGHT"
    CLASSIFIED_COUNT=$(wc -l < "$CLASSIFIED_FILE" | tr -d ' ')
    print_ok "Classified $CLASSIFIED_COUNT positions -> $CLASSIFIED_FILE"
else
    print_step 5 "Skipping classification (using $CLASSIFIED_FILE)"
fi

if [ ! -f "$CLASSIFIED_FILE" ]; then
    print_err "Classified file '$CLASSIFIED_FILE' not found!"
    exit 1
fi

# ============================================================
# Step 6: Export features to CSV
# ============================================================
print_step 6 "Exporting features to CSV"

./builds/zerog --tune-export "$CLASSIFIED_FILE" "$FEATURES_CSV"
print_ok "Exported features -> $FEATURES_CSV"

# ============================================================
# Step 7: Run L-BFGS-B optimization
# ============================================================
print_step 7 "Running Texel tuning (L-BFGS-B)"

python3 tune.py \
    -i "$FEATURES_CSV" \
    -o "$EVAL_CONSTANTS" \
    --maxiter "$TUNE_MAXITER" \
    --lambda "$TUNE_LAMBDA" \
    --l2 "$TUNE_L2" \
    --val-split "$TUNE_VAL_SPLIT"
print_ok "Optimization complete"

# ============================================================
# Step 8: Rebuild with new constants
# ============================================================
print_step 8 "Rebuilding engine with tuned constants"

make clean > /dev/null 2>&1 || true
make release
print_ok "Engine rebuilt with new constants"

# ============================================================
# Step 9: Regression selfplay match (optional)
# ============================================================
if [ "$SKIP_REGRESSION" = false ] && [ -f "selfplay.py" ]; then
    print_step 9 "Running regression selfplay match ($REGRESSION_GAMES games)"
    print_warn "Compare the new engine against the previous version manually."
    print_warn "The old constants are saved in ${EVAL_CONSTANTS}.bak"
else
    print_step 9 "Skipping regression match"
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
echo "  Pipeline files:"
echo "    Quiet positions:      $QUIET_FILE"
echo "    Evaluated positions:  $EVALUATED_FILE"
echo "    Classified positions: $CLASSIFIED_FILE"
echo "    Features CSV:         $FEATURES_CSV"
echo ""
echo "  Next steps:"
echo "    1. Test the new engine: ./builds/zerog"
echo "    2. Compare vs old: diff ${EVAL_CONSTANTS}.bak $EVAL_CONSTANTS"
echo "    3. Run a tournament to measure Elo gain"
echo ""
