#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "lex.h"

static FILE *arquivo_fonte;
static int linha_atual;

static TInfoAtomo cria_atomo(TAtomo atomo, char *texto)
{
    TInfoAtomo info;

    info.atomo = atomo;
    strcpy(info.texto, texto);
    info.linha = linha_atual;

    return info;
}

void inicializa_lex(FILE *arquivo)
{
    arquivo_fonte = arquivo;
    linha_atual = 1;
}

static void adiciona_caractere(char *texto, int *posicao, char caractere)
{
    texto[*posicao] = caractere;
    (*posicao)++;
    texto[*posicao] = '\0';
}

static TInfoAtomo verifica_reservada(char *texto)
{
    struct
    {
        char *lexema;
        TAtomo atomo;

    } tabela[] =
        {
            {"proc", PROC},
            {"func", FUNC},
            {"start", START},
            {"end", END},
            {"globvars", GLOBVARS},
            {"locvars", LOCVARS},
            {"is", IS},
            {"echo", ECHO},
            {"get", GET},
            {"case", CASE},
            {"otherwise", OTHERWISE},
            {"choose", CHOOSE},
            {"match", MATCH},
            {"others", OTHERS},
            {"for", FOR},
            {"from", FROM},
            {"to", TO},
            {"by", BY},
            {"do", DO},
            {"while", WHILE},
            {"repeat", REPEAT},
            {"until", UNTIL},
            {"return", RETURN},
            {"int", INT},
            {"logic", LOGIC},
            {"chr", CHR}};

    int tamanho = sizeof(tabela) / sizeof(tabela[0]);

    for (int i = 0; i < tamanho; i++)
    {
        if (strcmp(texto, tabela[i].lexema) == 0)
            return cria_atomo(tabela[i].atomo, texto);
    }

    return cria_atomo(IDENTIFICADOR, texto);
}

TInfoAtomo proximo_atomo()
{
    char texto[256];
    int posicao = 0;
    int caractere;

    while ((caractere = fgetc(arquivo_fonte)) != EOF)
    {

        if (caractere == ' ' || caractere == '\t' || caractere == '\r')
            continue;

        if (caractere == '\n')
        {
            linha_atual++;
            continue;
        }

        /* comentário de linha */
        if (caractere == '#')
        {
            while ((caractere = fgetc(arquivo_fonte)) != '\n' && caractere != EOF)
                ;

            if (caractere == '\n')
                linha_atual++;

            continue;
        }

        /* comentário de bloco */
        if (caractere == '/')
        {
            int proximo = fgetc(arquivo_fonte);

            if (proximo == '#')
            {
                while ((caractere = fgetc(arquivo_fonte)) != EOF)
                {
                    if (caractere == '\n')
                        linha_atual++;

                    if (caractere == '#')
                    {
                        if (fgetc(arquivo_fonte) == '/')
                            break;
                    }
                }

                continue;
            }

            ungetc(proximo, arquivo_fonte);
        }

        /* identificadores */
        if (isalpha(caractere) || caractere == '_')
        {
            adiciona_caractere(texto, &posicao, caractere);

            while ((caractere = fgetc(arquivo_fonte)) != EOF &&
                   (isalnum(caractere) || caractere == '_'))
            {
                adiciona_caractere(texto, &posicao, caractere);
            }

            ungetc(caractere, arquivo_fonte);

            return verifica_reservada(texto);
        }

        /* números */
        if (isdigit(caractere))
        {
            adiciona_caractere(texto, &posicao, caractere);

            while ((caractere = fgetc(arquivo_fonte)) != EOF &&
                   isdigit(caractere))
            {
                adiciona_caractere(texto, &posicao, caractere);
            }

            ungetc(caractere, arquivo_fonte);

            return cria_atomo(CONSTANTE_INTEIRA, texto);
        }

        /* strings */
        if (caractere == '"')
        {
            while ((caractere = fgetc(arquivo_fonte)) != '"' &&
                   caractere != EOF)
            {
                adiciona_caractere(texto, &posicao, caractere);
            }

            return cria_atomo(STRING, texto);
        }

        /* caracteres */
        if (caractere == '\'')
        {
            caractere = fgetc(arquivo_fonte);

            adiciona_caractere(texto, &posicao, caractere);

            fgetc(arquivo_fonte);

            return cria_atomo(CONSTANTE_CARACTERE, texto);
        }

        switch (caractere)
        {

        case '<':

            caractere = fgetc(arquivo_fonte);

            if (caractere == '<')
                return cria_atomo(ATRIBUICAO, "<<");

            if (caractere == '=')
                return cria_atomo(MENORIGUAL, "<=");

            ungetc(caractere, arquivo_fonte);

            return cria_atomo(MENOR, "<");

        case '>':

            caractere = fgetc(arquivo_fonte);

            if (caractere == '=')
                return cria_atomo(MAIORIGUAL, ">=");

            ungetc(caractere, arquivo_fonte);

            return cria_atomo(MAIOR, ">");

        case '~':

            caractere = fgetc(arquivo_fonte);

            if (caractere == '=')
                return cria_atomo(DIFERENTE, "~=");

            ungetc(caractere, arquivo_fonte);

            return cria_atomo(NEGACAO, "~");

        case '+':
            return cria_atomo(SOMA, "+");

        case '-':
        {
            int proximo = fgetc(arquivo_fonte);

            if (proximo == '>')
                return cria_atomo(SETA, "->");

            ungetc(proximo, arquivo_fonte);

            return cria_atomo(SUBTRACAO, "-");
        }

        case '*':
            return cria_atomo(MULTIPLICACAO, "*");

        case '/':
        {
            int proximo = fgetc(arquivo_fonte);

            if (proximo == '/')
                return cria_atomo(DIVISAO, "//");

            ungetc(proximo, arquivo_fonte);

            return cria_atomo(ERRO, "/");
        }

        case '=':
            return cria_atomo(IGUAL, "=");

        case '&':
            return cria_atomo(E_LOGICO, "&");

        case '|':
            return cria_atomo(OU_LOGICO, "|");

        case '(':
            return cria_atomo(ABREPARENTESES, "(");

        case ')':
            return cria_atomo(FECHAPARENTESES, ")");

        case '[':
            return cria_atomo(ABRECOLCHETE, "[");

        case ']':
            return cria_atomo(FECHACOLCHETE, "]");

        case ',':
            return cria_atomo(VIRGULA, ",");

        case ';':
            return cria_atomo(PONTOEVIRGULA, ";");

        case ':':
            return cria_atomo(DOISPONTOS, ":");
        }

        texto[0] = caractere;
        texto[1] = '\0';

        return cria_atomo(ERRO, texto);
    }

    return cria_atomo(FIMARQUIVO, "EOF");
}

char *nome_atomo(TAtomo atomo)
{
    switch (atomo)
    {
    case IDENTIFICADOR:
        return "IDENTIFICADOR";

    case CONSTANTE_INTEIRA:
        return "CONSTANTE_INTEIRA";

    case CONSTANTE_CARACTERE:
        return "CONSTANTE_CARACTERE";

    case STRING:
        return "STRING";

    case PROC:
        return "PROC";

    case FUNC:
        return "FUNC";

    case START:
        return "START";

    case END:
        return "END";

    case GLOBVARS:
        return "GLOBVARS";

    case LOCVARS:
        return "LOCVARS";

    case ECHO:
        return "ECHO";

    case GET:
        return "GET";

    case CASE:
        return "CASE";

    case OTHERWISE:
        return "OTHERWISE";

    case CHOOSE:
        return "CHOOSE";

    case MATCH:
        return "MATCH";

    case OTHERS:
        return "OTHERS";

    case ATRIBUICAO:
        return "ATRIBUICAO";

    case SOMA:
        return "SOMA";

    case SUBTRACAO:
        return "SUBTRACAO";

    case MULTIPLICACAO:
        return "MULTIPLICACAO";

    case DIVISAO:
        return "DIVISAO";

    case IGUAL:
        return "IGUAL";

    case DIFERENTE:
        return "DIFERENTE";

    case MAIOR:
        return "MAIOR";

    case MENOR:
        return "MENOR";

    case MAIORIGUAL:
        return "MAIORIGUAL";

    case MENORIGUAL:
        return "MENORIGUAL";

    case E_LOGICO:
        return "E_LOGICO";

    case OU_LOGICO:
        return "OU_LOGICO";

    case NEGACAO:
        return "NEGACAO";

    case ABREPARENTESES:
        return "ABREPARENTESES";

    case FECHAPARENTESES:
        return "FECHAPARENTESES";

    case ABRECOLCHETE:
        return "ABRECOLCHETE";

    case FECHACOLCHETE:
        return "FECHACOLCHETE";

    case VIRGULA:
        return "VIRGULA";

    case PONTOEVIRGULA:
        return "PONTOEVIRGULA";

    case DOISPONTOS:
        return "DOISPONTOS";

    case IS:
        return "IS";

    case INT:
        return "INT";

    case LOGIC:
        return "LOGIC";

    case CHR:
        return "CHR";

    case SETA:
        return "SETA";

    case ERRO:
        return "ERRO";

    case FIMARQUIVO:
        return "FIMARQUIVO";

    default:
        return "DESCONHECIDO";
    }
}