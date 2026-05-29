#include "boards.h"
#include "uci.h"
#include "eval.h"

int main(void)
{
    bitboard_init();

    uci_run(stdin, stdout);

    eval_free();
    return 0;
}