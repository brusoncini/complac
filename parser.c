/*
 * ASDR da linguagem SLAC2 com Traducao Dirigida a Sintaxe.
 *
 * As acoes semanticas (tabela de simbolos, escopo, tipos e parametros)
 * e a emissao das instrucoes MEPA acontecem durante o proprio
 * reconhecimento sintatico, em uma unica passagem sobre o fonte.
 *
 * Organizacao da memoria na MEPA:
 *   - nivel 0: variaveis globais (deslocamentos 0..G-1) seguidas das
 *     variaveis locais de main (deslocamentos G..);
 *   - nivel 1: sub-rotinas. Locais a partir do deslocamento 0 e
 *     parametros com deslocamentos negativos:
 *         [retorno da funcao] -(P+5)
 *         [parametros]        -(P+4) .. -5
 *         [end. retorno, D antigo, nivel, elo estatico]  -4 .. -1
 *     onde P e a quantidade de posicoes ocupadas pelos parametros.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"
#include "parser.h"
#include "symtab.h"
#include "diag.h"
#include "gerador.h"

static FILE *arquivo_parser;
static TInfoAtomo atual, proximo;
static int erros;
static int erros_semanticos;
static TabelaSimbolos tabela_parser;
static TEscopo escopo_atual = GLOBAL;
static char contexto_atual[TAM_NOME] = "global";

/* Estado da geracao de codigo */
static int nivel_atual;            /* 0: global/main, 1: sub-rotinas */
static int proximo_endereco;       /* proximo deslocamento livre */
static int total_globais;          /* posicoes ocupadas pelas globais */
static int temporarios;            /* posicoes temporarias ativas (choose) */
static int usou_return;
static int teve_subrotinas;
static TSimbolo *subrotina_atual;  /* NULL dentro de main */
static TRotulo rotulo_retorno;
static TRotulo rotulo_main;

/* ------------------------------------------------------------------ */
/* Utilitarios do ASDR                                                 */
/* ------------------------------------------------------------------ */

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

static void erro_semantico(int linha, const char *mensagem, const char *lexema)
{
    diag_error(linha, "semantico", mensagem, lexema);
    erros_semanticos++;
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

static int proximo_fecha_argumento(void)
{
    return proximo.atomo == VIRGULA || proximo.atomo == FECHAPARENTESES;
}

static int eh_inicio_cmd(void)
{
    return eh(ECHO) || eh(GET) || eh(CASE) || eh(CHOOSE) || eh(FOR) ||
           eh(WHILE) || eh(REPEAT) || eh(RETURN) || eh(IDENTIFICADOR);
}

static TTipo tipo_expr(void);
static int parse_cmd(void);

/* ------------------------------------------------------------------ */
/* Acoes semanticas auxiliares                                         */
/* ------------------------------------------------------------------ */

static void exige_tipo(TTipo achado, TTipo esperado, const char *contexto, int linha)
{
    if (achado == TIPO_NAO_DEFINIDO || esperado == TIPO_NAO_DEFINIDO || achado == esperado)
        return;

    char mensagem[256];
    snprintf(mensagem, sizeof(mensagem), "tipo incompativel: esperado %s, encontrado %s",
             nome_tipo(esperado), nome_tipo(achado));
    erro_semantico(linha, mensagem, contexto);
}

static int posicoes(TSimbolo *simbolo)
{
    return simbolo->tamanho_vetor > 0 ? simbolo->tamanho_vetor : 1;
}

static TSimbolo *insere_declaracao(char *nome, TCategoriaSimbolo categoria,
                                   TTipo tipo, TEscopo escopo, int tamanho, int linha)
{
    const char *contexto = escopo == GLOBAL ? "global" : contexto_atual;
    int duplicado = busca_simbolo_contexto(&tabela_parser, nome, escopo, contexto) != NULL;

    /* Parametros e locais de uma mesma sub-rotina formam um unico escopo */
    if (escopo == LOCAL)
        duplicado |= busca_simbolo_contexto(&tabela_parser, nome, PARAMETRO, contexto) != NULL;
    else if (escopo == PARAMETRO)
        duplicado |= busca_simbolo_contexto(&tabela_parser, nome, LOCAL, contexto) != NULL;

    if (duplicado)
    {
        erro_semantico(linha, "identificador ja declarado no escopo", nome);
        return NULL;
    }

    if (!insere_simbolo_contexto(&tabela_parser, nome, categoria, tipo, escopo, contexto))
        return NULL;

    TSimbolo *simbolo = busca_simbolo_contexto(&tabela_parser, nome, escopo, contexto);
    if (simbolo != NULL)
        simbolo->tamanho_vetor = tamanho;
    return simbolo;
}

/* Declaracao previa: o identificador deve ser uma variavel visivel */
static TSimbolo *busca_variavel(const char *nome, int linha)
{
    TSimbolo *simbolo = busca_no_escopo(&tabela_parser, nome, escopo_atual, contexto_atual);

    if (simbolo == NULL)
    {
        erro_semantico(linha, "identificador nao declarado", nome);
        return NULL;
    }
    if (simbolo->categoria != VARIAVEL)
    {
        erro_semantico(linha, "sub-rotina usada como variavel", nome);
        return NULL;
    }
    return simbolo;
}

static TSimbolo *vetor_visivel(const char *nome)
{
    TSimbolo *simbolo = busca_no_escopo(&tabela_parser, nome, escopo_atual, contexto_atual);
    if (simbolo != NULL && simbolo->categoria == VARIAVEL && simbolo->tamanho_vetor > 0)
        return simbolo;
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Acesso a variaveis na MEPA                                          */
/* ------------------------------------------------------------------ */

static void gera_carrega(TSimbolo *s)
{
    gera2(s->eh_referencia ? "CRVI" : "CRVL", s->nivel, s->endereco);
}

static void gera_armazena(TSimbolo *s)
{
    gera2(s->eh_referencia ? "ARMI" : "ARMZ", s->nivel, s->endereco);
}

/* Empilha o endereco da variavel (ou o endereco guardado em um ref) */
static void gera_endereco(TSimbolo *s)
{
    gera2(s->eh_referencia ? "CRVL" : "CREN", s->nivel, s->endereco);
}

/* Comprimento de uma string apos o tratamento de escapes (\n, \t ...) */
static int comprimento_string(const char *texto, int *valida)
{
    int tamanho = 0;
    *valida = 1;
    for (int i = 0; texto[i] != '\0'; i++)
    {
        if (texto[i] == '\\')
        {
            if (texto[i + 1] == '\0') { *valida = 0; break; }
            i++;
        }
        tamanho++;
    }
    return tamanho;
}

/* Empilha a string literal (CRST) e devolve quantas posicoes ocupa */
static int gera_string(const char *texto, int linha)
{
    int valida;
    int tamanho = comprimento_string(texto, &valida);
    char literal[TAM_NOME + 4];

    if (!valida)
        erro_semantico(linha, "string termina com caractere de escape", texto);

    snprintf(literal, sizeof(literal), "\"%s\"", texto);
    gera_instr_mepa(NULL, "CRST", literal, NULL);
    return tamanho + 1;
}

/* ------------------------------------------------------------------ */
/* Declaracoes                                                         */
/* ------------------------------------------------------------------ */

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
        if (*tamanho_vetor <= 0)
            erro_semantico(atual.linha, "dimensao de vetor deve ser maior que zero", atual.texto);
        avanca();
        if (!espera(FECHACOLCHETE, "]")) return 0;
    }
    return 1;
}

static int parse_decls(TEscopo escopo)
{
    char nomes[32][TAM_NOME];
    int linhas[32];
    int quantidade = 0;

    if (!eh(IDENTIFICADOR)) { erro("identificador"); return 0; }
    linhas[quantidade] = atual.linha;
    strcpy(nomes[quantidade++], atual.texto);
    avanca();

    while (aceita(VIRGULA))
    {
        if (!eh(IDENTIFICADOR)) { erro("identificador"); return 0; }
        if (quantidade < 32)
        {
            linhas[quantidade] = atual.linha;
            strcpy(nomes[quantidade++], atual.texto);
        }
        avanca();
    }

    if (!espera(KIND, "is")) return 0;

    TTipo tipo;
    int tamanho_vetor;
    if (!parse_tipo(&tipo, &tamanho_vetor)) return 0;
    if (!espera(PONTOEVIRGULA, ";")) return 0;

    for (int i = 0; i < quantidade; i++)
    {
        TSimbolo *s = insere_declaracao(nomes[i], VARIAVEL, tipo, escopo,
                                        tamanho_vetor, linhas[i]);
        if (s != NULL)
        {
            /* Alocacao: cada variavel recebe nivel e deslocamento */
            s->nivel = nivel_atual;
            s->endereco = proximo_endereco;
            proximo_endereco += posicoes(s);
        }
    }
    return 1;
}

static int parse_gvars(void)
{
    nivel_atual = 0;
    proximo_endereco = 0;
    if (aceita(GLOBVARS))
    {
        if (!eh(IDENTIFICADOR)) { erro("declaracao de variavel"); return 0; }
        do { if (!parse_decls(GLOBAL)) return 0; } while (eh(IDENTIFICADOR));
    }
    total_globais = proximo_endereco;
    if (total_globais > 0)
        gera1("AMEM", total_globais);
    return 1;
}

/* Devolve a quantidade de posicoes alocadas para as locais */
static int parse_lvars(int *posicoes_locais)
{
    int inicio = proximo_endereco;
    *posicoes_locais = 0;
    if (!aceita(LOCVARS)) return 1;
    if (!eh(IDENTIFICADOR)) { erro("declaracao de variavel"); return 0; }
    do { if (!parse_decls(LOCAL)) return 0; } while (eh(IDENTIFICADOR));
    *posicoes_locais = proximo_endereco - inicio;
    return 1;
}

static int parse_param(TSimbolo *subrotina)
{
    int referencia = aceita(REF);
    if (!eh(IDENTIFICADOR)) { erro("identificador"); return 0; }
    char nome[TAM_NOME];
    int linha = atual.linha;
    strcpy(nome, atual.texto);
    avanca();
    if (!espera(DOISPONTOS, ":")) return 0;
    TTipo tipo;
    int tamanho_vetor;
    if (!parse_tipo(&tipo, &tamanho_vetor)) return 0;

    TSimbolo *p = insere_declaracao(nome, VARIAVEL, tipo, PARAMETRO, tamanho_vetor, linha);
    if (subrotina->quantidade_parametros >= MAX_PARAMETROS)
    {
        erro_semantico(linha, "quantidade maxima de parametros excedida", nome);
        return 1;
    }
    if (p != NULL)
    {
        p->eh_referencia = referencia;
        p->nivel = 1;
    }
    subrotina->parametros[subrotina->quantidade_parametros++] = p;
    return 1;
}

static int parse_params(TSimbolo *subrotina)
{
    subrotina->quantidade_parametros = 0;
    if (eh(IDENTIFICADOR) || eh(REF))
    {
        if (!parse_param(subrotina)) return 0;
        while (aceita(VIRGULA))
            if (!parse_param(subrotina)) return 0;
    }

    /* Calcula os deslocamentos (negativos) dos parametros */
    int total = 0;
    for (int i = 0; i < subrotina->quantidade_parametros; i++)
    {
        TSimbolo *p = subrotina->parametros[i];
        if (p != NULL)
            total += p->eh_referencia ? 1 : posicoes(p);
    }
    int deslocamento = -(total + 4);
    for (int i = 0; i < subrotina->quantidade_parametros; i++)
    {
        TSimbolo *p = subrotina->parametros[i];
        if (p == NULL) continue;
        p->endereco = deslocamento;
        deslocamento += p->eh_referencia ? 1 : posicoes(p);
    }
    subrotina->tamanho_parametros = total;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Expressoes (codigo em notacao pos-fixa)                             */
/* ------------------------------------------------------------------ */

/* id '[' expr ']' : empilha o endereco do elemento (base + indice) */
static TTipo parse_elemento_endereco(void)
{
    char nome[TAM_NOME];
    int linha = atual.linha;
    strcpy(nome, atual.texto);
    TSimbolo *s = busca_variavel(nome, linha);
    avanca();
    if (!espera(ABRECOLCHETE, "[")) return TIPO_NAO_DEFINIDO;

    int vetor = s != NULL && s->tamanho_vetor > 0;
    if (s != NULL && !vetor)
        erro_semantico(linha, "identificador nao e um vetor", nome);
    if (vetor)
        gera_endereco(s);

    /* Indice constante pode ser validado em tempo de compilacao */
    if (vetor && eh(CONSTANTE_INTEIRA) && proximo.atomo == FECHACOLCHETE)
    {
        int indice = atoi(atual.texto);
        if (indice < 0 || indice >= s->tamanho_vetor)
            erro_semantico(linha, "indice fora dos limites do vetor", nome);
    }

    int linha_indice = atual.linha;
    TTipo tipo_indice = tipo_expr();
    exige_tipo(tipo_indice, TIPO_INT, "indice de vetor", linha_indice);
    if (!espera(FECHACOLCHETE, "]")) return TIPO_NAO_DEFINIDO;

    if (vetor)
        gera1("INDX", 1);
    return vetor ? s->tipo : TIPO_NAO_DEFINIDO;
}

/* Descarta o restante de um argumento invalido ate ',' ou ')' */
static void descarta_argumento(void)
{
    int profundidade = 0;
    while (!eh(FIMARQUIVO) && !eh(PONTOEVIRGULA))
    {
        if (profundidade == 0 && (eh(VIRGULA) || eh(FECHAPARENTESES)))
            return;
        if (eh(ABREPARENTESES)) profundidade++;
        if (eh(FECHAPARENTESES)) profundidade--;
        avanca();
    }
}

static void parse_argumento(TSimbolo *parametro, const char *subrotina, int posicao)
{
    char contexto[TAM_NOME + 64];
    int linha = atual.linha;
    snprintf(contexto, sizeof(contexto), "argumento %d de %s", posicao, subrotina);

    if (parametro == NULL)
    {
        tipo_expr();
        return;
    }

    /* Parametro vetor: o argumento deve ser um vetor compativel */
    if (parametro->tamanho_vetor > 0)
    {
        if (eh(IDENTIFICADOR) && proximo_fecha_argumento())
        {
            TSimbolo *s = busca_variavel(atual.texto, linha);
            avanca();
            if (s == NULL) return;
            if (s->tamanho_vetor == 0)
                erro_semantico(linha, "esperado vetor como argumento", contexto);
            else if (s->tipo != parametro->tipo || s->tamanho_vetor != parametro->tamanho_vetor)
                erro_semantico(linha, "vetor com tipo ou dimensao incompativel", contexto);
            else
            {
                gera_endereco(s);
                if (!parametro->eh_referencia)
                    gera1("CRVM", s->tamanho_vetor); /* copia por valor */
            }
        }
        else
        {
            erro_semantico(linha, "esperado vetor como argumento", contexto);
            tipo_expr();
        }
        return;
    }

    /* Passagem por referencia: empilha o endereco de uma variavel */
    if (parametro->eh_referencia)
    {
        TTipo tipo = TIPO_NAO_DEFINIDO;
        if (eh(IDENTIFICADOR) && proximo.atomo == ABRECOLCHETE)
        {
            tipo = parse_elemento_endereco();
        }
        else if (eh(IDENTIFICADOR) && proximo_fecha_argumento())
        {
            TSimbolo *s = busca_variavel(atual.texto, linha);
            avanca();
            if (s != NULL && s->tamanho_vetor > 0)
                erro_semantico(linha, "vetor passado para parametro escalar", contexto);
            else if (s != NULL)
            {
                gera_endereco(s);
                tipo = s->tipo;
            }
        }
        else
        {
            erro_semantico(linha, "argumento por referencia deve ser uma variavel", contexto);
            descarta_argumento();
            return;
        }

        if (!eh(VIRGULA) && !eh(FECHAPARENTESES))
        {
            erro_semantico(linha, "argumento por referencia deve ser uma variavel", contexto);
            descarta_argumento();
            return;
        }
        exige_tipo(tipo, parametro->tipo, contexto, linha);
        return;
    }

    /* Passagem por valor */
    TTipo tipo = tipo_expr();
    exige_tipo(tipo, parametro->tipo, contexto, linha);
}

static TTipo parse_call(int em_expressao)
{
    char nome[TAM_NOME];
    int linha = atual.linha;
    strcpy(nome, atual.texto);
    TSimbolo *sub = busca_simbolo_contexto(&tabela_parser, nome, GLOBAL, "global");

    espera(IDENTIFICADOR, "identificador");
    if (!espera(ABREPARENTESES, "(")) return TIPO_NAO_DEFINIDO;

    int valida = 0;
    if (sub == NULL)
        erro_semantico(linha, "sub-rotina nao declarada", nome);
    else if (sub->categoria != SUBROTINA)
        erro_semantico(linha, "identificador nao e uma sub-rotina", nome);
    else if (strcmp(nome, "main") == 0)
        erro_semantico(linha, "main nao pode ser chamada", nome);
    else
        valida = 1;

    if (valida && em_expressao && sub->tipo == TIPO_VOID)
        erro_semantico(linha, "procedimento nao retorna valor e nao pode ser usado em expressao", nome);

    int funcao = valida && sub->tipo != TIPO_VOID;
    if (funcao)
        gera1("AMEM", 1); /* espaco para o valor de retorno */

    int quantidade = 0;
    if (!eh(FECHAPARENTESES))
    {
        do
        {
            TSimbolo *p = NULL;
            if (valida && quantidade < sub->quantidade_parametros)
                p = sub->parametros[quantidade];
            parse_argumento(p, nome, quantidade + 1);
            quantidade++;
        } while (aceita(VIRGULA));
    }

    if (!espera(FECHAPARENTESES, ")")) return TIPO_NAO_DEFINIDO;

    if (valida && quantidade != sub->quantidade_parametros)
    {
        char mensagem[128];
        snprintf(mensagem, sizeof(mensagem),
                 "quantidade de argumentos incompativel: esperado %d, encontrado %d",
                 sub->quantidade_parametros, quantidade);
        erro_semantico(linha, mensagem, nome);
    }

    if (valida)
        gera_instr_mepa(NULL, "CHPR", sub->rotulo, "1");

    /* Funcao chamada como comando: descarta o valor de retorno */
    if (funcao && !em_expressao)
        gera1("DMEM", 1);

    return funcao ? sub->tipo : TIPO_NAO_DEFINIDO;
}

static TTipo parse_fact(void)
{
    int linha = atual.linha;

    if (aceita(NEGACAO))
    {
        exige_tipo(parse_fact(), TIPO_LOGIC, "~", linha);
        gera0("NEGA");
        return TIPO_LOGIC;
    }

    if (aceita(SUBTRACAO))
    {
        exige_tipo(parse_fact(), TIPO_INT, "- unario", linha);
        gera0("INVR");
        return TIPO_INT;
    }

    if (aceita(ABREPARENTESES))
    {
        TTipo tipo = tipo_expr();
        espera(FECHAPARENTESES, ")");
        return tipo;
    }

    if (eh(CONSTANTE_INTEIRA))
    {
        gera_instr_mepa(NULL, "CRCT", atual.texto, NULL);
        avanca();
        return TIPO_INT;
    }

    if (eh(CONSTANTE_CARACTERE))
    {
        gera1("CRCT", (unsigned char)atual.texto[0]);
        avanca();
        return TIPO_CHR;
    }

    if (eh(STRING))
    {
        erro_semantico(linha, "string so pode ser usada em echo ou atribuida a vetor chr", atual.texto);
        avanca();
        return TIPO_NAO_DEFINIDO;
    }

    if (eh(IDENTIFICADOR))
    {
        if (proximo.atomo == ABREPARENTESES)
            return parse_call(1);

        if (proximo.atomo == ABRECOLCHETE)
        {
            TTipo tipo = parse_elemento_endereco();
            if (tipo != TIPO_NAO_DEFINIDO)
                gera0("CONT");
            return tipo;
        }

        TSimbolo *s = busca_variavel(atual.texto, linha);
        char nome[TAM_NOME];
        strcpy(nome, atual.texto);
        avanca();
        if (s == NULL) return TIPO_NAO_DEFINIDO;
        if (s->tamanho_vetor > 0)
        {
            erro_semantico(linha, "vetor usado sem indice em expressao", nome);
            return TIPO_NAO_DEFINIDO;
        }
        gera_carrega(s);
        return s->tipo;
    }

    erro("elemento da expressao");
    return TIPO_NAO_DEFINIDO;
}

static TTipo parse_earip(void)
{
    TTipo esquerda = parse_fact();

    while (eh(MULTIPLICACAO) || eh(DIVISAO))
    {
        TAtomo op = atual.atomo;
        int linha = atual.linha;
        avanca();
        TTipo direita = parse_fact();
        exige_tipo(esquerda, TIPO_INT, op == MULTIPLICACAO ? "*" : "/", linha);
        exige_tipo(direita, TIPO_INT, op == MULTIPLICACAO ? "*" : "/", linha);
        gera0(op == MULTIPLICACAO ? "MULT" : "DIVI");
        esquerda = TIPO_INT;
    }

    return esquerda;
}

static TTipo parse_earit(void)
{
    TTipo esquerda = parse_earip();

    while (eh(SOMA) || eh(SUBTRACAO))
    {
        TAtomo op = atual.atomo;
        int linha = atual.linha;
        avanca();
        TTipo direita = parse_earip();
        exige_tipo(esquerda, TIPO_INT, op == SOMA ? "+" : "-", linha);
        exige_tipo(direita, TIPO_INT, op == SOMA ? "+" : "-", linha);
        gera0(op == SOMA ? "SOMA" : "SUBT");
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
        char simbolo[TAM_NOME];
        int linha = atual.linha;
        snprintf(simbolo, sizeof(simbolo), "%s", atual.texto);
        avanca();
        TTipo direita = parse_earit();

        if (esquerda != TIPO_NAO_DEFINIDO && direita != TIPO_NAO_DEFINIDO)
        {
            if (esquerda != direita)
                erro_semantico(linha, "comparacao entre tipos diferentes", simbolo);
            else if (op != IGUAL && op != DIFERENTE && esquerda == TIPO_LOGIC)
                erro_semantico(linha, "operador relacional de ordem nao se aplica a logic", simbolo);
        }

        const char *inst = op == MAIOR ? "CMMA" : op == MAIORIGUAL ? "CMAG" :
                           op == MENOR ? "CMME" : op == MENORIGUAL ? "CMEG" :
                           op == IGUAL ? "CMIG" : "CMDG";
        gera0(inst);
        esquerda = TIPO_LOGIC;
    }

    return esquerda;
}

static TTipo parse_elogc(void)
{
    TTipo esquerda = parse_erlac();

    while (eh(E_LOGICO))
    {
        int linha = atual.linha;
        avanca();
        TTipo direita = parse_erlac();
        exige_tipo(esquerda, TIPO_LOGIC, "&", linha);
        exige_tipo(direita, TIPO_LOGIC, "&", linha);
        gera0("CONJ");
        esquerda = TIPO_LOGIC;
    }

    return esquerda;
}

static TTipo tipo_expr(void)
{
    TTipo esquerda = parse_elogc();

    while (eh(OU_LOGICO))
    {
        int linha = atual.linha;
        avanca();
        TTipo direita = parse_elogc();
        exige_tipo(esquerda, TIPO_LOGIC, "|", linha);
        exige_tipo(direita, TIPO_LOGIC, "|", linha);
        gera0("DISJ");
        esquerda = TIPO_LOGIC;
    }

    return esquerda;
}

/* ------------------------------------------------------------------ */
/* Comandos                                                            */
/* ------------------------------------------------------------------ */

static void parse_elemento_echo(void)
{
    int linha = atual.linha;

    /* String literal: CRST + IMPS */
    if (eh(STRING) && proximo_fecha_argumento())
    {
        int k = gera_string(atual.texto, linha);
        gera1("IMPS", k);
        avanca();
        return;
    }

    /* Vetor chr inteiro, no formato [tamanho, c1, c2, ...] */
    if (eh(IDENTIFICADOR) && proximo_fecha_argumento())
    {
        TSimbolo *v = vetor_visivel(atual.texto);
        if (v != NULL)
        {
            if (v->tipo != TIPO_CHR)
                erro_semantico(linha, "apenas vetores chr podem ser impressos diretamente", v->lexema);
            else
            {
                gera_endereco(v);
                gera1("CRVM", v->tamanho_vetor);
                gera1("IMPS", v->tamanho_vetor);
            }
            avanca();
            return;
        }
    }

    TTipo tipo = tipo_expr();
    if (tipo == TIPO_CHR) gera0("IMPC");
    else if (tipo == TIPO_INT || tipo == TIPO_LOGIC) gera0("IMPR");
}

static int parse_echo(void)
{
    if (!espera(ECHO, "echo")) return 0;
    if (!espera(ABREPARENTESES, "(")) return 0;
    parse_elemento_echo();
    while (aceita(VIRGULA))
        parse_elemento_echo();
    return espera(FECHAPARENTESES, ")");
}

static int parse_get(void)
{
    if (!espera(GET, "get")) return 0;
    if (!espera(ABREPARENTESES, "(")) return 0;
    int linha = atual.linha;
    if (!eh(IDENTIFICADOR)) { erro("identificador"); return 0; }

    TTipo tipo;
    if (proximo.atomo == ABRECOLCHETE)
    {
        tipo = parse_elemento_endereco();
        if (tipo != TIPO_NAO_DEFINIDO)
        {
            gera0("LEIT");
            gera1("ARVM", 1);
        }
    }
    else
    {
        TSimbolo *s = busca_variavel(atual.texto, linha);
        avanca();
        tipo = TIPO_NAO_DEFINIDO;
        if (s != NULL && s->tamanho_vetor > 0)
            erro_semantico(linha, "get nao pode ler um vetor inteiro", s->lexema);
        else if (s != NULL)
        {
            tipo = s->tipo;
            gera0("LEIT");
            gera_armazena(s);
        }
    }

    if (tipo == TIPO_LOGIC)
        erro_semantico(linha, "get aceita apenas variaveis int ou chr", "get");
    return espera(FECHAPARENTESES, ")");
}

static int parse_atr(void)
{
    int linha = atual.linha;
    char nome[TAM_NOME];
    strcpy(nome, atual.texto);

    /* v[i] << expr */
    if (proximo.atomo == ABRECOLCHETE)
    {
        TTipo destino = parse_elemento_endereco();
        if (!espera(ATRIBUICAO, "<<")) return 0;
        TTipo origem = tipo_expr();
        exige_tipo(origem, destino, "atribuicao", linha);
        if (destino != TIPO_NAO_DEFINIDO)
            gera1("ARVM", 1);
        return 1;
    }

    if (!eh(IDENTIFICADOR)) { erro("identificador"); return 0; }
    TSimbolo *s = busca_variavel(nome, linha);
    avanca();
    if (!espera(ATRIBUICAO, "<<")) return 0;

    /* Atribuicao de vetor inteiro: string literal ou outro vetor */
    if (s != NULL && s->tamanho_vetor > 0)
    {
        int n = s->tamanho_vetor;
        if (eh(STRING) && proximo.atomo == PONTOEVIRGULA)
        {
            int valida;
            int tamanho = comprimento_string(atual.texto, &valida);
            if (s->tipo != TIPO_CHR)
                erro_semantico(linha, "string so pode ser atribuida a vetor chr", nome);
            else if (tamanho + 1 > n)
                erro_semantico(linha, "string maior que a capacidade do vetor", nome);
            else
            {
                gera_endereco(s);
                int k = gera_string(atual.texto, linha);
                for (; k < n; k++)
                    gera1("CRCT", 0); /* completa o bloco do vetor */
                gera1("ARVM", n);
            }
            avanca();
            return 1;
        }
        if (eh(IDENTIFICADOR) && proximo.atomo == PONTOEVIRGULA)
        {
            TSimbolo *o = busca_variavel(atual.texto, atual.linha);
            avanca();
            if (o == NULL) return 1;
            if (o->tamanho_vetor != n || o->tipo != s->tipo)
                erro_semantico(linha, "atribuicao entre vetores incompativeis", nome);
            else
            {
                gera_endereco(s);
                gera_endereco(o);
                gera1("CRVM", n);
                gera1("ARVM", n);
            }
            return 1;
        }
        erro_semantico(linha, "vetor so pode receber string ou vetor compativel", nome);
        tipo_expr();
        return 1;
    }

    TTipo origem = tipo_expr();
    if (s != NULL)
    {
        if (origem != TIPO_NAO_DEFINIDO && origem != s->tipo)
            erro_semantico(linha, "atribuicao com tipos incompativeis", nome);
        gera_armazena(s);
    }
    return 1;
}

static int parse_ret(void)
{
    int linha = atual.linha;
    if (!espera(RETURN, "return")) return 0;

    int funcao = subrotina_atual != NULL && subrotina_atual->tipo != TIPO_VOID;
    if (funcao)
    {
        if (eh(PONTOEVIRGULA))
            erro_semantico(linha, "return sem valor em funcao", subrotina_atual->lexema);
        else
        {
            TTipo tipo = tipo_expr();
            exige_tipo(tipo, subrotina_atual->tipo, "valor de retorno", linha);
            gera2("ARMZ", 1, -(subrotina_atual->tamanho_parametros + 5));
        }
    }
    else if (!eh(PONTOEVIRGULA))
    {
        erro_semantico(linha, "procedimento nao pode retornar valor",
                       subrotina_atual ? subrotina_atual->lexema : "main");
        tipo_expr();
    }

    if (!usou_return)
        novo_rotulo(rotulo_retorno);
    usou_return = 1;
    if (temporarios > 0)
        gera1("DMEM", temporarios);
    gera_desvio("DSVS", rotulo_retorno);
    return 1;
}

static int parse_condicao(const char *comando)
{
    int linha = atual.linha;
    TTipo tipo = tipo_expr();
    exige_tipo(tipo, TIPO_LOGIC, comando, linha);
    return 1;
}

static int parse_case(void)
{
    TRotulo falso, fim;
    if (!espera(CASE, "case")) return 0;
    if (!espera(ABREPARENTESES, "(")) return 0;
    parse_condicao("condicao do case");
    if (!espera(FECHAPARENTESES, ")")) return 0;

    novo_rotulo(falso);
    gera_desvio("DSVF", falso);
    if (!parse_cmd()) return 0;
    if (!espera(PONTOEVIRGULA, ";")) return 0;

    if (aceita(OTHERWISE))
    {
        novo_rotulo(fim);
        gera_desvio("DSVS", fim);
        gera_rotulo(falso);
        if (!parse_cmd()) return 0;
        if (!espera(PONTOEVIRGULA, ";")) return 0;
        gera_rotulo(fim);
    }
    else
        gera_rotulo(falso);

    return espera(END, "end");
}

/* match c1, c2, ... -> cmd ; */
static int parse_match(int temporario, const char *fim, int *vistos, int *quantidade_vistos)
{
    TRotulo proximo_match;
    int constantes = 0;

    if (!espera(MATCH, "match")) return 0;
    do
    {
        if (!eh(CONSTANTE_INTEIRA)) { erro("constante inteira"); return 0; }
        int valor = atoi(atual.texto);
        for (int i = 0; i < *quantidade_vistos; i++)
            if (vistos[i] == valor)
                erro_semantico(atual.linha, "constante repetida no choose", atual.texto);
        if (*quantidade_vistos < 256)
            vistos[(*quantidade_vistos)++] = valor;

        gera2("CRVL", nivel_atual, temporario);
        gera1("CRCT", valor);
        gera0("CMIG");
        if (constantes > 0)
            gera0("DISJ");
        constantes++;
        avanca();
    } while (aceita(VIRGULA));

    if (!espera(IMPLIC, "->")) return 0;
    novo_rotulo(proximo_match);
    gera_desvio("DSVF", proximo_match);
    if (!parse_cmd()) return 0;
    if (!espera(PONTOEVIRGULA, ";")) return 0;
    gera_desvio("DSVS", fim);
    gera_rotulo(proximo_match);
    return 1;
}

static int parse_choose(void)
{
    TRotulo fim;
    int vistos[256];
    int quantidade_vistos = 0;

    if (!espera(CHOOSE, "choose")) return 0;
    if (!espera(ABREPARENTESES, "(")) return 0;

    /* O seletor e avaliado uma unica vez e guardado em um temporario */
    int temporario = proximo_endereco + temporarios;
    temporarios++;
    gera1("AMEM", 1);
    int linha = atual.linha;
    exige_tipo(tipo_expr(), TIPO_INT, "seletor do choose", linha);
    gera2("ARMZ", nivel_atual, temporario);

    if (!espera(FECHAPARENTESES, ")")) return 0;
    if (!eh(MATCH)) { erro("match"); return 0; }

    novo_rotulo(fim);
    while (eh(MATCH))
        if (!parse_match(temporario, fim, vistos, &quantidade_vistos)) return 0;

    if (aceita(OTHERS))
    {
        if (!espera(IMPLIC, "->")) return 0;
        if (!parse_cmd()) return 0;
        if (!espera(PONTOEVIRGULA, ";")) return 0;
    }

    gera_rotulo(fim);
    gera1("DMEM", 1);
    temporarios--;
    return espera(END, "end");
}

/* Limite do for: identificador ou constante inteira */
static int parse_limite_for(void)
{
    int linha = atual.linha;
    if (eh(CONSTANTE_INTEIRA))
    {
        gera_instr_mepa(NULL, "CRCT", atual.texto, NULL);
        avanca();
        return 1;
    }
    if (eh(IDENTIFICADOR))
    {
        TSimbolo *s = busca_variavel(atual.texto, linha);
        avanca();
        if (s != NULL && s->tamanho_vetor > 0)
            erro_semantico(linha, "limite do for nao pode ser vetor", s->lexema);
        else if (s != NULL)
        {
            exige_tipo(s->tipo, TIPO_INT, "limite do for", linha);
            gera_carrega(s);
        }
        return 1;
    }
    erro("identificador ou constante inteira");
    return 0;
}

static int parse_for(void)
{
    TRotulo inicio, fim;
    if (!espera(FOR, "for")) return 0;
    int linha = atual.linha;
    if (!eh(IDENTIFICADOR)) { erro("identificador"); return 0; }

    TSimbolo *controle = busca_variavel(atual.texto, linha);
    if (controle != NULL && controle->tamanho_vetor > 0)
    {
        erro_semantico(linha, "variavel de controle do for nao pode ser vetor", controle->lexema);
        controle = NULL;
    }
    else if (controle != NULL && controle->tipo != TIPO_INT)
    {
        erro_semantico(linha, "variavel de controle do for deve ser int", controle->lexema);
        controle = NULL;
    }
    avanca();

    /* inicializacao */
    if (!espera(FROM, "from")) return 0;
    if (!parse_limite_for()) return 0;
    if (controle) gera_armazena(controle);

    /* condicao: controle <= limite */
    novo_rotulo(inicio);
    novo_rotulo(fim);
    gera_rotulo(inicio);
    if (controle) gera_carrega(controle);
    if (!espera(TO, "to")) return 0;
    if (!parse_limite_for()) return 0;
    gera0("CMEG");
    gera_desvio("DSVF", fim);

    int passo = 1;
    if (aceita(BY))
    {
        if (!eh(CONSTANTE_INTEIRA)) { erro("constante inteira"); return 0; }
        passo = atoi(atual.texto);
        if (passo <= 0)
            erro_semantico(atual.linha, "passo do for deve ser maior que zero", atual.texto);
        avanca();
    }

    if (!espera(DO, "do")) return 0;
    while (!eh(END) && !eh(FIMARQUIVO))
    {
        if (!parse_cmd()) return 0;
        if (!espera(PONTOEVIRGULA, ";")) return 0;
    }

    /* incremento e desvio para o teste */
    if (controle)
    {
        gera_carrega(controle);
        gera1("CRCT", passo);
        gera0("SOMA");
        gera_armazena(controle);
    }
    gera_desvio("DSVS", inicio);
    gera_rotulo(fim);
    return espera(END, "end");
}

static int parse_while(void)
{
    TRotulo inicio, fim;
    if (!espera(WHILE, "while")) return 0;
    if (!espera(ABREPARENTESES, "(")) return 0;
    novo_rotulo(inicio);
    novo_rotulo(fim);
    gera_rotulo(inicio);
    parse_condicao("condicao do while");
    if (!espera(FECHAPARENTESES, ")")) return 0;
    if (!espera(DO, "do")) return 0;
    gera_desvio("DSVF", fim);
    while (!eh(END) && !eh(FIMARQUIVO))
    {
        if (!parse_cmd()) return 0;
        if (!espera(PONTOEVIRGULA, ";")) return 0;
    }
    gera_desvio("DSVS", inicio);
    gera_rotulo(fim);
    return espera(END, "end");
}

static int parse_repeat(void)
{
    TRotulo inicio;
    if (!espera(REPEAT, "repeat")) return 0;
    novo_rotulo(inicio);
    gera_rotulo(inicio);
    while (eh_inicio_cmd())
    {
        if (!parse_cmd()) return 0;
        if (!espera(PONTOEVIRGULA, ";")) return 0;
    }
    if (!espera(UNTIL, "until")) return 0;
    if (!espera(ABREPARENTESES, "(")) return 0;
    parse_condicao("condicao do repeat");
    gera_desvio("DSVF", inicio);
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
        if (proximo.atomo == ABREPARENTESES) { parse_call(0); return 1; }
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

/* ------------------------------------------------------------------ */
/* Sub-rotinas e programa principal                                    */
/* ------------------------------------------------------------------ */

static int parse_subrotina(int eh_funcao)
{
    if (!espera(eh_funcao ? FUNC : PROC, eh_funcao ? "func" : "proc")) return 0;
    if (!eh(IDENTIFICADOR)) { erro("identificador"); return 0; }
    char nome[TAM_NOME];
    int linha = atual.linha;
    strcpy(nome, atual.texto);
    avanca();
    if (!espera(ABREPARENTESES, "(")) return 0;

    /* A sub-rotina e registrada antes do corpo para permitir recursao */
    TSimbolo *sub = insere_declaracao(nome, SUBROTINA, TIPO_VOID, GLOBAL, 0, linha);
    if (sub == NULL) return 0;
    novo_rotulo(sub->rotulo);

    TEscopo escopo_anterior = escopo_atual;
    char contexto_anterior[TAM_NOME];
    strcpy(contexto_anterior, contexto_atual);
    strcpy(contexto_atual, nome);
    escopo_atual = PARAMETRO;
    nivel_atual = 1;

    if (!parse_params(sub)) return 0;
    if (!espera(FECHAPARENTESES, ")")) return 0;

    if (eh_funcao)
    {
        TTipo tipo;
        int tamanho_vetor;
        if (!espera(DOISPONTOS, ":")) return 0;
        if (!parse_tipo(&tipo, &tamanho_vetor)) return 0;
        if (tamanho_vetor > 0)
            erro_semantico(linha, "funcao nao pode retornar vetor", nome);
        sub->tipo = tipo;
    }

    subrotina_atual = sub;
    usou_return = 0;
    temporarios = 0;

    gera_instr_mepa(sub->rotulo, "ENPR", "1", NULL);

    escopo_atual = LOCAL;
    proximo_endereco = 0;
    int locais;
    if (!parse_lvars(&locais)) return 0;
    if (locais > 0) gera1("AMEM", locais);

    if (!parse_bco()) return 0;

    if (usou_return) gera_rotulo(rotulo_retorno);
    if (locais > 0) gera1("DMEM", locais);
    gera1("RTPR", sub->tamanho_parametros);

    if (eh_funcao && !usou_return)
        erro_semantico(linha, "funcao sem comando return", nome);

    escopo_atual = escopo_anterior;
    strcpy(contexto_atual, contexto_anterior);
    subrotina_atual = NULL;
    nivel_atual = 0;
    return 1;
}

static int eh_inicio_main(void)
{
    return eh(PROC) && proximo.atomo == IDENTIFICADOR && strcmp(proximo.texto, "main") == 0;
}

static int parse_subs(void)
{
    teve_subrotinas = eh(FUNC) || (eh(PROC) && !eh_inicio_main());
    if (teve_subrotinas)
    {
        /* O codigo das sub-rotinas fica antes de main: desvia por cima */
        novo_rotulo(rotulo_main);
        gera_desvio("DSVS", rotulo_main);
    }

    while (eh(FUNC) || (eh(PROC) && !eh_inicio_main()))
    {
        if (!parse_subrotina(eh(FUNC))) return 0;
    }
    return 1;
}

static int parse_princ(void)
{
    if (!eh_inicio_main()) { erro("proc main"); return 0; }
    avanca();
    int linha = atual.linha;
    avanca();
    if (!espera(ABREPARENTESES, "(")) return 0;

    char nome[] = "main";
    TSimbolo *principal = insere_declaracao(nome, SUBROTINA, TIPO_VOID, GLOBAL, 0, linha);
    if (principal == NULL) return 0;

    strcpy(contexto_atual, "main");
    escopo_atual = PARAMETRO;
    if (!parse_params(principal)) return 0;
    if (principal->quantidade_parametros > 0)
        erro_semantico(linha, "main nao pode ter parametros", "main");
    if (!espera(FECHAPARENTESES, ")")) return 0;

    if (teve_subrotinas)
        gera_rotulo(rotulo_main);

    /* Locais de main ficam no nivel 0, logo apos as globais */
    escopo_atual = LOCAL;
    nivel_atual = 0;
    subrotina_atual = NULL;
    usou_return = 0;
    temporarios = 0;
    proximo_endereco = total_globais;

    int locais;
    if (!parse_lvars(&locais)) return 0;
    if (locais > 0) gera1("AMEM", locais);

    if (!parse_bco()) return 0;

    if (usou_return) gera_rotulo(rotulo_retorno);
    if (locais > 0) gera1("DMEM", locais);
    return 1;
}

static int finaliza_com_erro(void)
{
    fclose(arquivo_parser);
    diag_trace("SAI", "prg", atual.linha);
    return 0;
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
    erros_semanticos = 0;
    teve_subrotinas = 0;
    subrotina_atual = NULL;
    atual = proximo_atomo();
    proximo = proximo_atomo();

    if (atual.atomo == ERRO)
        registra_erro_lexico(atual);

    escopo_atual = GLOBAL;
    strcpy(contexto_atual, "global");

    /* <prg> ::= [<gvars>] {<subs>} <princ> */
    gera0("INPP");

    diag_trace("ENTRA", "gvars", atual.linha);
    if (!parse_gvars()) { diag_trace("SAI", "gvars", atual.linha); return finaliza_com_erro(); }
    diag_trace("SAI", "gvars", atual.linha);

    diag_trace("ENTRA", "subs", atual.linha);
    if (!parse_subs()) { diag_trace("SAI", "subs", atual.linha); return finaliza_com_erro(); }
    diag_trace("SAI", "subs", atual.linha);

    diag_trace("ENTRA", "princ", atual.linha);
    if (!parse_princ()) { diag_trace("SAI", "princ", atual.linha); return finaliza_com_erro(); }
    diag_trace("SAI", "princ", atual.linha);

    if (!eh(FIMARQUIVO))
    {
        erro("fim do arquivo");
        return finaliza_com_erro();
    }

    if (total_globais > 0)
        gera1("DMEM", total_globais);
    gera0("PARA");
    gera0("FIM");

    fclose(arquivo_parser);
    diag_trace("SAI", "prg", atual.linha);

    if (erros == 0 && erros_semanticos == 0)
    {
        diag_info("Analise sintatica e semantica concluida com sucesso.");
        return 1;
    }
    return 0;
}

TabelaSimbolos *obtem_tabela_parser(void)
{
    return &tabela_parser;
}
