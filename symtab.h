#ifndef SYMTAB_H
#define SYMTAB_H

#define TAM_TABELA 211
#define TAM_NOME 256

typedef enum
{
    GLOBAL,
    LOCAL,
    PARAMETRO

} TEscopo;

typedef enum
{
    VARIAVEL,
    SUBROTINA

} TCategoriaSimbolo;

typedef enum
{
    TIPO_INT,
    TIPO_LOGIC,
    TIPO_CHR,
    TIPO_VOID,
    TIPO_NAO_DEFINIDO

} TTipo;

typedef struct Simbolo
{
    char lexema[TAM_NOME];

    TCategoriaSimbolo categoria;

    TTipo tipo;

    TEscopo escopo;

    int tamanho_vetor;

    int quantidade_parametros;

    struct Simbolo *proximo;

} TSimbolo;

typedef struct
{
    TSimbolo *tabela[TAM_TABELA];

} TabelaSimbolos;

void inicializa_tabela(TabelaSimbolos *tabela);

int insere_simbolo(
    TabelaSimbolos *tabela,
    char *lexema,
    TCategoriaSimbolo categoria,
    TTipo tipo,
    TEscopo escopo);

TSimbolo *busca_simbolo(
    TabelaSimbolos *tabela,
    char *lexema);

void imprime_tabela(TabelaSimbolos *tabela);

char *nome_tipo(TTipo tipo);

char *nome_categoria(TCategoriaSimbolo categoria);

char *nome_escopo(TEscopo escopo);

#endif