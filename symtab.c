#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symtab.h"

static int hash(char *lexema)
{
    unsigned int valor = 0;

    while (*lexema)
    {
        valor = valor * 31u + (unsigned char)*lexema;
        lexema++;
    }

    return valor % TAM_TABELA;
}

void inicializa_tabela(TabelaSimbolos *tabela)
{
    for (int i = 0; i < TAM_TABELA; i++)
        tabela->tabela[i] = NULL;
    tabela->primeiro = NULL;
    tabela->ultimo = NULL;
}

int insere_simbolo(
    TabelaSimbolos *tabela,
    char *lexema,
    TCategoriaSimbolo categoria,
    TTipo tipo,
    TEscopo escopo)
{
    return insere_simbolo_contexto(tabela, lexema, categoria, tipo, escopo, "global");
}

int insere_simbolo_contexto(
    TabelaSimbolos *tabela,
    char *lexema,
    TCategoriaSimbolo categoria,
    TTipo tipo,
    TEscopo escopo,
    const char *contexto)
{
    int posicao = hash(lexema);
    TSimbolo *existente = tabela->tabela[posicao];

    while (existente != NULL)
    {
        if (strcmp(existente->lexema, lexema) == 0 &&
            existente->escopo == escopo &&
            strcmp(existente->contexto, contexto) == 0)
            return 0;
        existente = existente->proximo;
    }

    TSimbolo *novo = malloc(sizeof(TSimbolo));
    if (novo == NULL) return 0;

    strcpy(novo->lexema, lexema);
    novo->categoria = categoria;
    novo->tipo = tipo;
    novo->escopo = escopo;
    novo->tamanho_vetor = 0;
    novo->quantidade_parametros = 0;
    strcpy(novo->contexto, contexto);
    novo->ordem_proximo = NULL;
    novo->proximo = tabela->tabela[posicao];
    tabela->tabela[posicao] = novo;

    if (tabela->primeiro == NULL)
        tabela->primeiro = novo;
    else
        tabela->ultimo->ordem_proximo = novo;
    tabela->ultimo = novo;

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
                "%-15s | %-12s | %-10s | %-10s",
                atual->lexema,
                nome_categoria(atual->categoria),
                nome_tipo(atual->tipo),
                nome_escopo(atual->escopo));

            if (atual->tamanho_vetor > 0)
                printf(" | VETOR=%d", atual->tamanho_vetor);

            if (atual->quantidade_parametros > 0)
                printf(" | PARAMETROS=%d", atual->quantidade_parametros);

            printf("\n");

            atual = atual->proximo;
        }
    }
}


void grava_tabela_log(TabelaSimbolos *tabela, FILE *arquivo)
{
    TSimbolo *atual = tabela->primeiro;
    while (atual != NULL)
    {
        if (atual->escopo == GLOBAL)
            fprintf(arquivo, "SCOPE=global ");
        else if (atual->escopo == PARAMETRO)
            fprintf(arquivo, "SCOPE=proc:%s.params ", atual->contexto);
        else
            fprintf(arquivo, "SCOPE=proc:%s.locals ", atual->contexto);

        fprintf(arquivo, "id=\"%s\" cat=%s tipo=%s extra=",
                atual->lexema, nome_categoria(atual->categoria), nome_tipo(atual->tipo));

        if (atual->tamanho_vetor > 0)
            fprintf(arquivo, "VETOR=%d", atual->tamanho_vetor);
        else if (atual->quantidade_parametros > 0)
            fprintf(arquivo, "PARAMETROS=%d", atual->quantidade_parametros);
        else
            fprintf(arquivo, "-");

        fprintf(arquivo, "\n");
        atual = atual->ordem_proximo;
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

    case TIPO_STRING:
        return "STRING";

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
TSimbolo *busca_simbolo_escopo(
    TabelaSimbolos *tabela,
    char *lexema,
    TEscopo escopo)
{
    int posicao = hash(lexema);
    TSimbolo *atual = tabela->tabela[posicao];

    while (atual != NULL)
    {
        if (strcmp(atual->lexema, lexema) == 0 && atual->escopo == escopo)
            return atual;
        atual = atual->proximo;
    }

    return NULL;
}

TSimbolo *busca_simbolo_contexto(
    TabelaSimbolos *tabela,
    const char *lexema,
    TEscopo escopo,
    const char *contexto)
{
    int posicao = hash((char *)lexema);
    TSimbolo *atual = tabela->tabela[posicao];
    while (atual != NULL)
    {
        if (strcmp(atual->lexema, lexema) == 0 &&
            atual->escopo == escopo &&
            strcmp(atual->contexto, contexto) == 0)
            return atual;
        atual = atual->proximo;
    }
    return NULL;
}
