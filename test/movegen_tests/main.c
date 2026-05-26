#include "test_helpers.h"
#include "tests.h"

int main(void)
{
    bitboard_init();

    UNITY_BEGIN();

    /* ── Roundtrip Tests ── */
    RUN_TEST(test_apply_undo_knight_roundtrip);
    RUN_TEST(test_apply_double_push_sets_ep);
    RUN_TEST(test_capture_resets_fifty);
    RUN_TEST(test_pawn_push_resets_fifty);
    RUN_TEST(test_castling_rights_king_move);
    RUN_TEST(test_castling_rights_rook_moves);
    RUN_TEST(test_en_passant_capture_roundtrip);
    RUN_TEST(test_promotion_undo);
    RUN_TEST(test_castling_make_unmake);
    RUN_TEST(test_black_move_increments_and_decrements_fullmove);
    RUN_TEST(test_apply_undo_all_moves_in_standard_positions);
    RUN_TEST(test_castling_rights_lost_by_rook_capture);
    RUN_TEST(test_all_promotion_types_roundtrip);
    RUN_TEST(test_en_passant_restores_previous_ep_square);

    /* ── Pseudo-Legal Tests ── */
    RUN_TEST(test_startpos_white_move_count);
    RUN_TEST(test_startpos_includes_e2e4);
    RUN_TEST(test_startpos_includes_b1c3);
    RUN_TEST(test_startpos_does_not_include_blocked_castling);
    RUN_TEST(test_castling_generated_when_path_clear);
    RUN_TEST(test_black_move_count_after_white_move);
    RUN_TEST(test_en_passant_generated);
    RUN_TEST(test_no_en_passant_without_ep_square);
    RUN_TEST(test_promotions_for_white_pawn_on_a7);
    RUN_TEST(test_no_moves_leave_own_king_in_check);

    /* ── Attack Tests ── */
    RUN_TEST(test_e4_attacked_by_black_knight_on_f6);
    RUN_TEST(test_d2_attacked_by_white_queen_on_d1);
    RUN_TEST(test_f7_attacked_by_white_bishop_on_c4);
    RUN_TEST(test_g2_attacked_by_black_pawn_on_h3);

    /* ── Perft Tests ── */
    RUN_TEST(test_perft_startpos_depth0);
    RUN_TEST(test_perft_startpos_depth1);
    RUN_TEST(test_perft_startpos_depth2);
    RUN_TEST(test_perft_startpos_depth3);
    RUN_TEST(test_perft_startpos_depth4);
    RUN_TEST(test_perft_kiwipete_depth1);
    RUN_TEST(test_perft_kiwipete_depth2);
    RUN_TEST(test_perft_kiwipete_depth3);
    RUN_TEST(test_perft_pos3_depth1);
    RUN_TEST(test_perft_pos3_depth2);
    RUN_TEST(test_perft_pos3_depth3);
    RUN_TEST(test_perft_pos4_depth1);
    RUN_TEST(test_perft_pos4_depth2);
    RUN_TEST(test_perft_pos4_depth3);
    RUN_TEST(test_perft_pos5_depth1);
    RUN_TEST(test_perft_pos5_depth2);
    RUN_TEST(test_perft_pos6_depth1);
    RUN_TEST(test_perft_pos6_depth2);

    /* ── Zobrist Tests ── */
    RUN_TEST(test_zobrist_hash_keys_startpos);
    RUN_TEST(test_zobrist_hash_keys_kiwipete);
    RUN_TEST(test_zobrist_hash_keys_pos4);
    RUN_TEST(test_zobrist_hash_keys_pos3);
    RUN_TEST(test_zobrist_hash_keys_pos5);
    RUN_TEST(test_zobrist_hash_keys_pos6);
    RUN_TEST(test_zobrist_castling_white_ks);
    RUN_TEST(test_zobrist_castling_white_qs);
    RUN_TEST(test_zobrist_castling_black_ks);
    RUN_TEST(test_zobrist_castling_black_qs);
    RUN_TEST(test_zobrist_en_passant_capture_white);
    RUN_TEST(test_zobrist_en_passant_capture_black);
    RUN_TEST(test_zobrist_en_passant_ignored_cleared);
    RUN_TEST(test_zobrist_promotions_and_promo_captures);
    RUN_TEST(test_zobrist_castling_rights_loss_king_move);
    RUN_TEST(test_zobrist_castling_rights_loss_rook_move);
    RUN_TEST(test_zobrist_castling_rights_loss_rook_captured);

    /* ── King Safety / Promotion Capture Tests ── */
    RUN_TEST(test_legal_movegen_ep_discovered_check);
    RUN_TEST(test_legal_movegen_castling_restrictions);
    RUN_TEST(test_legal_movegen_double_check_evasions);
    RUN_TEST(test_legal_movegen_absolute_pins);
    RUN_TEST(test_legal_movegen_safety_recursive_startpos);
    RUN_TEST(test_legal_movegen_safety_recursive_kiwipete);
    RUN_TEST(test_legal_movegen_safety_recursive_pos3);
    RUN_TEST(test_legal_movegen_safety_recursive_pos4);
    RUN_TEST(test_legal_movegen_safety_recursive_pos5);
    RUN_TEST(test_legal_movegen_safety_recursive_pos6);
    RUN_TEST(test_promotion_captures_all_combinations);
    RUN_TEST(test_promotion_captures_castling_rights);
    RUN_TEST(test_promotion_captures_movegen_pinned);
    RUN_TEST(test_promotion_captures_movegen_resolves_check);

    return UNITY_END();
}
