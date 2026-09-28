#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"
#include "parser.h"
#include "opts.h"
#include "diag.h"

static const char *categoria_log(TAtomo atomo)
{
    switch (atomo)
    {
    case IDENTIFICADOR: return "sIDENTIF";
    case CONSTANTE_INTEIRA: return "sCTEINT";
    case CONSTANTE_CARACTERE: return "sCTECHAR";
    case STRING: return "sSTRING";
    case PROC: return "sPROC";
    case FUNC: return "sFUNC";
    case START: return "sSTART";
    case END: return "sEND";
    case GLOBVARS: return "sGLOBVARS";
    case LOCVARS: return "sLOCVARS";
    case KIND: return "sKIND";
    case REF: return "sREF";
    case ECHO: return "sECHO";
    case GET: return "sGET";
    case CASE: return "sCASE";
    case OTHERWISE: return "sOTHERWISE";
    case CHOOSE: return "sCHOOSE";
    case MATCH: return "sMATCH";
    case OTHERS: return "sOTHERS";
    case FOR: return "sFOR";
    case FROM: return "sFROM";
    case TO: return "sTO";
    case BY: return "sBY";
    case DO: return "sDO";
    case WHILE: return "sWHILE";
    case REPEAT: return "sREPEAT";
    case UNTIL: return "sUNTIL";
    case RETURN: return "sRETURN";
    case INT: return "sINT";
    case LOGIC: return "sLOGIC";
    case CHR: return "sCHR";
    case ATRIBUICAO: return "sATRIB";
    case SOMA: return "sSOMA";
    case SUBTRACAO: return "sSUBRAT";
    case MULTIPLICACAO: return "sMULT";
    case DIVISAO: return "sDIV";
    case IGUAL: return "sIGUAL";
    case DIFERENTE: return "sDIFERENTE";
    case MAIOR: return "sMAIOR";
    case MENOR: return "sMENOR";
    case MAIORIGUAL: return "sMAIORIGUAL";
    case MENORIGUAL: return "sMENORIGUAL";
    case E_LOGICO: return "sAND";
    case OU_LOGICO: return "sOR";
    case NEGACAO: return "sNEG";
    case IMPLIC: return "sIMPLIC";
    case ABREPARENTESES: return "sABREPAREN";
    case FECHAPARENTESES: return "sFECHAPAREN";
    case ABRECOLCHETE: return "sABRECOLCH";
    case FECHACOLCHETE: return "sFECHACOLCH";
    case VIRGULA: return "sVIRGULA";
    case PONTOEVIRGULA: return "sPONTOVIRG";
    case DOISPONTOS: return "sDOISPONTOS";
    case ERRO: return "sERRO";
    case FIMARQUIVO: return "sEOF";
    default: return "sDESCONHECIDO";
    }
}

static void nome_log(const char *fonte, const char *ext, char *saida, size_t tamanho)
{
    snprintf(saida, tamanho, "%s", fonte);
    char *ponto = strrchr(saida, '.');
    if (ponto != NULL)
        *ponto = '\0';
    strncat(saida, ext, tamanho - strlen(saida) - 1);
}

static int grava_tokens(const char *fonte)
{
    FILE *entrada = fopen(fonte, "r");
    char log_nome[512];
    nome_log(fonte, ".tk", log_nome, sizeof(log_nome));

    if (entrada == NULL)
    {
        fprintf(stderr, "Erro ao abrir arquivo fonte para gerar tokens.\n");
        return 0;
    }

    FILE *saida = fopen(log_nome, "w");
    if (saida == NULL)
    {
        fclose(entrada);
        fprintf(stderr, "Erro ao criar log de tokens.\n");
        return 0;
    }

    inicializa_lex(entrada);

    TInfoAtomo atomo;
    do
    {
        atomo = proximo_atomo();
        fprintf(saida, "%d %s \"%s\"\n",
                atomo.linha, categoria_log(atomo.atomo), atomo.texto);
    } while (atomo.atomo != FIMARQUIVO);

    fclose(entrada);
    fclose(saida);

    printf("Log de tokens: %s\n", log_nome);
    return 1;
}

int main(int argc, char **argv)
{
    TOpcoes opcoes;

    if (!opts_parse(argc, argv, &opcoes))
        return 1;

    diag_init();

    FILE *trace = NULL;
    char log_nome[512];
    int sucesso_logs = 1;

    if (opcoes.opcao == OPCAO_TOKENS)
        sucesso_logs = grava_tokens(opcoes.arquivo);

    if (opcoes.opcao == OPCAO_TRACE)
    {
        nome_log(opcoes.arquivo, ".trc", log_nome, sizeof(log_nome));
        trace = fopen(log_nome, "w");

        if (trace == NULL)
        {
            fprintf(stderr, "Erro ao criar log de rastreamento.\n");
            return 1;
        }

        diag_set_trace(trace);
    }

    int sucesso = analisa_sintaxe(opcoes.arquivo);

    if (opcoes.opcao == OPCAO_SYMTAB)
    {
        nome_log(opcoes.arquivo, ".ts", log_nome, sizeof(log_nome));
        FILE *saida = fopen(log_nome, "w");

        if (saida == NULL)
        {
            fprintf(stderr, "Erro ao criar tabela de simbolos.\n");
            sucesso_logs = 0;
        }
        else
        {
            grava_tabela_log(obtem_tabela_parser(), saida);
            fclose(saida);
            printf("Log da tabela de simbolos: %s\n", log_nome);
        }
    }

    if (trace != NULL)
    {
        fclose(trace);
        printf("Log de rastreamento: %s\n", log_nome);
    }

    return (sucesso && sucesso_logs) ? 0 : 1;
}
