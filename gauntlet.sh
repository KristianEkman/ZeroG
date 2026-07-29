#!/bin/bash
# ==============================================================================
# Cutechess Gauntlet Tournament Runner
# ==============================================================================
#
# Automatically scans an engines directory and starts a cutechess-cli gauntlet
# tournament with a master engine (default: ZeroG) playing against all of them.
# ==============================================================================

# Exit on errors
set -e

# Get script directory
SCRIPT_DIR=$(dirname "$(realpath "$0")")

# Default settings
ENGINES_DIR="/home/kristian/engines"
ENGINES_FILE="tested_engines.txt"
MASTER_ENGINE="./builds/zerog"
MASTER_NAME="ZeroG"
GAMES=100
CONCURRENCY=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
TC="10+0.1"
OPENINGS="games/top_engine_games.pgn"
PGN_OUT="gauntlet_results.pgn"
FRESH_PGN=false
CUTECHESS_CLI=""

# Ordo settings
RUN_ORDO=true
ORDO_ANCHOR_NAME="Aurora"
ORDO_ANCHOR_RATING="2763"

# Colors for premium console output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

print_banner() {
    echo -e "${BLUE}======================================================================${NC}"
    echo -e "${BLUE}          Cutechess Gauntlet Tournament Runner${NC}"
    echo -e "${BLUE}======================================================================${NC}"
}

print_error() {
    echo -e "${RED}Error: $1${NC}" >&2
}

print_warning() {
    echo -e "${YELLOW}Warning: $1${NC}"
}

print_info() {
    echo -e "${CYAN}$1${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

show_help() {
    print_banner
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -m, --master PATH       Path to the master engine executable (default: $MASTER_ENGINE)"
    echo "  -mn, --master-name NAME  Name of the master engine (default: $MASTER_NAME)"
    echo "  -d, --engines-dir DIR   Directory containing opponent engines (default: $ENGINES_DIR)"
    echo "  -ef, --engines-file FILE Text file containing list of opponent engines (default: $ENGINES_FILE)"
    echo "  -g, --games N           Number of games to play per opponent encounter (default: $GAMES)"
    echo "  -c, --concurrency N     Number of concurrent games (default: $CONCURRENCY)"
    echo "  -t, --tc TC             Time control (default: $TC)"
    echo "  -o, --openings FILE     Path to the openings PGN file (default: $OPENINGS)"
    echo "  -p, --pgnout FILE       Path to save PGN games output (default: $PGN_OUT)"
    echo "  -an, --anchor-name NAME Anchor player/engine name for Ordo Elo calculation (default: $ORDO_ANCHOR_NAME)"
    echo "  -ar, --anchor-rating R  Rating for the anchor player (default: $ORDO_ANCHOR_RATING)"
    echo "  -fr, --fresh            Delete existing PGN output file before starting for a fresh run"
    echo "  --no-ordo               Disable Ordo Elo calculation at the end"
    echo "  -h, --help              Show this help message"
    echo ""
    exit 0
}

# Parse command line options
while [[ $# -gt 0 ]]; do
    case "$1" in
        -m|--master)
            MASTER_ENGINE="$2"
            shift 2
            ;;
        -mn|--master-name)
            MASTER_NAME="$2"
            shift 2
            ;;
        -d|--engines-dir)
            ENGINES_DIR="$2"
            shift 2
            ;;
        -ef|--engines-file)
            ENGINES_FILE="$2"
            shift 2
            ;;
        -g|--games)
            GAMES="$2"
            shift 2
            ;;
        -c|--concurrency)
            CONCURRENCY="$2"
            shift 2
            ;;
        -t|--tc)
            TC="$2"
            shift 2
            ;;
        -o|--openings)
            OPENINGS="$2"
            shift 2
            ;;
        -p|--pgnout)
            PGN_OUT="$2"
            shift 2
            ;;
        -an|--anchor-name)
            ORDO_ANCHOR_NAME="$2"
            shift 2
            ;;
        -ar|--anchor-rating)
            ORDO_ANCHOR_RATING="$2"
            shift 2
            ;;
        -fr|--fresh)
            FRESH_PGN=true
            shift 1
            ;;
        --no-ordo)
            RUN_ORDO=false
            shift 1
            ;;
        -h|--help)
            show_help
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            ;;
    esac
done

# Resolve default anchor name to master engine name if not set
if [ -z "$ORDO_ANCHOR_NAME" ]; then
    ORDO_ANCHOR_NAME="$MASTER_NAME"
fi

print_banner

# 1. Locate cutechess-cli
if [ -x "../cutechess/cutechess/build/cutechess-cli" ]; then
    CUTECHESS_CLI="../cutechess/cutechess/build/cutechess-cli"
elif command -v cutechess-cli &> /dev/null; then
    CUTECHESS_CLI="cutechess-cli"
else
    print_error "cutechess-cli not found!"
    print_info "Please install cutechess-cli or compile it in '../cutechess/'."
    exit 1
fi
print_success "Found cutechess-cli: $CUTECHESS_CLI"

# 2. Verify master engine
if [ ! -x "$MASTER_ENGINE" ]; then
    print_error "Master engine executable not found or not executable: $MASTER_ENGINE"
    print_info "Please build it first or specify correct path using -m/--master."
    exit 1
fi
print_success "Master engine verified: $MASTER_NAME ($MASTER_ENGINE)"

# 3. Verify engines directory
if [ ! -d "$ENGINES_DIR" ]; then
    print_error "Engines directory not found: $ENGINES_DIR"
    exit 1
fi

# 4. Verify opening book
if [ ! -f "$OPENINGS" ]; then
    print_warning "Opening book '$OPENINGS' not found. Will run without opening book."
    OPENINGS=""
else
    print_success "Opening book verified: $OPENINGS"
fi

# 5. Clean name helper function
get_clean_name() {
    local file_name="$1"
    local base=$(basename "$file_name")
    
    case "$base" in
        Wilted-1-0-0-0-Linux-x86-64-v3)
            echo "Wilted"
            ;;
        aurora-v1.27.0-time-hotfix-linux)
            echo "Aurora"
            ;;
        Coiled_1.1_x64)
            echo "Coiled"
            ;;
        reyna-1.0-linux)
            echo "Reyna"
            ;;
        casanchess)
            echo "Casanchess"
            ;;
        toad)
            echo "Toad"
            ;;
        zerog_prev)
            echo "ZeroG-Prev"
            ;;
        zerog-1.0.0)
            echo "ZeroG-1.0.0"
            ;;
        zerog-1.0.2)
            echo "ZeroG-1.0.2"
            ;;
        zerog)
            echo "ZeroG"
            ;;
        *)
            # Strip common suffixes and capitalize
            local clean=$(echo "$base" | sed -E 's/[-_]?(linux|x64|x86|v[0-9.]+|time-hotfix)//gi')
            echo "$(echo "${clean:0:1}" | tr '[:lower:]' '[:upper:]')${clean:1}"
            ;;
    esac
}

# 6. Locate or compile Ordo Elo calculator
find_or_build_ordo() {
    if command -v ordo &> /dev/null; then
        echo "ordo"
        return 0
    fi
    
    local ordo_dir
    ordo_dir=$(realpath "$SCRIPT_DIR/../Ordo")
    
    if [ -x "$ordo_dir/ordo" ]; then
        echo "$ordo_dir/ordo"
        return 0
    fi
    
    print_info "Ordo Elo calculator not found. Downloading and compiling from GitHub..." >&2
    
    # Check if git is available
    if ! command -v git &> /dev/null; then
        print_warning "git is required to download Ordo. Skipping Ordo calculation." >&2
        return 1
    fi
    
    # Check if make is available
    if ! command -v make &> /dev/null; then
        print_warning "make is required to compile Ordo. Skipping Ordo calculation." >&2
        return 1
    fi
    
    # Clone and build
    if ! git clone --depth 1 https://github.com/michiguel/Ordo.git "$ordo_dir" >&2; then
        print_warning "Failed to clone Ordo repository. Skipping Ordo calculation." >&2
        return 1
    fi
    
    if ! (cd "$ordo_dir" && make >&2); then
        print_warning "Failed to compile Ordo. Skipping Ordo calculation." >&2
        return 1
    fi
    
    if [ -x "$ordo_dir/ordo" ]; then
        print_success "Ordo compiled successfully at $ordo_dir/ordo" >&2
        echo "$ordo_dir/ordo"
        return 0
    else
        print_warning "Ordo executable not found after build. Skipping Ordo calculation." >&2
        return 1
    fi
}

# 6. Find opponent engines
OPPOSE_FILES=()

# Resolve engines file path relative to SCRIPT_DIR if not absolute
if [[ "$ENGINES_FILE" != /* ]] && [ ! -f "$ENGINES_FILE" ]; then
    ENGINES_FILE_RESOLVED="$SCRIPT_DIR/$ENGINES_FILE"
else
    ENGINES_FILE_RESOLVED="$ENGINES_FILE"
fi

if [ -f "$ENGINES_FILE_RESOLVED" ]; then
    print_info "Loading opponent engines list from '$ENGINES_FILE_RESOLVED'..."
    while IFS= read -r line || [ -n "$line" ]; do
        # Trim leading and trailing whitespace
        line=$(echo "$line" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
        # Skip empty lines and comments
        [[ -z "$line" || "$line" =~ ^# ]] && continue
        
        # Resolve path
        if [[ "$line" == /* || "$line" == ./* ]]; then
            engine_path="$line"
        else
            engine_path="$ENGINES_DIR/$line"
        fi
        
        if [ -x "$engine_path" ]; then
            OPPOSE_FILES+=("$engine_path")
        else
            print_warning "Engine not found or not executable: $engine_path (from $ENGINES_FILE_RESOLVED)"
        fi
    done < "$ENGINES_FILE_RESOLVED"
else
    print_info "Engine list file '$ENGINES_FILE_RESOLVED' not found. Scanning '$ENGINES_DIR'..."
    mapfile -t OPPOSE_FILES < <(find "$ENGINES_DIR" -type f -executable 2>/dev/null)
fi

if [ ${#OPPOSE_FILES[@]} -eq 0 ]; then
    print_error "No executable opponent engines found."
    exit 1
fi

# Prepare engine arguments for cutechess-cli
ENGINE_ARGS=()

# Add master engine as the first engine (seed / gauntlet champion)
# Set working directory to master engine's directory
MASTER_ABS=$(realpath "$MASTER_ENGINE")
MASTER_DIR=$(dirname "$MASTER_ABS")
ENGINE_ARGS+=(
    "-engine" "cmd=$MASTER_ABS" "proto=uci" "name=$MASTER_NAME" "dir=$MASTER_DIR"
)

print_info "Opponent engines found:"
for file in "${OPPOSE_FILES[@]}"; do
    # Skip the master engine if it is in the engines directory
    if [[ "$(realpath "$file")" == "$MASTER_ABS" ]]; then
        continue
    fi
    
    abs_file=$(realpath "$file")
    name=$(get_clean_name "$abs_file")
    dir=$(dirname "$abs_file")
    print_info "  - $name ($abs_file)"
    
    ENGINE_ARGS+=(
        "-engine" "cmd=$abs_file" "proto=uci" "name=$name" "dir=$dir"
    )
done

# Check if we have at least one opponent
if [ ${#ENGINE_ARGS[@]} -le 5 ]; then
    print_error "No opponent engines found to run against."
    exit 1
fi

# 7. Construct and run cutechess-cli command
CMD=("$CUTECHESS_CLI")
CMD+=("${ENGINE_ARGS[@]}")
CMD+=("-each" "tc=$TC")
CMD+=("-tournament" "gauntlet")
CMD+=("-games" "$GAMES")
CMD+=("-concurrency" "$CONCURRENCY")
CMD+=("-repeat")
CMD+=("-recover")

if [ -n "$OPENINGS" ]; then
    CMD+=("-openings" "file=$OPENINGS" "format=pgn" "plies=16" "order=random")
fi

if [ -n "$PGN_OUT" ]; then
    if [ "$FRESH_PGN" = true ] && [ -f "$PGN_OUT" ]; then
        print_info "Removing existing PGN output file '$PGN_OUT' for a fresh run..."
        rm -f "$PGN_OUT"
    fi
    CMD+=("-pgnout" "$PGN_OUT")
fi

echo ""
print_info "Tournament configuration:"
echo "  Master Engine: $MASTER_NAME"
echo "  Games:         $GAMES games per match"
echo "  Concurrency:   $CONCURRENCY"
echo "  Time Control:  $TC"
if [ -n "$OPENINGS" ]; then
    echo "  Opening Book:  $OPENINGS"
fi
if [ -n "$PGN_OUT" ]; then
    echo "  PGN Output:    $PGN_OUT"
fi
echo ""

print_info "Starting cutechess-cli gauntlet match..."
echo "Command: ${CMD[*]}"
echo "----------------------------------------------------------------------"

# Run the tournament command (do not use exec so we can run ordo after it finishes)
if "${CMD[@]}"; then
    print_success "Tournament finished successfully."
else
    print_warning "Tournament finished with errors or was interrupted."
fi

# Run Ordo Elo calculation
if [ "$RUN_ORDO" = true ]; then
    if [ ! -f "$PGN_OUT" ] || [ ! -s "$PGN_OUT" ]; then
        print_warning "No games were recorded to '$PGN_OUT'. Skipping Elo calculation."
    else
        echo ""
        print_info "======================================================================"
        print_info "          Calculating Elo Ratings with Ordo"
        print_info "======================================================================"
        
        ORDO_BIN=$(find_or_build_ordo || true)
        if [ -n "$ORDO_BIN" ] && [ -x "$ORDO_BIN" ]; then
            print_info "Running Ordo: $ORDO_BIN -p $PGN_OUT -A \"$ORDO_ANCHOR_NAME\" -a $ORDO_ANCHOR_RATING -G"
            echo ""
            set +e # Don't exit if Ordo has a rating calculation error
            "$ORDO_BIN" -p "$PGN_OUT" -A "$ORDO_ANCHOR_NAME" -a "$ORDO_ANCHOR_RATING" -G
            set -e
        else
            print_warning "Could not locate or build Ordo. Please install it manually."
        fi
    fi
fi
