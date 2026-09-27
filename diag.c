#include <stdio.h>
#include "diag.h"

static int erros;
static FILE *trace_file;

void diag_init(void)
{
    erros = 0;
    trace_file = NULL;
}

void diag_set_trace(FILE *arquivo)
{
    trace_file = arquivo;
}

void diag_error(int linha, const char *tipo, const char *mensagem, const char *lexema)
{
    if (lexema != NULL && lexema[0] != '\0')
        fprintf(stderr, "Erro %s na linha %d: %s (%s)\n", tipo, linha, mensagem, lexema);
    else
        fprintf(stderr, "Erro %s na linha %d: %s\n", tipo, linha, mensagem);
    erros++;
}

void diag_info(const char *mensagem)
{
    printf("%s\n", mensagem);
}

void diag_trace(const char *evento, const char *nao_terminal, int linha)
{
    if (trace_file != NULL)
        fprintf(trace_file, "%s %s linha=%d\n", evento, nao_terminal, linha);
}

int diag_errors(void)
{
    return erros;
}
