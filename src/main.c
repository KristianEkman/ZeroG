#include "boards.h"
#include "uci.h"

int main(void)
{
    bitboard_init();

    uci_run(stdin, stdout);
    return 0;
}