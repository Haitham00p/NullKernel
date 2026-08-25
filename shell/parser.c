#include "parser.h"
#include <stdint.h>
#include <stddef.h>
#include "mm/heap/heap.h"
#include "lib/string/string.h"

/* Duplicates the first Len bytes of Src into a fresh heap string (NUL added).
   Returns NULL when the allocation fails. */
static char *ParserDup(const char *Src, uint32_t Len)
{
    char *Out = (char *)kmalloc((uint64_t)Len + 1U);
    if (Out == NULL) return NULL;
    for (uint32_t i = 0; i < Len; i++)
    {
        Out[i] = Src[i];
    }
    Out[Len] = '\0';
    return Out;
}

void ParserParse(const char *Input, PARSED_COMMAND *Output)
{
    Output->Command = NULL;
    Output->Args = NULL;
    Output->ArgCount = 0;

    if (Input == NULL) return;

    uint32_t Index = 0;

    /* Skip leading whitespace. */
    while (Input[Index] == ' ' || Input[Index] == '\t') Index++;

    /* Command name: up to the next whitespace. */
    uint32_t CmdStart = Index;
    while (Input[Index] != '\0' && Input[Index] != ' ' && Input[Index] != '\t')
    {
        Index++;
    }
    Output->Command = ParserDup(Input + CmdStart, Index - CmdStart);
    if (Output->Command == NULL) return;

    /* Skip whitespace between the name and the first argument. */
    while (Input[Index] == ' ' || Input[Index] == '\t') Index++;

    /* Count arguments so the vector can be allocated once. */
    uint32_t Count = 0;
    uint32_t Tmp = Index;
    while (Input[Tmp] != '\0')
    {
        while (Input[Tmp] == ' ' || Input[Tmp] == '\t') Tmp++;
        if (Input[Tmp] == '\0') break;
        Count++;
        while (Input[Tmp] != '\0' && Input[Tmp] != ' ' && Input[Tmp] != '\t')
        {
            Tmp++;
        }
    }

    if (Count == 0) return;

    Output->Args = (char **)kmalloc((uint64_t)sizeof(char *) * (uint64_t)Count);
    if (Output->Args == NULL) return;

    uint32_t ArgIdx = 0;
    while (Input[Index] != '\0' && ArgIdx < Count)
    {
        while (Input[Index] == ' ' || Input[Index] == '\t') Index++;
        uint32_t AStart = Index;
        while (Input[Index] != '\0' && Input[Index] != ' ' && Input[Index] != '\t')
        {
            Index++;
        }
        Output->Args[ArgIdx] = ParserDup(Input + AStart, Index - AStart);
        ArgIdx++;
    }
    Output->ArgCount = Count;
}

void ParserFree(PARSED_COMMAND *Cmd)
{
    if (Cmd == NULL) return;

    if (Cmd->Command != NULL)
    {
        kfree(Cmd->Command);
        Cmd->Command = NULL;
    }
    if (Cmd->Args != NULL)
    {
        for (uint32_t i = 0; i < Cmd->ArgCount; i++)
        {
            if (Cmd->Args[i] != NULL) kfree(Cmd->Args[i]);
        }
        kfree(Cmd->Args);
        Cmd->Args = NULL;
    }
    Cmd->ArgCount = 0;
}
