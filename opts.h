#ifndef OPTS_H
#define OPTS_H

typedef enum
{
    OPCAO_NENHUMA,
    OPCAO_TOKENS,
    OPCAO_SYMTAB,
    OPCAO_TRACE
} TOpcao;

typedef struct
{
    const char *arquivo;
    TOpcao opcao;
} TOpcoes;

int opts_parse(int argc, char **argv, TOpcoes *opcoes);
void opts_uso(const char *programa);

#endif
