#include "boards.h"

int main(void)
{
    Board board;
    board_startpos(&board);
    board_print(&board);
    return 0;
}
