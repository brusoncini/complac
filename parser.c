#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"
#include "parser.h"
#include "symtab.h"
#include "semantic.h"
#include "diag.h"

static FILE *arquivo_parser;
static TInfoAtomo atual, proximo;
static int erros;
static TabelaSimbolos tabela_parser;
static TEscopo escopo_atual = GLOBAL;
static TTipo tipo_retorno = TIPO_VOID;
static char contexto_atual[TAM_NOME] = "global";

static void avanca(void)
{
    atual = proximo;
    proximo = proximo_atomo();
}

static void erro(const char *esperado)
{
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

static int sem_erro(void) { return quantidade_erros_semanticos() == 0; }

static int eh_inicio_cmd(void)
{
    return eh(ECHO) || eh(GET) || eh(CASE) || eh(CHOOSE) || eh(FOR) ||
           eh(WHILE) || eh(REPEAT) || eh(RETURN) || eh(IDENTIFICADOR);
}

static TTipo tipo_expr(void);
static TTipo tipo_elem(void);
static int parse_cmd(void);
static int parse_bco(void);

static int eh_numerico(TTipo tipo) { return tipo == TIPO_INT; }

static TTipo busca_tipo_identificador(const char *nome, int linha)
{
    TSimbolo *simbolo = busca_no_escopo(&tabela_parser, nome, escopo_atual, contexto_atual);
    if (simbolo == NULL)
    {
        erro_semantico(linha, "identificador nao declarado", nome);
        return TIPO_NAO_DEFINIDO;
    }
    return simbolo->tipo;
}

static TSimbolo *busca_simbolo_atual(const char *nome, int linha)
{
    TSimbolo *simbolo = busca_no_escopo(&tabela_parser, nome, escopo_atual, contexto_atual);
    if (simbolo == NULL)
        erro_semantico(linha, "identificador nao declarado", nome);
    return simbolo;
}

static int insere_declaracao(char *nome, TCategoriaSimbolo categoria,
                             TTipo tipo, TEscopo escopo, int tamanho)
{
    if (busca_simbolo_contexto(&tabela_parser, nome, escopo, escopo == GLOBAL ? "global" : contexto_atual) != NULL)
    {
        erro_semantico(atual.linha, "identificador ja declarado no escopo", nome);
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
        insere_declaracao(nomes[i], VARIAVEL, tipo, escopo, tamanho_vetor);
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
    int linha = atual.linha;
    TSimbolo *simbolo = busca_simbolo_atual(nome, linha);
    avanca();
    if (!espera(ABRECOLCHETE, "[")) return TIPO_NAO_DEFINIDO;
    TTipo indice = tipo_expr();
    if (indice != TIPO_INT && indice != TIPO_NAO_DEFINIDO)
        erro_semantico(linha, "indice de vetor deve ser inteiro", nome);
    if (!espera(FECHACOLCHETE, "]")) return TIPO_NAO_DEFINIDO;
    if (simbolo != NULL && simbolo->tamanho_vetor == 0)
        erro_semantico(linha, "identificador nao e vetor", nome);
    return simbolo ? simbolo->tipo : TIPO_NAO_DEFINIDO;
}

static TTipo parse_call(void)
{
    char nome[TAM_NOME];
    strcpy(nome, atual.texto);
    int linha = atual.linha;
    TSimbolo *simbolo = busca_simbolo_contexto(&tabela_parser, nome, GLOBAL, "global");

    if (simbolo == NULL)
        erro_semantico(linha, "subrotina nao declarada", nome);
    else if (simbolo->categoria != SUBROTINA)
        erro_semantico(linha, "identificador nao e subrotina", nome);

    espera(IDENTIFICADOR, "identificador");
    if (!espera(ABREPARENTESES, "(")) return TIPO_NAO_DEFINIDO;

    int quantidade = 0;
    if (!eh(FECHAPARENTESES))
    {
        if (tipo_expr() == TIPO_NAO_DEFINIDO) { }
        quantidade++;
        while (aceita(VIRGULA))
        {
            tipo_expr();
            quantidade++;
        }
    }

    if (!espera(FECHAPARENTESES, ")")) return TIPO_NAO_DEFINIDO;
    if (simbolo != NULL && simbolo->quantidade_parametros != quantidade)
        erro_semantico(linha, "quantidade de argumentos diferente da declaracao", nome);
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
        TTipo tipo = parse_fact();
        if (tipo != TIPO_LOGIC && tipo != TIPO_NAO_DEFINIDO)
            erro_semantico(atual.linha, "negacao exige expressao logica", "~");
        return TIPO_LOGIC;
    }
    if (aceita(SUBTRACAO))
    {
        TTipo tipo = parse_fact();
        if (!eh_numerico(tipo) && tipo != TIPO_NAO_DEFINIDO)
            erro_semantico(atual.linha, "sinal negativo exige inteiro", "-");
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
        if (proximo.atomo == ABREPARENTESES) return parse_call();
        if (proximo.atomo == ABRECOLCHETE) return parse_vetor();
        char nome[TAM_NOME];
        strcpy(nome, atual.texto);
        int linha = atual.linha;
        TTipo tipo = busca_tipo_identificador(nome, linha);
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
        TTipo direita = parse_fact();
        if ((esquerda != TIPO_INT || direita != TIPO_INT) &&
            esquerda != TIPO_NAO_DEFINIDO && direita != TIPO_NAO_DEFINIDO)
            erro_semantico(atual.linha, "operacao aritmetica exige inteiros", "* ou /");
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
        TTipo direita = parse_earip();
        if ((esquerda != TIPO_INT || direita != TIPO_INT) &&
            esquerda != TIPO_NAO_DEFINIDO && direita != TIPO_NAO_DEFINIDO)
            erro_semantico(atual.linha, "operacao aritmetica exige inteiros", "+ ou -");
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
        TAtomo op = atual.atomo;
        avanca();
        TTipo direita = parse_earit();
        if (op == IGUAL || op == DIFERENTE)
        {
            if (esquerda != direita && esquerda != TIPO_NAO_DEFINIDO && direita != TIPO_NAO_DEFINIDO)
                erro_semantico(atual.linha, "comparacao exige tipos iguais", "comparacao");
        }
        else if ((esquerda != TIPO_INT || direita != TIPO_INT) &&
                 esquerda != TIPO_NAO_DEFINIDO && direita != TIPO_NAO_DEFINIDO)
            erro_semantico(atual.linha, "comparacao relacional exige inteiros", "comparacao");
        esquerda = TIPO_LOGIC;
    }
    return esquerda;
}

static TTipo parse_elogc(void)
{
    TTipo esquerda = parse_erlac();
    while (aceita(E_LOGICO))
    {
        TTipo direita = parse_erlac();
        if ((esquerda != TIPO_LOGIC || direita != TIPO_LOGIC) &&
            esquerda != TIPO_NAO_DEFINIDO && direita != TIPO_NAO_DEFINIDO)
            erro_semantico(atual.linha, "operador logico exige valores logicos", "&");
        esquerda = TIPO_LOGIC;
    }
    return esquerda;
}

static TTipo tipo_expr(void)
{
    TTipo esquerda = parse_elogc();
    while (aceita(OU_LOGICO))
    {
        TTipo direita = parse_elogc();
        if ((esquerda != TIPO_LOGIC || direita != TIPO_LOGIC) &&
            esquerda != TIPO_NAO_DEFINIDO && direita != TIPO_NAO_DEFINIDO)
            erro_semantico(atual.linha, "operador logico exige valores logicos", "|");
        esquerda = TIPO_LOGIC;
    }
    return esquerda;
}

static TTipo tipo_elem(void) { return tipo_expr(); }

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
        char nome[TAM_NOME];
        strcpy(nome, atual.texto);
        int linha = atual.linha;
        TSimbolo *simbolo = busca_simbolo_atual(nome, linha);
        if (proximo.atomo == ABRECOLCHETE) parse_vetor();
        else avanca();
        if (simbolo != NULL && simbolo->categoria != VARIAVEL)
            erro_semantico(linha, "get exige uma variavel", nome);
    }
    else erro("identificador");
    return espera(FECHAPARENTESES, ")");
}

static int parse_atr(void)
{
    char nome[TAM_NOME];
    strcpy(nome, atual.texto);
    int linha = atual.linha;
    TSimbolo *simbolo = busca_simbolo_atual(nome, linha);
    TTipo alvo = TIPO_NAO_DEFINIDO;

    if (proximo.atomo == ABRECOLCHETE)
        alvo = parse_vetor();
    else
    {
        espera(IDENTIFICADOR, "identificador");
        if (simbolo != NULL) alvo = simbolo->tipo;
    }

    if (!espera(ATRIBUICAO, "<<")) return 0;
    TTipo valor = tipo_elem();
    if (alvo != TIPO_NAO_DEFINIDO && valor != TIPO_NAO_DEFINIDO && alvo != valor)
        erro_semantico(linha, "tipos incompativeis na atribuicao", nome);
    return 1;
}

static int parse_ret(void)
{
    int linha = atual.linha;
    if (!espera(RETURN, "return")) return 0;
    TTipo tipo = tipo_elem();
    if (tipo_retorno == TIPO_VOID)
        erro_semantico(linha, "procedimento nao pode retornar valor", "return");
    else if (tipo != TIPO_NAO_DEFINIDO && tipo != tipo_retorno)
        erro_semantico(linha, "tipo de retorno incompatível", "return");
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
    char nome[TAM_NOME];
    strcpy(nome, atual.texto);
    int linha = atual.linha;
    if (!espera(IDENTIFICADOR, "identificador")) return 0;
    TTipo tipo = busca_tipo_identificador(nome, linha);
    if (tipo != TIPO_INT && tipo != TIPO_NAO_DEFINIDO)
        erro_semantico(linha, "variavel de controle deve ser inteira", nome);
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
    TTipo tipo = tipo_expr();
    if (tipo != TIPO_LOGIC && tipo != TIPO_NAO_DEFINIDO)
        erro_semantico(atual.linha, "condicao do while deve ser logica", "while");
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
    TTipo tipo = tipo_expr();
    if (tipo != TIPO_LOGIC && tipo != TIPO_NAO_DEFINIDO)
        erro_semantico(atual.linha, "condicao do repeat deve ser logica", "until");
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
    TTipo retorno_anterior = tipo_retorno;
    char contexto_anterior[TAM_NOME];
    strcpy(contexto_anterior, contexto_atual);
    strcpy(contexto_atual, nome);
    escopo_atual = PARAMETRO;
    int quantidade_parametros;
    if (!parse_params(&quantidade_parametros)) return 0;
    if (!espera(FECHAPARENTESES, ")")) return 0;

    insere_declaracao(nome, SUBROTINA, TIPO_VOID, GLOBAL, 0);
    TSimbolo *simbolo = busca_simbolo_contexto(&tabela_parser, nome, GLOBAL, "global");
    if (simbolo != NULL) simbolo->quantidade_parametros = quantidade_parametros;

    escopo_atual = LOCAL;
    tipo_retorno = TIPO_VOID;
    if (!parse_lvars()) return 0;
    int resultado = parse_bco();
    escopo_atual = anterior;
    tipo_retorno = retorno_anterior;
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
    TTipo retorno_anterior = tipo_retorno;
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
    insere_declaracao(nome, SUBROTINA, tipo, GLOBAL, 0);
    TSimbolo *simbolo = busca_simbolo_contexto(&tabela_parser, nome, GLOBAL, "global");
    if (simbolo != NULL) simbolo->quantidade_parametros = quantidade_parametros;

    escopo_atual = LOCAL;
    tipo_retorno = tipo;
    if (!parse_lvars()) return 0;
    int resultado = parse_bco();
    escopo_atual = anterior;
    tipo_retorno = retorno_anterior;
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
    inicializa_semantica();
    inicializa_lex(arquivo_parser);
    atual = proximo_atomo();
    proximo = proximo_atomo();
    erros = 0;
    escopo_atual = GLOBAL;
    tipo_retorno = TIPO_VOID;
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

    if (erros == 0 && sem_erro())
    {
        diag_info("Analise sintatica concluida com sucesso.");
        diag_info("Analise semantica concluida com sucesso.");
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
