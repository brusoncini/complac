#include "lex.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Uso: %s arquivo.slac\n", argv[0]);
        return 1;
    }

    lex_init(argv[1]);

    TInfoAtomo token;

    do
    {
        token = lex_next();

        printf(
            "%d <%s> \"%s\"\n",
            token.linha,
            lex_token_name(token.atomo),
            token.lexema);

    } while (token.atomo != S_FIM);

    return 0;
}