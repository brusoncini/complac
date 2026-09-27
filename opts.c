#include <stdio.h>
#include <string.h>
#include "opts.h"

void opts_uso(const char *programa)
{
    fprintf(stderr, "Uso: %s <arquivo.slac> [--tokens | --symtab | --trace]\n", programa);
}

int opts_parse(int argc, char **argv, TOpcoes *opcoes)
{
    if (argc < 2 || argc > 3)
    {
        opts_uso(argv[0]);
        return 0;
    }

    opcoes->arquivo = argv[1];
    opcoes->opcao = OPCAO_NENHUMA;

    if (argc == 3)
    {
        if (strcmp(argv[2], "--tokens") == 0)
            opcoes->opcao = OPCAO_TOKENS;
        else if (strcmp(argv[2], "--symtab") == 0)
            opcoes->opcao = OPCAO_SYMTAB;
        else if (strcmp(argv[2], "--trace") == 0)
            opcoes->opcao = OPCAO_TRACE;
        else
        {
            opts_uso(argv[0]);
            return 0;
        }
    }

    return 1;
}
