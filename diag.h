#ifndef DIAG_H
#define DIAG_H

#include <stdio.h>

void diag_init(void);
void diag_set_trace(FILE *arquivo);
void diag_error(int linha, const char *tipo, const char *mensagem, const char *lexema);
void diag_info(const char *mensagem);
void diag_trace(const char *evento, const char *nao_terminal, int linha);
int diag_errors(void);

#endif
