#ifndef TESTS_H
#define TESTS_H

/* Roundtrip tests */
void test_apply_undo_knight_roundtrip(void);
void test_apply_double_push_sets_ep(void);
void test_capture_resets_fifty(void);
void test_pawn_push_resets_fifty(void);
void test_castling_rights_king_move(void);
void test_castling_rights_rook_moves(void);
void test_en_passant_capture_roundtrip(void);
void test_promotion_undo(void);
void test_castling_make_unmake(void);
void test_black_move_increments_and_decrements_fullmove(void);
void test_apply_undo_all_moves_in_standard_positions(void);
void test_castling_rights_lost_by_rook_capture(void);
void test_all_promotion_types_roundtrip(void);
void test_en_passant_restores_previous_ep_square(void);

/* Pseudo-legal tests */
void test_startpos_white_move_count(void);
void test_startpos_includes_e2e4(void);
void test_startpos_includes_b1c3(void);
void test_startpos_does_not_include_blocked_castling(void);
void test_castling_generated_when_path_clear(void);
void test_black_move_count_after_white_move(void);
void test_en_passant_generated(void);
void test_no_en_passant_without_ep_square(void);
void test_promotions_for_white_pawn_on_a7(void);
void test_no_moves_leave_own_king_in_check(void);

/* Attack tests */
void test_e4_attacked_by_black_knight_on_f6(void);
void test_d2_attacked_by_white_queen_on_d1(void);
void test_f7_attacked_by_white_bishop_on_c4(void);
void test_g2_attacked_by_black_pawn_on_h3(void);

/* Perft tests */
void test_perft_startpos_depth0(void);
void test_perft_startpos_depth1(void);
void test_perft_startpos_depth2(void);
void test_perft_startpos_depth3(void);
void test_perft_startpos_depth4(void);
void test_perft_kiwipete_depth1(void);
void test_perft_kiwipete_depth2(void);
void test_perft_kiwipete_depth3(void);
void test_perft_pos3_depth1(void);
void test_perft_pos3_depth2(void);
void test_perft_pos3_depth3(void);
void test_perft_pos4_depth1(void);
void test_perft_pos4_depth2(void);
void test_perft_pos4_depth3(void);
void test_perft_pos5_depth1(void);
void test_perft_pos5_depth2(void);
void test_perft_pos6_depth1(void);
void test_perft_pos6_depth2(void);

/* Zobrist tests */
void test_zobrist_hash_keys_startpos(void);
void test_zobrist_hash_keys_kiwipete(void);
void test_zobrist_hash_keys_pos4(void);
void test_zobrist_hash_keys_pos3(void);
void test_zobrist_hash_keys_pos5(void);
void test_zobrist_hash_keys_pos6(void);
void test_zobrist_castling_white_ks(void);
void test_zobrist_castling_white_qs(void);
void test_zobrist_castling_black_ks(void);
void test_zobrist_castling_black_qs(void);
void test_zobrist_en_passant_capture_white(void);
void test_zobrist_en_passant_capture_black(void);
void test_zobrist_en_passant_ignored_cleared(void);
void test_zobrist_promotions_and_promo_captures(void);
void test_zobrist_castling_rights_loss_king_move(void);
void test_zobrist_castling_rights_loss_rook_move(void);
void test_zobrist_castling_rights_loss_rook_captured(void);

/* King safety tests */
void test_legal_movegen_ep_discovered_check(void);
void test_legal_movegen_castling_restrictions(void);
void test_legal_movegen_double_check_evasions(void);
void test_legal_movegen_absolute_pins(void);
void test_legal_movegen_safety_recursive_startpos(void);
void test_legal_movegen_safety_recursive_kiwipete(void);
void test_legal_movegen_safety_recursive_pos3(void);
void test_legal_movegen_safety_recursive_pos4(void);
void test_legal_movegen_safety_recursive_pos5(void);
void test_legal_movegen_safety_recursive_pos6(void);
void test_promotion_captures_all_combinations(void);
void test_promotion_captures_castling_rights(void);
void test_promotion_captures_movegen_pinned(void);
void test_promotion_captures_movegen_resolves_check(void);

#endif /* TESTS_H */
