#include "uci/uci_internal.h"
#include "eval.h"
#include "search/threads.h"
#include <stdio.h>
#include <string.h>

static char uci_save_quiet_positions_file[512] = "";

const char* uci_get_save_quiet_positions_file(void)
{
    return uci_save_quiet_positions_file;
}

static int write_uci_handshake(FILE *output) {
  if (fprintf(output, "id name ZeroG 1.0.0\n") < 0 ||
      fprintf(output, "id author Kristian Ekman\n") < 0 ||
      fprintf(output,
              "option name Hash type spin default 16 min 1 max 1024\n") < 0 ||
      fprintf(output,
              "option name Threads type spin default 1 min 1 max 64\n") < 0 ||
      fprintf(output,
              "option name LMR_Base type spin default 2 min 0 max 20\n") < 0 ||
      fprintf(
          output,
          "option name Futility_Margin type spin default 114 min 0 max 500\n") <
          0 ||
      fprintf(output,
              "option name RFP_Margin type spin default 111 min 0 max 300\n") <
          0 ||
      fprintf(output,
              "option name NMP_Min_Depth type spin default 2 min 1 max 10\n") <
          0 ||
      fprintf(
          output,
          "option name Singular_Margin type spin default 2 min 0 max 10\n") <
          0 ||
      fprintf(output, "option name Aspiration_Window type spin default 35 min "
                      "5 max 200\n") < 0 ||
      fprintf(output,
              "option name LMR_Min_Depth type spin default 2 min 1 max 15\n") <
          0 ||
      fprintf(
          output,
          "option name Futility_Max_Depth type spin default 4 min 1 max 5\n") <
          0 ||
      fprintf(output, "option name LMR_History_Divisor type spin default 2000 min 100 max 100000\n") < 0 ||
      fprintf(output, "option name SaveQuietPositionsFile type string default <empty>\n") < 0 ||
      fprintf(output, "option name UseNN type check default false\n") < 0 ||
      fprintf(output, "uciok\n") < 0) {
    return -1;
  }

  fflush(output);
  return 0;
}

int uci_handle_line(UciState *state, const char *line, FILE *output,
                    int *should_quit) {
  const char *args;

  if (state == NULL || line == NULL || output == NULL || should_quit == NULL) {
    return -1;
  }

  *should_quit = 0;
  line = uci_skip_spaces(line);

  if (*line == '\0') {
    return 0;
  }

  if (uci_starts_with_keyword(line, "uci", &args)) {
    (void)args;
    return write_uci_handshake(output);
  }

  if (uci_starts_with_keyword(line, "isready", &args)) {
    (void)args;
    if (fprintf(output, "readyok\n") < 0) {
      return -1;
    }

    fflush(output);
    return 0;
  }

  if (uci_starts_with_keyword(line, "ucinewgame", &args)) {
    (void)args;
    return uci_set_start_position(&state->board);
  }

  if (uci_starts_with_keyword(line, "position", &args)) {
    return uci_apply_position(state, args);
  }

  if (uci_starts_with_keyword(line, "go", &args)) {
    SearchResult result;
    SearchLimits limits;

    if (uci_parse_go_search_limits(state, args, &limits) != 0) {
      return -1;
    }

    if (uci_search_with_limits(&state->board, &limits, output, &result) != 0) {
      return -1;
    }

    return uci_write_bestmove(&state->board, output,
                              result.has_legal_move ? result.best_move : 0);
  }

  if (uci_starts_with_keyword(line, "setoption", &args)) {
    unsigned hash_size_mb;
    int spin_val;

    if (uci_parse_hash_option_value(args, &hash_size_mb) == 0) {
      return search_set_hash_size_mb(hash_size_mb);
    } else if (uci_parse_spin_option_value(args, "LMR_Base", &spin_val) == 0) {
      return search_set_lmr_base(spin_val);
    } else if (uci_parse_spin_option_value(args, "Futility_Margin",
                                           &spin_val) == 0) {
      return search_set_futility_margin(spin_val);
    } else if (uci_parse_spin_option_value(args, "RFP_Margin", &spin_val) ==
               0) {
      return search_set_rfp_margin(spin_val);
    } else if (uci_parse_spin_option_value(args, "NMP_Min_Depth", &spin_val) ==
               0) {
      return search_set_nmp_min_depth(spin_val);
    } else if (uci_parse_spin_option_value(args, "Singular_Margin",
                                           &spin_val) == 0) {
      return search_set_singular_margin(spin_val);
    } else if (uci_parse_spin_option_value(args, "Aspiration_Window",
                                           &spin_val) == 0) {
      return search_set_aspiration_window(spin_val);
    } else if (uci_parse_spin_option_value(args, "LMR_Min_Depth", &spin_val) ==
               0) {
      return search_set_lmr_min_depth(spin_val);
    } else if (uci_parse_spin_option_value(args, "Futility_Max_Depth",
                                           &spin_val) == 0) {
      return search_set_futility_max_depth(spin_val);
    } else if (uci_parse_spin_option_value(args, "LMR_History_Divisor",
                                           &spin_val) == 0) {
      return search_set_lmr_history_divisor(spin_val);
    } else if (uci_parse_spin_option_value(args, "Threads", &spin_val) == 0) {
      return search_set_threads(spin_val);
    }

    char path_buf[512];
    if (uci_parse_string_option_value(args, "SaveQuietPositionsFile", path_buf, sizeof(path_buf)) == 0) {
      if (strcmp(path_buf, "<empty>") == 0) {
        uci_save_quiet_positions_file[0] = '\0';
      } else {
        strncpy(uci_save_quiet_positions_file, path_buf, sizeof(uci_save_quiet_positions_file));
        uci_save_quiet_positions_file[sizeof(uci_save_quiet_positions_file) - 1] = '\0';
      }
      return 0;
    }

    if (uci_parse_string_option_value(args, "UseNN", path_buf, sizeof(path_buf)) == 0) {
      if (strcmp(path_buf, "true") == 0) {
        if (eval_nn) use_nn = true;
      } else if (strcmp(path_buf, "false") == 0) {
        use_nn = false;
      }
      return 0;
    }

    return -1;
  }

  if (uci_starts_with_keyword(line, "stop", &args) ||
      uci_starts_with_keyword(line, "ponderhit", &args)) {
    (void)args;
    return 0;
  }

  if (uci_starts_with_keyword(line, "quit", &args)) {
    (void)args;
    *should_quit = 1;
    return 0;
  }

  return -1;
}