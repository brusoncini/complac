#include <stdio.h>
#include <string.h>

#include "gerador.h"
#include "diag.h"

static FILE *codigo = NULL;
static int contador_rotulos = 0;

int gerador_inicia(void)
{
    contador_rotulos = 0;
    codigo = tmpfile();
    return codigo != NULL;
}

void gera_instr_mepa(const char *rotulo, const char *mnemonico,
                     const char *parametro1, const char *parametro2)
{
    char prefixo[TAM_ROTULO + 2] = "";

    if (codigo == NULL)
        return;

    if (rotulo != NULL)
        snprintf(prefixo, sizeof(prefixo), "%s:", rotulo);

    fprintf(codigo, "%-6s%s", prefixo, mnemonico);

    /* O interpretador nao aceita espacos entre os dois parametros */
    if (parametro1 != NULL)
        fprintf(codigo, " %s", parametro1);
    if (parametro2 != NULL)
        fprintf(codigo, ",%s", parametro2);

    fputc('\n', codigo);
}

void novo_rotulo(TRotulo rotulo)
{
    snprintf(rotulo, TAM_ROTULO, "L%d", ++contador_rotulos);
}

void gera0(const char *mnemonico)
{
    gera_instr_mepa(NULL, mnemonico, NULL, NULL);
}

void gera1(const char *mnemonico, int parametro)
{
    char p[32];
    snprintf(p, sizeof(p), "%d", parametro);
    gera_instr_mepa(NULL, mnemonico, p, NULL);
}

void gera2(const char *mnemonico, int parametro1, int parametro2)
{
    char p1[32], p2[32];
    snprintf(p1, sizeof(p1), "%d", parametro1);
    snprintf(p2, sizeof(p2), "%d", parametro2);
    gera_instr_mepa(NULL, mnemonico, p1, p2);
}

void gera_desvio(const char *mnemonico, const char *rotulo)
{
    gera_instr_mepa(NULL, mnemonico, rotulo, NULL);
}

void gera_rotulo(const char *rotulo)
{
    gera_instr_mepa(rotulo, "NADA", NULL, NULL);
}

/* Grava o arquivo .mepa somente em caso de sucesso. Em caso de erro,
   remove um eventual .mepa antigo para nao deixar codigo desatualizado. */
int gerador_finaliza(const char *destino, int sucesso)
{
    int ok = 0;

    if (codigo == NULL)
    {
        remove(destino);
        return 0;
    }

    if (sucesso)
    {
        FILE *saida = fopen(destino, "w");
        if (saida == NULL)
        {
            diag_error(0, "arquivo", "nao foi possivel gravar o arquivo MEPA", destino);
        }
        else
        {
            int c;
            rewind(codigo);
            while ((c = fgetc(codigo)) != EOF)
                fputc(c, saida);
            fclose(saida);
            printf("Codigo MEPA gerado: %s\n", destino);
            ok = 1;
        }
    }
    else
    {
        remove(destino);
    }

    fclose(codigo);
    codigo = NULL;
    return ok;
}
