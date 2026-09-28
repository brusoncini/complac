#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"
#include "parser.h"
#include "symtab.h"
#include "diag.h"

static FILE *arquivo_parser;
static TInfoAtomo atual, proximo;
static int erros;
static TabelaSimbolos tabela_parser;
static TEscopo escopo_atual = GLOBAL;
static char contexto_atual[TAM_NOME] = "global";

static void registra_erro_lexico(TInfoAtomo token)
{
    diag_error(token.linha, "lexico", "token invalido", token.texto);
    erros++;
}

static void avanca(void)
{
    atual = proximo;
    proximo = proximo_atomo();

    if (atual.atomo == ERRO)
        registra_erro_lexico(atual);
}

static void erro(const char *esperado)
{
    if (atual.atomo == ERRO)
        return;

    char mensagem[512];
    snprintf(mensagem, sizeof(mensagem), "esperado %s, encontrado %s (%s)",
             esperado, nome_atomo(atual.atomo), atual.texto);
    diag_error(atual.linha, "sintatico", mensagem, NULL);
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
    if (aceita(atomo)) return 1;
    erro(nome);
    return 0;
}

static int eh(TAtomo atomo) { return atual.atomo == atomo; }

static int eh_inicio_cmd(void)
{
    return eh(ECHO) || eh(GET) || eh(CASE) || eh(CHOOSE) || eh(FOR) ||
           eh(WHILE) || eh(REPEAT) || eh(RETURN) || eh(IDENTIFICADOR);
}

static TTipo tipo_expr(void);
static TTipo tipo_elem(void);
static int parse_cmd(void);
static int parse_bco(void);

static TTipo busca_tipo_identificador(const char *nome)
{
    TSimbolo *simbolo = busca_no_escopo(&tabela_parser, nome, escopo_atual, contexto_atual);
    return simbolo != NULL ? simbolo->tipo : TIPO_NAO_DEFINIDO;
}

static TSimbolo *busca_simbolo_atual(const char *nome)
{
    return busca_no_escopo(&tabela_parser, nome, escopo_atual, contexto_atual);
}

static int insere_declaracao(char *nome, TCategoriaSimbolo categoria,
                             TTipo tipo, TEscopo escopo, int tamanho)
{
    if (busca_simbolo_contexto(&tabela_parser, nome, escopo,
                               escopo == GLOBAL ? "global" : contexto_atual) != NULL)
    {
        diag_error(atual.linha, "tabela",
                   "identificador ja declarado no escopo", nome);
        erros++;
        return 0;
    }

    if (!insere_simbolo_contexto(&tabela_parser, nome, categoria, tipo, escopo, escopo == GLOBAL ? "global" : contexto_atual))
        return 0;

    if (tamanho > 0)
    {
        TSimbolo *simbolo = busca_simbolo_contexto(&tabela_parser, nome, escopo, escopo == GLOBAL ? "global" : contexto_atual);
        if (simbolo != NULL) simbolo->tamanho_vetor = tamanho;
    }
    return 1;
}

static int parse_tipo(TTipo *tipo, int *tamanho_vetor)
{
    *tamanho_vetor = 0;
    if (aceita(INT)) *tipo = TIPO_INT;
    else if (aceita(LOGIC)) *tipo = TIPO_LOGIC;
    else if (aceita(CHR)) *tipo = TIPO_CHR;
    else { erro("tipo"); return 0; }

    if (aceita(ABRECOLCHETE))
    {
        if (!eh(CONSTANTE_INTEIRA)) { erro("constante inteira"); return 0; }
        *tamanho_vetor = atoi(atual.texto);
        avanca();
        if (!espera(FECHACOLCHETE, "]")) return 0;
    }
    return 1;
}

static int parse_decls(TEscopo escopo)
{
    char nomes[32][TAM_NOME];
    int quantidade = 0;

    if (!eh(IDENTIFICADOR)) { erro("identificador"); return 0; }
    strcpy(nomes[quantidade++], atual.texto);
    avanca();

    while (aceita(VIRGULA))
    {
        if (!eh(IDENTIFICADOR)) { erro("identificador"); return 0; }
        if (quantidade < 32) strcpy(nomes[quantidade++], atual.texto);
        avanca();
    }

    if (!espera(KIND, "is")) return 0;

    TTipo tipo;
    int tamanho_vetor;
    if (!parse_tipo(&tipo, &tamanho_vetor)) return 0;
    if (!espera(PONTOEVIRGULA, ";")) return 0;

    for (int i = 0; i < quantidade; i++)
    {
        if (!insere_declaracao(nomes[i], VARIAVEL, tipo, escopo, tamanho_vetor))
            return 0;
    }
    return 1;
}

static int parse_gvars(void)
{
    if (!aceita(GLOBVARS)) return 1;
    if (!eh(IDENTIFICADOR)) { erro("declaracao de variavel"); return 0; }
    do { if (!parse_decls(GLOBAL)) return 0; } while (eh(IDENTIFICADOR));
    return 1;
}

static int parse_lvars(void)
{
    if (!aceita(LOCVARS)) return 1;
    if (!eh(IDENTIFICADOR)) { erro("declaracao de variavel"); return 0; }
    do { if (!parse_decls(LOCAL)) return 0; } while (eh(IDENTIFICADOR));
    return 1;
}

static int parse_param(void)
{
    aceita(REF);
    if (!eh(IDENTIFICADOR)) { erro("identificador"); return 0; }
    char nome[TAM_NOME];
    strcpy(nome, atual.texto);
    avanca();
    if (!espera(DOISPONTOS, ":")) return 0;
    TTipo tipo;
    int tamanho_vetor;
    if (!parse_tipo(&tipo, &tamanho_vetor)) return 0;
    insere_declaracao(nome, VARIAVEL, tipo, PARAMETRO, tamanho_vetor);
    return 1;
}

static int parse_params(int *quantidade)
{
    *quantidade = 0;
    if (!eh(IDENTIFICADOR) && !eh(REF)) return 1;
    if (!parse_param()) return 0;
    (*quantidade)++;
    while (aceita(VIRGULA))
    {
        if (!parse_param()) return 0;
        (*quantidade)++;
    }
    return 1;
}

static TTipo parse_vetor(void)
{
    if (!eh(IDENTIFICADOR)) { erro("identificador"); return TIPO_NAO_DEFINIDO; }
    char nome[TAM_NOME];
    strcpy(nome, atual.texto);
    TSimbolo *simbolo = busca_simbolo_atual(nome);
    avanca();
    if (!espera(ABRECOLCHETE, "[")) return TIPO_NAO_DEFINIDO;
    tipo_expr();
    if (!espera(FECHACOLCHETE, "]")) return TIPO_NAO_DEFINIDO;
    return simbolo ? simbolo->tipo : TIPO_NAO_DEFINIDO;
}

static TTipo parse_call(void)
{
    char nome[TAM_NOME];
    strcpy(nome, atual.texto);
    TSimbolo *simbolo = busca_simbolo_contexto(&tabela_parser, nome, GLOBAL, "global");

    espera(IDENTIFICADOR, "identificador");
    if (!espera(ABREPARENTESES, "(")) return TIPO_NAO_DEFINIDO;

    if (!eh(FECHAPARENTESES))
    {
        tipo_expr();
        while (aceita(VIRGULA))
            tipo_expr();
    }

    if (!espera(FECHAPARENTESES, ")")) return TIPO_NAO_DEFINIDO;
    return simbolo ? simbolo->tipo : TIPO_NAO_DEFINIDO;
}

static TTipo parse_litl(void)
{
    if (aceita(CONSTANTE_INTEIRA)) return TIPO_INT;
    if (aceita(CONSTANTE_CARACTERE)) return TIPO_CHR;
    if (aceita(STRING)) return TIPO_STRING;
    erro("literal");
    return TIPO_NAO_DEFINIDO;
}

static TTipo parse_fact(void)
{
    if (aceita(NEGACAO))
    {
        parse_fact();
        return TIPO_LOGIC;
    }

    if (aceita(SUBTRACAO))
    {
        parse_fact();
        return TIPO_INT;
    }

    if (aceita(ABREPARENTESES))
    {
        TTipo tipo = tipo_expr();
        espera(FECHAPARENTESES, ")");
        return tipo;
    }

    if (eh(STRING) || eh(CONSTANTE_INTEIRA) || eh(CONSTANTE_CARACTERE))
        return parse_litl();

    if (eh(IDENTIFICADOR))
    {
        if (proximo.atomo == ABREPARENTESES)
            return parse_call();

        if (proximo.atomo == ABRECOLCHETE)
            return parse_vetor();

        char nome[TAM_NOME];
        strcpy(nome, atual.texto);
        TTipo tipo = busca_tipo_identificador(nome);
        avanca();
        return tipo;
    }

    erro("elemento da expressao");
    return TIPO_NAO_DEFINIDO;
}

static TTipo parse_earip(void)
{
    TTipo esquerda = parse_fact();

    while (eh(MULTIPLICACAO) || eh(DIVISAO))
    {
        avanca();
        parse_fact();
        esquerda = TIPO_INT;
    }

    return esquerda;
}

static TTipo parse_earit(void)
{
    TTipo esquerda = parse_earip();

    while (eh(SOMA) || eh(SUBTRACAO))
    {
        avanca();
        parse_earip();
        esquerda = TIPO_INT;
    }

    return esquerda;
}

static TTipo parse_erlac(void)
{
    TTipo esquerda = parse_earit();

    while (eh(MAIOR) || eh(MAIORIGUAL) || eh(IGUAL) || eh(MENOR) ||
           eh(MENORIGUAL) || eh(DIFERENTE))
    {
        avanca();
        parse_earit();
        esquerda = TIPO_LOGIC;
    }

    return esquerda;
}

static TTipo parse_elogc(void)
{
    TTipo esquerda = parse_erlac();

    while (aceita(E_LOGICO))
    {
        parse_erlac();
        esquerda = TIPO_LOGIC;
    }

    return esquerda;
}

static TTipo tipo_expr(void)
{
    TTipo esquerda = parse_elogc();

    while (aceita(OU_LOGICO))
    {
        parse_elogc();
        esquerda = TIPO_LOGIC;
    }

    return esquerda;
}

static TTipo tipo_elem(void)
{
    return tipo_expr();
}

static int parse_echo(void)
{
    if (!espera(ECHO, "echo")) return 0;
    if (!espera(ABREPARENTESES, "(")) return 0;
    if (tipo_elem() == TIPO_NAO_DEFINIDO) return 0;
    while (aceita(VIRGULA))
    {
        if (tipo_elem() == TIPO_NAO_DEFINIDO) return 0;
    }
    return espera(FECHAPARENTESES, ")");
}

static int parse_get(void)
{
    if (!espera(GET, "get")) return 0;
    if (!espera(ABREPARENTESES, "(")) return 0;
    if (eh(IDENTIFICADOR))
    {
        if (proximo.atomo == ABRECOLCHETE) parse_vetor();
        else avanca();
    }
    else erro("identificador");
    return espera(FECHAPARENTESES, ")");
}

static int parse_atr(void)
{
    if (proximo.atomo == ABRECOLCHETE)
        parse_vetor();
    else
        if (!espera(IDENTIFICADOR, "identificador")) return 0;

    if (!espera(ATRIBUICAO, "<<")) return 0;
    tipo_elem();
    return 1;
}

static int parse_ret(void)
{
    if (!espera(RETURN, "return")) return 0;
    tipo_elem();
    return 1;
}

static int parse_case(void)
{
    if (!espera(CASE, "case")) return 0;
    if (!espera(ABREPARENTESES, "(")) return 0;
    tipo_expr();
    if (!espera(FECHAPARENTESES, ")")) return 0;
    if (!parse_cmd()) return 0;
    if (!espera(PONTOEVIRGULA, ";")) return 0;
    if (aceita(OTHERWISE))
    {
        if (!parse_cmd()) return 0;
        if (!espera(PONTOEVIRGULA, ";")) return 0;
    }
    return espera(END, "end");
}

static int parse_match(void)
{
    if (!espera(MATCH, "match")) return 0;
    if (!espera(CONSTANTE_INTEIRA, "constante inteira")) return 0;
    while (aceita(VIRGULA)) if (!espera(CONSTANTE_INTEIRA, "constante inteira")) return 0;
    if (!espera(IMPLIC, "->")) return 0;
    if (!parse_cmd()) return 0;
    return espera(PONTOEVIRGULA, ";");
}

static int parse_choose(void)
{
    if (!espera(CHOOSE, "choose")) return 0;
    if (!espera(ABREPARENTESES, "(")) return 0;
    tipo_expr();
    if (!espera(FECHAPARENTESES, ")")) return 0;
    if (!eh(MATCH)) { erro("match"); return 0; }
    while (eh(MATCH)) if (!parse_match()) return 0;
    if (aceita(OTHERS))
    {
        if (!espera(IMPLIC, "->")) return 0;
        if (!parse_cmd()) return 0;
        if (!espera(PONTOEVIRGULA, ";")) return 0;
    }
    return espera(END, "end");
}

static int parse_for(void)
{
    if (!espera(FOR, "for")) return 0;
    if (!espera(IDENTIFICADOR, "identificador")) return 0;
    if (!espera(FROM, "from")) return 0;
    if (!aceita(IDENTIFICADOR) && !aceita(CONSTANTE_INTEIRA)) { erro("identificador ou constante inteira"); return 0; }
    if (!espera(TO, "to")) return 0;
    if (!aceita(IDENTIFICADOR) && !aceita(CONSTANTE_INTEIRA)) { erro("identificador ou constante inteira"); return 0; }
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
    tipo_expr();
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
    if (!espera(REPEAT, "repeat")) return 0;
    while (eh_inicio_cmd())
    {
        if (!parse_cmd()) return 0;
        if (!espera(PONTOEVIRGULA, ";")) return 0;
    }
    if (!espera(UNTIL, "until")) return 0;
    if (!espera(ABREPARENTESES, "(")) return 0;
    tipo_expr();
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
        if (proximo.atomo == ABREPARENTESES) { parse_call(); return 1; }
        return parse_atr();
    default: erro("comando"); return 0;
    }
}

static int parse_bco(void)
{
    if (!espera(START, "start")) return 0;
    while (!eh(END) && !eh(FIMARQUIVO))
    {
        if (!parse_cmd()) return 0;
        if (!espera(PONTOEVIRGULA, ";")) return 0;
    }
    return espera(END, "end");
}

static int parse_proc(void)
{

    if (!espera(PROC, "proc")) return 0;
    if (!eh(IDENTIFICADOR)) { erro("identificador"); return 0; }
    char nome[TAM_NOME]; strcpy(nome, atual.texto); avanca();
    if (!espera(ABREPARENTESES, "(")) return 0;

    TEscopo anterior = escopo_atual;
    char contexto_anterior[TAM_NOME];
    strcpy(contexto_anterior, contexto_atual);
    strcpy(contexto_atual, nome);
    escopo_atual = PARAMETRO;
    int quantidade_parametros;
    if (!parse_params(&quantidade_parametros)) return 0;
    if (!espera(FECHAPARENTESES, ")")) return 0;

    if (!insere_declaracao(nome, SUBROTINA, TIPO_VOID, GLOBAL, 0))
        return 0;
    TSimbolo *simbolo = busca_simbolo_contexto(&tabela_parser, nome, GLOBAL, "global");
    if (simbolo != NULL) simbolo->quantidade_parametros = quantidade_parametros;

    escopo_atual = LOCAL;
    if (!parse_lvars()) return 0;
    int resultado = parse_bco();
    escopo_atual = anterior;
    strcpy(contexto_atual, contexto_anterior);
    return resultado;
}

static int parse_func(void)
{
    if (!espera(FUNC, "func")) return 0;
    if (!eh(IDENTIFICADOR)) { erro("identificador"); return 0; }
    char nome[TAM_NOME]; strcpy(nome, atual.texto); avanca();
    if (!espera(ABREPARENTESES, "(")) return 0;

    TEscopo anterior = escopo_atual;
    char contexto_anterior[TAM_NOME];
    strcpy(contexto_anterior, contexto_atual);
    strcpy(contexto_atual, nome);
    escopo_atual = PARAMETRO;
    int quantidade_parametros;
    if (!parse_params(&quantidade_parametros)) return 0;
    if (!espera(FECHAPARENTESES, ")")) return 0;
    if (!espera(DOISPONTOS, ":")) return 0;

    TTipo tipo;
    int tamanho_vetor;
    if (!parse_tipo(&tipo, &tamanho_vetor)) return 0;
    (void)tamanho_vetor;
    if (!insere_declaracao(nome, SUBROTINA, tipo, GLOBAL, 0))
        return 0;
    TSimbolo *simbolo = busca_simbolo_contexto(&tabela_parser, nome, GLOBAL, "global");
    if (simbolo != NULL) simbolo->quantidade_parametros = quantidade_parametros;

    escopo_atual = LOCAL;
    if (!parse_lvars()) return 0;
    int resultado = parse_bco();
    escopo_atual = anterior;
    strcpy(contexto_atual, contexto_anterior);
    return resultado;
}

static int parse_subs(void)
{
    while (eh(FUNC) || (eh(PROC) && !(proximo.atomo == IDENTIFICADOR && strcmp(proximo.texto, "main") == 0)))
    {
        if (eh(FUNC)) { if (!parse_func()) return 0; }
        else { if (!parse_proc()) return 0; }
    }
    return 1;
}

static int parse_princ(void)
{
    if (!eh(PROC) || proximo.atomo != IDENTIFICADOR || strcmp(proximo.texto, "main") != 0)
    { erro("proc main"); return 0; }
    return parse_proc();
}

int analisa_sintaxe(const char *nome_arquivo)
{
    diag_trace("ENTRA", "prg", 1);
    arquivo_parser = fopen(nome_arquivo, "r");
    if (arquivo_parser == NULL)
    {
        diag_error(0, "arquivo", "nao foi possivel abrir o arquivo", nome_arquivo);
        return 0;
    }

    inicializa_tabela(&tabela_parser);
    inicializa_lex(arquivo_parser);
    erros = 0;
    atual = proximo_atomo();
    proximo = proximo_atomo();

    if (atual.atomo == ERRO)
        registra_erro_lexico(atual);

    escopo_atual = GLOBAL;
    strcpy(contexto_atual, "global");

    diag_trace("ENTRA", "gvars", atual.linha);
    if (!parse_gvars()) { diag_trace("SAI", "gvars", atual.linha); fclose(arquivo_parser); return 0; }
    diag_trace("SAI", "gvars", atual.linha);
    diag_trace("ENTRA", "subs", atual.linha);
    if (!parse_subs()) { diag_trace("SAI", "subs", atual.linha); fclose(arquivo_parser); return 0; }
    diag_trace("SAI", "subs", atual.linha);
    diag_trace("ENTRA", "princ", atual.linha);
    if (!parse_princ())
    {
        fclose(arquivo_parser);
        diag_trace("SAI", "princ", atual.linha);
        diag_trace("SAI", "prg", atual.linha);
        return 0;
    }
    diag_trace("SAI", "princ", atual.linha);

    if (!eh(FIMARQUIVO))
    {
        erro("fim do arquivo");
        fclose(arquivo_parser);
        diag_trace("SAI", "prg", atual.linha);
        return 0;
    }

    fclose(arquivo_parser);

    if (erros == 0)
    {
        diag_info("Analise sintatica concluida com sucesso.");
        diag_trace("SAI", "prg", atual.linha);
        return 1;
    }
    diag_trace("SAI", "prg", atual.linha);
    return 0;
}

TabelaSimbolos *obtem_tabela_parser(void)
{
    return &tabela_parser;
}
