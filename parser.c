#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"
#include "parser.h"

static FILE *arquivo_parser;
static TInfoAtomo atual;
static TInfoAtomo proximo;
static int erros;

static void avanca(void)
{
    atual = proximo;
    proximo = proximo_atomo();
}

static void erro(const char *esperado)
{
    fprintf(stderr, "Erro sintatico na linha %d: esperado %s, encontrado %s (%s)\n",
            atual.linha, esperado, nome_atomo(atual.atomo), atual.texto);
    erros++;
}

static int aceita(TAtomo atomo)
{
    if (atual.atomo == atomo)
    {
        avanca();
        return 1;
    }
    return 0;
}

static int espera(TAtomo atomo, const char *nome)
{
    if (aceita(atomo))
        return 1;
    erro(nome);
    return 0;
}

static int eh(TAtomo atomo)
{
    return atual.atomo == atomo;
}

static int eh_inicio_cmd(void)
{
    return eh(ECHO) || eh(GET) || eh(CASE) || eh(CHOOSE) || eh(FOR) ||
           eh(WHILE) || eh(REPEAT) || eh(RETURN) || eh(IDENTIFICADOR);
}

static int parse_expr(void);
static int parse_cmd(void);
static int parse_bco(void);
static int parse_elem(void);

static int parse_tipo(void)
{
    if (aceita(INT) || aceita(LOGIC) || aceita(CHR))
    {
        if (aceita(ABRECOLCHETE))
        {
            if (!espera(CONSTANTE_INTEIRA, "constante inteira"))
                return 0;
            if (!espera(FECHACOLCHETE, "]"))
                return 0;
        }
        return 1;
    }
    erro("tipo");
    return 0;
}

static int parse_decls(void)
{
    if (!espera(IDENTIFICADOR, "identificador"))
        return 0;

    while (aceita(VIRGULA))
    {
        if (!espera(IDENTIFICADOR, "identificador"))
            return 0;
    }

    if (!espera(KIND, "is"))
        return 0;

    if (!parse_tipo())
        return 0;

    return espera(PONTOEVIRGULA, ";");
}

static int parse_gvars(void)
{
    if (!aceita(GLOBVARS))
        return 1;

    if (!eh(IDENTIFICADOR))
    {
        erro("declaracao de variavel");
        return 0;
    }

    do
    {
        if (!parse_decls())
            return 0;
    } while (eh(IDENTIFICADOR));

    return 1;
}

static int parse_lvars(void)
{
    if (!aceita(LOCVARS))
        return 1;

    if (!eh(IDENTIFICADOR))
    {
        erro("declaracao de variavel");
        return 0;
    }

    do
    {
        if (!parse_decls())
            return 0;
    } while (eh(IDENTIFICADOR));

    return 1;
}

static int parse_param(void)
{
    aceita(REF);
    if (!espera(IDENTIFICADOR, "identificador"))
        return 0;
    if (!espera(DOISPONTOS, ":"))
        return 0;
    return parse_tipo();
}

static int parse_params(void)
{
    if (!eh(IDENTIFICADOR) && !eh(REF))
        return 1;

    if (!parse_param())
        return 0;

    while (aceita(VIRGULA))
    {
        if (!parse_param())
            return 0;
    }
    return 1;
}

static int parse_vetor(void)
{
    if (!espera(IDENTIFICADOR, "identificador"))
        return 0;
    if (!espera(ABRECOLCHETE, "["))
        return 0;

    if (!aceita(CONSTANTE_INTEIRA) && !aceita(IDENTIFICADOR))
    {
        erro("constante inteira ou identificador");
        return 0;
    }

    return espera(FECHACOLCHETE, "]");
}

static int parse_call(void)
{
    if (!espera(IDENTIFICADOR, "identificador"))
        return 0;
    if (!espera(ABREPARENTESES, "("))
        return 0;

    if (!eh(FECHAPARENTESES))
    {
        if (!parse_expr())
            return 0;
        while (aceita(VIRGULA))
        {
            if (!parse_expr())
                return 0;
        }
    }

    return espera(FECHAPARENTESES, ")");
}

static int parse_litl(void)
{
    if (aceita(STRING) || aceita(CONSTANTE_INTEIRA) || aceita(CONSTANTE_CARACTERE))
        return 1;
    erro("literal");
    return 0;
}

static int parse_fact(void)
{
    if (aceita(NEGACAO) || aceita(SUBTRACAO))
        return parse_fact();

    if (aceita(ABREPARENTESES))
    {
        if (!parse_expr())
            return 0;
        return espera(FECHAPARENTESES, ")");
    }

    if (eh(STRING) || eh(CONSTANTE_INTEIRA) || eh(CONSTANTE_CARACTERE))
        return parse_litl();

    if (eh(IDENTIFICADOR))
    {
        if (proximo.atomo == ABREPARENTESES)
            return parse_call();
        if (proximo.atomo == ABRECOLCHETE)
            return parse_vetor();
        return espera(IDENTIFICADOR, "identificador");
    }

    erro("elemento da expressao");
    return 0;
}

static int parse_earip(void)
{
    if (!parse_fact())
        return 0;
    while (eh(MULTIPLICACAO) || eh(DIVISAO))
    {
        avanca();
        if (!parse_fact())
            return 0;
    }
    return 1;
}

static int parse_earit(void)
{
    if (!parse_earip())
        return 0;
    while (eh(SOMA) || eh(SUBTRACAO))
    {
        avanca();
        if (!parse_earip())
            return 0;
    }
    return 1;
}

static int parse_erlac(void)
{
    if (!parse_earit())
        return 0;
    while (eh(MAIOR) || eh(MAIORIGUAL) || eh(IGUAL) || eh(MENOR) ||
           eh(MENORIGUAL) || eh(DIFERENTE))
    {
        avanca();
        if (!parse_earit())
            return 0;
    }
    return 1;
}

static int parse_elogc(void)
{
    if (!parse_erlac())
        return 0;
    while (aceita(E_LOGICO))
    {
        if (!parse_erlac())
            return 0;
    }
    return 1;
}

static int parse_expr(void)
{
    if (!parse_elogc())
        return 0;
    while (aceita(OU_LOGICO))
    {
        if (!parse_elogc())
            return 0;
    }
    return 1;
}

static int parse_elem(void)
{
    return parse_expr();
}

static int parse_echo(void)
{
    if (!espera(ECHO, "echo"))
        return 0;
    if (!espera(ABREPARENTESES, "("))
        return 0;
    if (!parse_elem())
        return 0;
    while (aceita(VIRGULA))
    {
        if (!parse_elem())
            return 0;
    }
    return espera(FECHAPARENTESES, ")");
}

static int parse_get(void)
{
    if (!espera(GET, "get"))
        return 0;
    if (!espera(ABREPARENTESES, "("))
        return 0;
    if (eh(IDENTIFICADOR) && proximo.atomo == ABRECOLCHETE)
    {
        if (!parse_vetor())
            return 0;
    }
    else if (!espera(IDENTIFICADOR, "identificador"))
        return 0;
    return espera(FECHAPARENTESES, ")");
}

static int parse_atr(void)
{
    if (eh(IDENTIFICADOR) && proximo.atomo == ABRECOLCHETE)
    {
        if (!parse_vetor())
            return 0;
    }
    else if (!espera(IDENTIFICADOR, "identificador"))
        return 0;

    if (!espera(ATRIBUICAO, "<<"))
        return 0;
    return parse_elem();
}

static int parse_ret(void)
{
    if (!espera(RETURN, "return"))
        return 0;
    return parse_elem();
}

static int parse_case(void)
{
    if (!espera(CASE, "case"))
        return 0;
    if (!espera(ABREPARENTESES, "("))
        return 0;
    if (!parse_expr())
        return 0;
    if (!espera(FECHAPARENTESES, ")"))
        return 0;
    if (!parse_cmd())
        return 0;
    if (!espera(PONTOEVIRGULA, ";"))
        return 0;
    if (aceita(OTHERWISE))
    {
        if (!parse_cmd())
            return 0;
        if (!espera(PONTOEVIRGULA, ";"))
            return 0;
    }
    return espera(END, "end");
}

static int parse_match(void)
{
    if (!espera(MATCH, "match"))
        return 0;
    if (!espera(CONSTANTE_INTEIRA, "constante inteira"))
        return 0;
    while (aceita(VIRGULA))
    {
        if (!espera(CONSTANTE_INTEIRA, "constante inteira"))
            return 0;
    }
    if (!espera(IMPLIC, "->"))
        return 0;
    if (!parse_cmd())
        return 0;
    return espera(PONTOEVIRGULA, ";");
}

static int parse_choose(void)
{
    if (!espera(CHOOSE, "choose"))
        return 0;
    if (!espera(ABREPARENTESES, "("))
        return 0;
    if (!parse_expr())
        return 0;
    if (!espera(FECHAPARENTESES, ")"))
        return 0;

    if (!eh(MATCH))
    {
        erro("match");
        return 0;
    }
    while (eh(MATCH))
    {
        if (!parse_match())
            return 0;
    }

    if (aceita(OTHERS))
    {
        if (!espera(IMPLIC, "->"))
            return 0;
        if (!parse_cmd())
            return 0;
        if (!espera(PONTOEVIRGULA, ";"))
            return 0;
    }
    return espera(END, "end");
}

static int parse_for(void)
{
    if (!espera(FOR, "for")) return 0;
    if (!espera(IDENTIFICADOR, "identificador")) return 0;
    if (!espera(FROM, "from")) return 0;
    if (!aceita(IDENTIFICADOR) && !aceita(CONSTANTE_INTEIRA))
    { erro("identificador ou constante inteira"); return 0; }
    if (!espera(TO, "to")) return 0;
    if (!aceita(IDENTIFICADOR) && !aceita(CONSTANTE_INTEIRA))
    { erro("identificador ou constante inteira"); return 0; }
    if (aceita(BY) && !espera(CONSTANTE_INTEIRA, "constante inteira")) return 0;
    if (!espera(DO, "do")) return 0;
    while (!eh(END) && !eh(FIMARQUIVO))
    {
        if (!parse_cmd()) return 0;
        if (!espera(PONTOEVIRGULA, ";")) return 0;
    }
    return espera(END, "end");
}

static int parse_while(void)
{
    if (!espera(WHILE, "while")) return 0;
    if (!espera(ABREPARENTESES, "(")) return 0;
    if (!parse_expr()) return 0;
    if (!espera(FECHAPARENTESES, ")")) return 0;
    if (!espera(DO, "do")) return 0;
    while (!eh(END) && !eh(FIMARQUIVO))
    {
        if (!parse_cmd()) return 0;
        if (!espera(PONTOEVIRGULA, ";")) return 0;
    }
    return espera(END, "end");
}

static int parse_repeat(void)
{
    if (!espera(REPEAT, "repeat"))
        return 0;
    while (eh_inicio_cmd())
    {
        if (!parse_cmd())
            return 0;
        if (!espera(PONTOEVIRGULA, ";"))
            return 0;
    }
    if (!espera(UNTIL, "until"))
        return 0;
    if (!espera(ABREPARENTESES, "("))
        return 0;
    if (!parse_expr())
        return 0;
    return espera(FECHAPARENTESES, ")");
}

static int parse_cmd(void)
{
    switch (atual.atomo)
    {
    case ECHO: return parse_echo();
    case GET: return parse_get();
    case CASE: return parse_case();
    case CHOOSE: return parse_choose();
    case FOR: return parse_for();
    case WHILE: return parse_while();
    case REPEAT: return parse_repeat();
    case RETURN: return parse_ret();
    case IDENTIFICADOR:
        if (proximo.atomo == ABREPARENTESES)
            return parse_call();
        return parse_atr();
    default:
        erro("comando");
        return 0;
    }
}

static int parse_bco(void)
{
    if (!espera(START, "start"))
        return 0;

    while (!eh(END) && !eh(FIMARQUIVO))
    {
        if (!parse_cmd())
            return 0;
        if (!espera(PONTOEVIRGULA, ";"))
            return 0;
    }

    return espera(END, "end");
}

static int parse_proc(void)
{
    if (!espera(PROC, "proc"))
        return 0;
    if (!espera(IDENTIFICADOR, "identificador"))
        return 0;
    if (!espera(ABREPARENTESES, "("))
        return 0;
    if (!parse_params())
        return 0;
    if (!espera(FECHAPARENTESES, ")"))
        return 0;
    if (!parse_lvars())
        return 0;
    return parse_bco();
}

static int parse_func(void)
{
    if (!espera(FUNC, "func"))
        return 0;
    if (!espera(IDENTIFICADOR, "identificador"))
        return 0;
    if (!espera(ABREPARENTESES, "("))
        return 0;
    if (!parse_params())
        return 0;
    if (!espera(FECHAPARENTESES, ")"))
        return 0;
    if (!espera(DOISPONTOS, ":"))
        return 0;
    if (!parse_tipo())
        return 0;
    if (!parse_lvars())
        return 0;
    return parse_bco();
}

static int parse_subs(void)
{
    while (eh(FUNC) || (eh(PROC) && !(proximo.atomo == IDENTIFICADOR && strcmp(proximo.texto, "main") == 0)))
    {
        if (eh(FUNC))
        {
            if (!parse_func()) return 0;
        }
        else
        {
            if (!parse_proc()) return 0;
        }
    }
    return 1;
}

static int parse_princ(void)
{
    if (!eh(PROC) || proximo.atomo != IDENTIFICADOR || strcmp(proximo.texto, "main") != 0)
    {
        erro("proc main");
        return 0;
    }
    return parse_proc();
}

int analisa_sintaxe(const char *nome_arquivo)
{
    arquivo_parser = fopen(nome_arquivo, "r");
    if (arquivo_parser == NULL)
    {
        fprintf(stderr, "Erro ao abrir arquivo: %s\n", nome_arquivo);
        return 0;
    }

    inicializa_lex(arquivo_parser);
    atual = proximo_atomo();
    proximo = proximo_atomo();
    erros = 0;

    if (!parse_gvars() || !parse_subs() || !parse_princ())
    {
        fclose(arquivo_parser);
        return 0;
    }

    if (!eh(FIMARQUIVO))
    {
        erro("fim do arquivo");
        fclose(arquivo_parser);
        return 0;
    }

    fclose(arquivo_parser);

    if (erros == 0)
    {
        printf("Analise sintatica concluida com sucesso.\n");
        return 1;
    }

    return 0;
}
