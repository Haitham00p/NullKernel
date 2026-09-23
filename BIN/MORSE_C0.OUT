#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static void MorseCode(const char *p)
{
    uint32_t i;

    for (i = 0; p[i] != '\0'; i++)
    {
        putchar(p[i]);
    }
    putchar(' ');
}

static void MorseChar(char c)
{
    if (c == ' ')
    {
        putchar('/');
        putchar(' ');
        return;
    }
    if (c >= 'A' && c <= 'Z')
    {
        c = (char)(c + 32);
    }
    if (c == 'a')
    {
        MorseCode(".-");
    }
    else if (c == 'b')
    {
        MorseCode("-...");
    }
    else if (c == 'c')
    {
        MorseCode("-.-.");
    }
    else if (c == 'd')
    {
        MorseCode("-..");
    }
    else if (c == 'e')
    {
        MorseCode(".");
    }
    else if (c == 'f')
    {
        MorseCode("..-.");
    }
    else if (c == 'g')
    {
        MorseCode("--.");
    }
    else if (c == 'h')
    {
        MorseCode("....");
    }
    else if (c == 'i')
    {
        MorseCode("..");
    }
    else if (c == 'j')
    {
        MorseCode(".---");
    }
    else if (c == 'k')
    {
        MorseCode("-.-");
    }
    else if (c == 'l')
    {
        MorseCode(".-..");
    }
    else if (c == 'm')
    {
        MorseCode("--");
    }
    else if (c == 'n')
    {
        MorseCode("-.");
    }
    else if (c == 'o')
    {
        MorseCode("---");
    }
    else if (c == 'p')
    {
        MorseCode(".--.");
    }
    else if (c == 'q')
    {
        MorseCode("--.-");
    }
    else if (c == 'r')
    {
        MorseCode(".-.");
    }
    else if (c == 's')
    {
        MorseCode("...");
    }
    else if (c == 't')
    {
        MorseCode("-");
    }
    else if (c == 'u')
    {
        MorseCode("..-");
    }
    else if (c == 'v')
    {
        MorseCode("...-");
    }
    else if (c == 'w')
    {
        MorseCode(".--");
    }
    else if (c == 'x')
    {
        MorseCode("-..-");
    }
    else if (c == 'y')
    {
        MorseCode("-.--");
    }
    else if (c == 'z')
    {
        MorseCode("--..");
    }
    else
    {
        putchar('?');
    }
    putchar(' ');
}

void main(void)
{
    char arg[256];
    uint32_t i;

    if (ShellArgCount() < 1)
    {
        puts("usage: morse <text>  (e.g. morse SOS)");
        return;
    }
    ShellArgGet(0, arg, (uint32_t)sizeof(arg));
    for (i = 0; arg[i] != '\0'; i++)
    {
        MorseChar(arg[i]);
    }
    putchar('\n');
}