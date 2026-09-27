#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "symtab.h"

void inicializa_semantica(void);
void erro_semantico(int linha, const char *mensagem, const char *lexema);
int quantidade_erros_semanticos(void);

TSimbolo *busca_no_escopo(
    TabelaSimbolos *tabela,
    const char *lexema,
    TEscopo escopo_atual,
    const char *contexto);

#endif
