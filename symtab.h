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
    TIPO_STRING,
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
    char contexto[TAM_NOME];

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

int insere_simbolo_contexto(
    TabelaSimbolos *tabela,
    char *lexema,
    TCategoriaSimbolo categoria,
    TTipo tipo,
    TEscopo escopo,
    const char *contexto);

TSimbolo *busca_simbolo(
    TabelaSimbolos *tabela,
    char *lexema);

TSimbolo *busca_simbolo_escopo(
    TabelaSimbolos *tabela,
    char *lexema,
    TEscopo escopo);

TSimbolo *busca_simbolo_contexto(
    TabelaSimbolos *tabela,
    const char *lexema,
    TEscopo escopo,
    const char *contexto);

void imprime_tabela(TabelaSimbolos *tabela);

char *nome_tipo(TTipo tipo);

char *nome_categoria(TCategoriaSimbolo categoria);

char *nome_escopo(TEscopo escopo);

#endif