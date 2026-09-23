#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symtab.h"

static int hash(char *lexema)
{
    int valor = 0;

    while (*lexema)
    {
        valor = valor * 31 + *lexema;
        lexema++;
    }

    return valor % TAM_TABELA;
}

void inicializa_tabela(TabelaSimbolos *tabela)
{
    for (int i = 0; i < TAM_TABELA; i++)
        tabela->tabela[i] = NULL;
}

int insere_simbolo(
    TabelaSimbolos *tabela,
    char *lexema,
    TCategoriaSimbolo categoria,
    TTipo tipo,
    TEscopo escopo)
{
    int posicao = hash(lexema);

    TSimbolo *novo = malloc(sizeof(TSimbolo));

    if (novo == NULL)
        return 0;

    strcpy(novo->lexema, lexema);

    novo->categoria = categoria;
    novo->tipo = tipo;
    novo->escopo = escopo;
    novo->tamanho_vetor = 0;
    novo->quantidade_parametros = 0;
    novo->proximo = tabela->tabela[posicao];

    tabela->tabela[posicao] = novo;

    return 1;
}

TSimbolo *busca_simbolo(
    TabelaSimbolos *tabela,
    char *lexema)
{

    int posicao = hash(lexema);

    TSimbolo *atual = tabela->tabela[posicao];

    while (atual != NULL)
    {

        if (strcmp(atual->lexema, lexema) == 0)
            return atual;

        atual = atual->proximo;
    }

    return NULL;
}

void imprime_tabela(TabelaSimbolos *tabela)
{
    printf("\n===== TABELA DE SIMBOLOS =====\n\n");

    for (int i = 0; i < TAM_TABELA; i++)
    {
        TSimbolo *atual = tabela->tabela[i];

        while (atual != NULL)
        {

            printf(
                "%-15s | %-12s | %-10s | %-10s\n",
                atual->lexema,
                nome_categoria(atual->categoria),
                nome_tipo(atual->tipo),
                nome_escopo(atual->escopo));

            atual = atual->proximo;
        }
    }
}

char *nome_tipo(TTipo tipo)
{
    switch (tipo)
    {
    case TIPO_INT:
        return "INT";

    case TIPO_LOGIC:
        return "LOGIC";

    case TIPO_CHR:
        return "CHR";

    case TIPO_VOID:
        return "VOID";

    default:
        return "NDEF";
    }
}

char *nome_categoria(TCategoriaSimbolo categoria)
{
    switch (categoria)
    {
    case VARIAVEL:
        return "VARIAVEL";

    case SUBROTINA:
        return "SUBROTINA";

    default:
        return "OUTRO";
    }
}

char *nome_escopo(TEscopo escopo)
{
    switch (escopo)
    {
    case GLOBAL:
        return "GLOBAL";

    case LOCAL:
        return "LOCAL";

    case PARAMETRO:
        return "PARAMETRO";

    default:
        return "OUTRO";
    }
}