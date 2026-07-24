## Status Summary

All **6 Critical Issues** and **EPD parsing** have been fixed and verified with tests!

| Category | Issue | Status |
|---|---|---|
| Critical 1 | Target meaning and scale (probability vs pawn units) | **FIXED** |
| Critical 2 | Fresh network weight quantization before first game | **FIXED** |
| Critical 3 | Network pointer ownership (`NeuralNetwork **nn_io`) | **FIXED** |
| Critical 4 | Repetition history tracking & threefold draw adjudication | **FIXED** |
| Critical 5 | Fallback evaluation side-to-move perspective | **FIXED** |
| Critical 6 | Feature index consistency (780 inputs: 768 piece-square + 4 castling + 8 en-passant) | **FIXED** |
| Other | EPD parsing line sanitization & whitespace trimming | **FIXED** |
| Other | Checkpointing interval (saving every N games instead of every game) | **REMAINING** |
| Other | Mini-batch gradient accumulation vs sequential Adam steps | **REMAINING** |
| Other | Replay buffer sampling (ply downsampling, deduplication) | **REMAINING** |
| Other | Initial teacher / opponent pool / network gating | **REMAINING** |

---

## Detailed Issue Breakdown

## Critical issues

### 1. The target has the wrong meaning and scale — [FIXED]

Converted blended target probability $p \in [0, 1]$ back to signed pawn evaluation scale using inverse logit mapping:
```c
static float probability_to_pawns(float p, float scale_cp) {
    const float eps = 0.001f;
    if (p < eps) p = eps;
    else if (p > 1.0f - eps) p = 1.0f - eps;
    float eval_cp = scale_cp * log10f(p / (1.0f - p));
    return eval_cp / 100.0f;
}
```

### 2. A fresh network is not quantized before the first game — [FIXED]

Added `nn_quantize(target_nn)` before beginning the self-play game loop.

### 3. Resetting the network leaves the caller with a dangling pointer — [FIXED]

Updated signature to `int online_trainer_run_selfplay(NeuralNetwork **nn_io, ...)` and updated caller in `src/main.c` to pass `&eval_nn`.

### 4. Repetition history is discarded before every search — [FIXED]

Maintained `SearchLimits limits` and `limits.history_hashes` across all game plies and added threefold repetition draw detection in the self-play loop.

### 5. The fallback evaluation has the wrong perspective for Black — [FIXED]

Adjusted fallback score to side-to-move perspective: `res.score = (pos.sideToMove == WHITE) ? white_score : -white_score`.

### 6. Likely 780-versus-768 feature mismatch — [FIXED]

Verified 780 features (768 piece-square + 4 castling + 8 en-passant file) are consistently extracted, updated, and evaluated across `nn_extract_features`, `nnue_refresh_accumulator`, `nnue_update_accumulator`, and `nnue_undo_accumulator`.

---

## Other important problems

### Opening-book positions may receive fake 0.5 labels — [N/A - Book disabled during online search]

### EPD parsing may silently eliminate opening diversity — [FIXED]

`load_epd_openings()` now trims EPD opcode separators (`;`) and trailing whitespace so lines parse cleanly with `fen_parse()`.

### Saving every game is unnecessarily expensive — [REMAINING]

Currently `online_trainer_end_game()` saves after every game. Can be changed to save every $N$ games (e.g. 50-100 games) or at shutdown.

### This is not actually mini-batch training — [REMAINING]

Currently performs $K$ sequential Adam steps per game. True mini-batching can aggregate gradients across $K$ samples before calling Adam.

### All plies are treated equally — [REMAINING]

Replay buffer currently records every ply. Can downsample every 2–4 plies or filter duplicates.

### Self-training from a fresh random evaluator is unstable — [REMAINING]

Training directly against self from random initialization can be improved by using a handcrafted evaluation teacher, pretrained network, or network pool gating.

