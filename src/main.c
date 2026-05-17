#include "boards.h"

int main(void)
{
    bitboard_init();

    Position pos;
    position_startpos(&pos);
    position_print(&pos);
    return 0;
}