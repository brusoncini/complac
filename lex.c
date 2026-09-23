#include "lex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

FILE *arquivo;

int linhaAtual;

typedef struct
{
    char *palavra;
    TAtomo token;

} Palavra;

Palavra palavras[] =
    {
        {"proc", S_PROC},
        {"func", S_FUNC},
        {"main", S_MAIN},
        {"globvars", S_GLOBVARS},
        {"locvars", S_LOCVARS},
        {"start", S_START},
        {"end", S_END},
        {"echo", S_ECHO},
        {"get", S_GET},
        {"case", S_CASE},
        {"otherwise", S_OTHERWISE},
        {"choose", S_CHOOSE},
        {"match", S_MATCH},
        {"others", S_OTHERS},
        {"for", S_FOR},
        {"from", S_FROM},
        {"to", S_TO},
        {"by", S_BY},
        {"while", S_WHILE},
        {"repeat", S_REPEAT},
        {"until", S_UNTIL},
        {"return", S_RETURN},
        {"int", S_INT},
        {"logic", S_LOGIC},
        {"chr", S_CHR},
        {NULL, 0}

};

TAtomo reconhece_palavra(char texto[])
{

    int i = 0;

    while (palavras[i].palavra != NULL)
    {
        if (strcmp(texto, palavras[i].palavra) == 0)
            return palavras[i].token;
        i++;
    }

    return S_IDENTIF;
}

void lex_init(char *nomeArquivo)
{

    arquivo = fopen(nomeArquivo, "r");

    if (arquivo == NULL)
    {
        printf("Erro ao abrir arquivo\n");
        exit(1);
    }

    linhaAtual = 1;
}

TInfoAtomo lex_next()
{

    TInfoAtomo token;
    token.lexema[0] = '\0';
    token.linha = linhaAtual;

    int c;

    do
    {
        c = fgetc(arquivo);

        if (c == '\n')
            linhaAtual++;

    } while (isspace(c));

    if (c == EOF)
    {
        token.atomo = S_FIM;

        return token;
    }

    if (isalpha(c))
    {
        int i = 0;

        do
        {
            token.lexema[i++] = c;
            c = fgetc(arquivo);

        } while (isalnum(c) || c == '_');

        token.lexema[i] = '\0';
        ungetc(c, arquivo);
        token.atomo = reconhece_palavra(token.lexema);

        return token;
    }

    if (isdigit(c))
    {
        int i = 0;

        do
        {
            token.lexema[i++] = c;
            c = fgetc(arquivo);

        } while (isdigit(c));

        token.lexema[i] = '\0';
        ungetc(c, arquivo);
        token.atomo = S_CTEINT;

        return token;
    }

    if (c == '"')
    {
        int i = 0;
        c = fgetc(arquivo);

        while (c != '"' && c != EOF)
        {
            token.lexema[i++] = c;
            c = fgetc(arquivo);
        }

        token.lexema[i] = '\0';
        token.atomo = S_STRING;

        return token;
    }

    switch (c)
    {
    case '<':
        c = fgetc(arquivo);

        if (c == '<')
        {
            token.atomo = S_ATRIB;
            strcpy(token.lexema, "<<");
        }
        else
        {
            token.atomo = S_MENOR;
            strcpy(token.lexema, "<");

            ungetc(c, arquivo);
        }

        return token;
    case '+':
        token.atomo = S_SOMA;
        strcpy(token.lexema, "+");

        return token;
    case '-':
        token.atomo = S_SUBRAT;
        strcpy(token.lexema, "-");

        return token;
    case '*':
        token.atomo = S_MULT;
        strcpy(token.lexema, "*");

        return token;
    case '=':
        token.atomo = S_IGUAL;
        strcpy(token.lexema, "=");

        return token;
    case '(':
        token.atomo = S_ABREPAR;
        strcpy(token.lexema, "(");

        return token;
    case ')':
        token.atomo = S_FECHAPAR;
        strcpy(token.lexema, ")");

        return token;
    case ';':
        token.atomo = S_PONTOEVIRGULA;
        strcpy(token.lexema, ";");

        return token;
    case ',':
        token.atomo = S_VIRGULA;
        strcpy(token.lexema, ",");

        return token;
    }

    token.atomo = S_ERRO;
    return token;
}

char *lex_token_name(TAtomo token)
{
    switch (token)
    {
    case S_PROC:
        return "sPROC";
    case S_FUNC:
        return "sFUNC";
    case S_MAIN:
        return "sMAIN";
    case S_START:
        return "sSTART";
    case S_END:
        return "sEND";
    case S_ECHO:
        return "sECHO";
    case S_GET:
        return "sGET";
    case S_IDENTIF:
        return "sIDENTIF";
    case S_CTEINT:
        return "sCTEINT";
    case S_STRING:
        return "sSTRING";
    case S_ABREPAR:
        return "sABREPAR";
    case S_FECHAPAR:
        return "sFECHAPAR";
    case S_PONTOEVIRGULA:
        return "sPONTOEVIRGULA";
    case S_ATRIB:
        return "sATRIB";
    case S_FIM:
        return "EOF";
    default:
        return "TOKEN";
    }
}