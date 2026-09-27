#include <stdio.h>
#include <string.h>
#include "semantic.h"
#include "diag.h"

static int erros_semanticos;

void inicializa_semantica(void)
{
    erros_semanticos = 0;
}

void erro_semantico(int linha, const char *mensagem, const char *lexema)
{
    diag_error(linha, "semantico", mensagem, lexema);
    erros_semanticos++;
}

int quantidade_erros_semanticos(void)
{
    return erros_semanticos;
}

TSimbolo *busca_no_escopo(
    TabelaSimbolos *tabela,
    const char *lexema,
    TEscopo escopo_atual,
    const char *contexto)
{
    TSimbolo *simbolo;

    if (escopo_atual != GLOBAL)
    {
        simbolo = busca_simbolo_contexto(tabela, lexema, PARAMETRO, contexto);
        if (simbolo != NULL)
            return simbolo;

        simbolo = busca_simbolo_contexto(tabela, lexema, LOCAL, contexto);
        if (simbolo != NULL)
            return simbolo;
    }

    return busca_simbolo_contexto(tabela, lexema, GLOBAL, "global");
}
