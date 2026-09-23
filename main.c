#include <stdio.h>

#include "lex.h"

int main(int argc, char **argv)
{

    /*
    ===============================
        TESTE ANALISADOR LÉXICO
    ===============================
    */

    if (argc < 2)
    {
        printf("Uso: %s arquivo.slac\n", argv[0]);
        return 1;
    }

    FILE *arquivo = fopen(argv[1], "r");

    if (arquivo == NULL)
    {
        printf("Erro ao abrir arquivo\n");
        return 1;
    }

    inicializa_lex(arquivo);

    TInfoAtomo atomo;

    do
    {
        atomo = proximo_atomo();

        printf(
            "%d | %-25s | %s\n",
            atomo.linha,
            nome_atomo(atomo.atomo),
            atomo.texto);

    } while (atomo.atomo != FIMARQUIVO);

    fclose(arquivo);

    return 0;
}