#ifndef PARSER_H
#define PARSER_H

#include "symtab.h"

int analisa_sintaxe(const char *nome_arquivo);
TabelaSimbolos *obtem_tabela_parser(void);

#endif
