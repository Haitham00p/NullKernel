#include <kernel/terminal/terminal.h>
#include <stdlib.h>
#include <string.c>

void main(void)
{
    int BUF[32];
    int n
    int bgCOLOR
    n = readv("/REG/COLOR/BG.V", BUF);
    BUF[n] = '\0';
    StringHexToUInt(BUF, &bgCOLOR);
    
}
