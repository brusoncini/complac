#ifndef SYMTAB_H
#define SYMTAB_H

#include <stdio.h>

#define TAM_TABELA 211
#define TAM_NOME 256
#define MAX_PARAMETROS 32

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
    int endereco;          /* deslocamento na MEPA (k em CRVL m,k) */
    int nivel;             /* nivel lexico na MEPA (m em CRVL m,k) */
    int eh_referencia;     /* parametro passado por referencia (ref) */
    int quantidade_parametros;
    int tamanho_parametros; /* posicoes de memoria ocupadas pelos parametros */
    char rotulo[16];       /* rotulo MEPA de entrada da sub-rotina */
    struct Simbolo *parametros[MAX_PARAMETROS];
    char contexto[TAM_NOME];
    struct Simbolo *proximo;
    struct Simbolo *ordem_proximo;
} TSimbolo;

typedef struct
{
    TSimbolo *tabela[TAM_TABELA];
    TSimbolo *primeiro;
    TSimbolo *ultimo;
} TabelaSimbolos;

void inicializa_tabela(TabelaSimbolos *tabela);
int insere_simbolo(TabelaSimbolos *tabela, char *lexema,
    TCategoriaSimbolo categoria, TTipo tipo, TEscopo escopo);
int insere_simbolo_contexto(TabelaSimbolos *tabela, char *lexema,
    TCategoriaSimbolo categoria, TTipo tipo, TEscopo escopo,
    const char *contexto);
TSimbolo *busca_simbolo(TabelaSimbolos *tabela, char *lexema);
TSimbolo *busca_simbolo_escopo(TabelaSimbolos *tabela,
    char *lexema, TEscopo escopo);
TSimbolo *busca_simbolo_contexto(TabelaSimbolos *tabela,
    const char *lexema, TEscopo escopo, const char *contexto);
TSimbolo *busca_no_escopo(TabelaSimbolos *tabela,
    const char *lexema, TEscopo escopo_atual, const char *contexto);
void imprime_tabela(TabelaSimbolos *tabela);
void grava_tabela_log(TabelaSimbolos *tabela, FILE *arquivo);
char *nome_tipo(TTipo tipo);
char *nome_categoria(TCategoriaSimbolo categoria);
char *nome_escopo(TEscopo escopo);

#endif
