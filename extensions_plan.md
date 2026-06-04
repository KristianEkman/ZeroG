# Search Extensions Implementation Plan

This document outlines the proposed design, logic, and testing plan for introducing additional search extensions to improve the playing strength of **ChessAI2027**.

---

## 1. Passed Pawn Push Extensions

### Objective
Extend the search when a passed pawn is pushed to the 7th rank (for White) or 2nd rank (for Black) to prevent horizon-effect miscalculations near promotion.

### Design
- **Detection**: During search, if the move is a pawn push to rank 7 (White) or rank 2 (Black), we check if the pawn is a *passed pawn*.
- **Helper Function**: We can add a function `int is_passed_pawn(const Position *pos, Square sq, Color side)` in evaluation or move generation.
- **Search Logic**:
  ```c
  // In pvs.c
  int is_pawn = PIECE_TYPE(pos->board[MOVE_FROM(move)]) == PAWN;
  int target_rank = RANK_OF(MOVE_TO(move));
  int is_near_promo = (target_rank == (pos->sideToMove == WHITE ? 6 : 1));

  if (is_pawn && is_near_promo && is_passed_pawn(pos, MOVE_FROM(move), pos->sideToMove)) {
      ext = 1;
  }
  ```

---

## 2. One-Reply Extensions

### Objective
Extend the search depth by 1 ply if there is only one legal move in the current position. 

### Design
- **Logic**: If the current node has exactly 1 legal move, the branching factor is 1, meaning extending is virtually free.
- **Search Logic**:
  - We can count the number of legal moves generated.
  - Or, if we are using pseudo-legal move generation (which we do), we can verify how many moves are legal after trying them. If only 1 move is found to be legal, we extend the search.
  - Since we search moves sequentially, we can't easily know if there's only 1 legal move until we have generated and filtered them all. 
  - To avoid full move generation overhead where not needed, we can check if `count == 1` when generating evasions (if in check) or when pseudo-legal moves count is small.

---

## 3. Recapture Extensions

### Objective
Extend search by 1 ply when a move recaptures material at the same square where a capture just took place, resolving forced tactical trades.

### Design
- **Logic**: If the previous move was a capture, and the current move captures a piece on the same target square.
- **History Tracking**: We can inspect the parent `UndoNode` to find the target square of the previous move:
  ```c
  if (history && history->undo) {
      Move prev_move = history->undo->move; // assuming Undo stores the move
      if (MOVE_IS_CAPTURE(prev_move) && MOVE_TO(move) == MOVE_TO(prev_move)) {
          ext = 1;
      }
  }
  ```

---

## 4. Double Singular Extensions

### Objective
Extend search by 2 plies if a singular move is exceptionally superior to all other moves.

### Design
- **Logic**: In the singular search, if the score fails low by a much wider margin (e.g. `singular_margin * 2` or a separate `double_singular_margin`), we extend the node depth by 2 plies.
- **Search Logic**:
  ```c
  // In pvs.c inside singular search logic
  int double_singular_margin = singular_margin * 2;
  int double_singular_beta = tt_entry.score - double_singular_margin;
  
  if (score < double_singular_beta) {
      extension = 2;
  } else if (score < singular_beta) {
      extension = 1;
  }
  ```

---

## Verification & Tuning Plan

To ensure these extensions actually improve playing strength (unlike basic check extensions, which showed a -22 Elo penalty), we will:
1. **Dynamic Toggles**: Wrap each extension in a UCI option (e.g., `Passed_Pawn_Extension`, `One_Reply_Extension`) during testing.
2. **Self-Play Tournaments**: Run 200-300 games for each option to measure the Elo change.
3. **Optimized Margins**: Tune margins (like singular margins) using the SPSA tuning script `spsa.py` to balance search overhead vs. tactical depth.
