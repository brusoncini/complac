#include "mepa.h"
#include "lex.h"
#include "symtab.h"
#include "parser.h"
#include "diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Traducao dirigida a sintaxe: a cada producao reconhecida, as
   instrucoes correspondentes sao emitidas em ordem pos-fixa. */
static FILE *entrada, *codigo;
static TInfoAtomo tok, look;
static TabelaSimbolos *simbolos;
static int erros, rotulos, globais, locais;
static char contexto[TAM_NOME];

static void emitir(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(codigo, fmt, args);
    va_end(args);
    fputc('\n', codigo);
}
static void erro_sem(const char *msg, const char *id) {
    diag_error(tok.linha, "semantico", msg, id);
    erros++;
}
static void avancar(void) { tok = look; look = proximo_atomo(); }
static int aceitar(TAtomo a) { if (tok.atomo != a) return 0; avancar(); return 1; }
static int exigir(TAtomo a, const char *nome) {
    if (aceitar(a)) return 1;
    erro_sem("token inesperado na geracao MEPA", nome);
    return 0;
}
static int rotulo(void) { return ++rotulos; }
static void marca(int n) { emitir("L%d: NADA", n); }
static TSimbolo *variavel(const char *nome) {
    TSimbolo *s = busca_no_escopo(simbolos, nome, LOCAL, contexto);
    if (!s || s->categoria != VARIAVEL) {
        erro_sem("variavel nao declarada", nome);
        return NULL;
    }
    if (s->tamanho_vetor) {
        erro_sem("vetores ainda nao suportados pela geracao MEPA", nome);
        return NULL;
    }
    if (s->escopo == PARAMETRO) {
        erro_sem("parametros exigem convencao de chamadas MEPA", nome);
        return NULL;
    }
    return s;
}
static void acesso(const char *instrucao, TSimbolo *s) {
    if (s) emitir("%s 0,%d", instrucao, s->endereco);
}
static void verificar(TTipo achado, TTipo esperado, const char *op) {
    if (achado != TIPO_NAO_DEFINIDO && achado != esperado)
        erro_sem("operacao com tipo incompativel", op);
}
static TTipo expressao(void);
static TTipo fator(void) {
    if (aceitar(NEGACAO)) {
        verificar(fator(), TIPO_LOGIC, "~"); emitir("NEGA"); return TIPO_LOGIC;
    }
    if (aceitar(SUBTRACAO)) {
        verificar(fator(), TIPO_INT, "-"); emitir("INVR"); return TIPO_INT;
    }
    if (aceitar(ABREPARENTESES)) {
        TTipo t = expressao(); exigir(FECHAPARENTESES, ")"); return t;
    }
    if (tok.atomo == CONSTANTE_INTEIRA) {
        emitir("CRCT %s", tok.texto); avancar(); return TIPO_INT;
    }
    if (tok.atomo == CONSTANTE_CARACTERE) {
        emitir("CRCT %u", (unsigned char)tok.texto[0]); avancar(); return TIPO_CHR;
    }
    if (tok.atomo == STRING) {
        erro_sem("strings nao suportadas por IMPR", tok.texto);
        avancar(); return TIPO_NAO_DEFINIDO;
    }
    if (tok.atomo == IDENTIFICADOR) {
        char nome[TAM_NOME]; snprintf(nome, sizeof(nome), "%s", tok.texto);
        TSimbolo *s = variavel(nome);
        avancar();
        if (tok.atomo == ABRECOLCHETE || tok.atomo == ABREPARENTESES) {
            erro_sem("vetores e chamadas de funcao ainda nao suportados", nome);
            return TIPO_NAO_DEFINIDO;
        }
        acesso("CRVL", s);
        return s ? s->tipo : TIPO_NAO_DEFINIDO;
    }
    erro_sem("expressao invalida", tok.texto);
    return TIPO_NAO_DEFINIDO;
}
static TTipo produto(void) {
    TTipo e = fator();
    while (tok.atomo == MULTIPLICACAO || tok.atomo == DIVISAO) {
        TAtomo op = tok.atomo; avancar(); TTipo d = fator();
        verificar(e, TIPO_INT, "aritmetica"); verificar(d, TIPO_INT, "aritmetica");
        emitir(op == MULTIPLICACAO ? "MULT" : "DIVI"); e = TIPO_INT;
    }
    return e;
}
static TTipo soma(void) {
    TTipo e = produto();
    while (tok.atomo == SOMA || tok.atomo == SUBTRACAO) {
        TAtomo op = tok.atomo; avancar(); TTipo d = produto();
        verificar(e, TIPO_INT, "aritmetica"); verificar(d, TIPO_INT, "aritmetica");
        emitir(op == SOMA ? "SOMA" : "SUBT"); e = TIPO_INT;
    }
    return e;
}
static TTipo relacao(void) {
    TTipo e = soma();
    while (tok.atomo == MAIOR || tok.atomo == MAIORIGUAL || tok.atomo == IGUAL ||
           tok.atomo == MENOR || tok.atomo == MENORIGUAL || tok.atomo == DIFERENTE) {
        TAtomo op = tok.atomo; avancar(); TTipo d = soma();
        if (e != TIPO_NAO_DEFINIDO && d != TIPO_NAO_DEFINIDO && e != d)
            erro_sem("comparacao entre tipos diferentes", "comparacao");
        if (op != IGUAL && op != DIFERENTE) {
            verificar(e, TIPO_INT, "comparacao"); verificar(d, TIPO_INT, "comparacao");
        }
        const char *inst = op == MAIOR ? "CMMA" : op == MAIORIGUAL ? "CMAG" :
                           op == MENOR ? "CMME" : op == MENORIGUAL ? "CMEG" :
                           op == IGUAL ? "CMIG" : "CMDG";
        emitir("%s", inst); e = TIPO_LOGIC;
    }
    return e;
}
static TTipo conjuncao(void) {
    TTipo e = relacao();
    while (aceitar(E_LOGICO)) {
        TTipo d = relacao();
        verificar(e, TIPO_LOGIC, "&"); verificar(d, TIPO_LOGIC, "&");
        emitir("CONJ"); e = TIPO_LOGIC;
    }
    return e;
}
static TTipo expressao(void) {
    TTipo e = conjuncao();
    while (aceitar(OU_LOGICO)) {
        TTipo d = conjuncao();
        verificar(e, TIPO_LOGIC, "|"); verificar(d, TIPO_LOGIC, "|");
        emitir("DISJ"); e = TIPO_LOGIC;
    }
    return e;
}
static int comando(void);
static int bloco_comandos(void) {
    while (tok.atomo != END && tok.atomo != FIMARQUIVO) {
        if (!comando() || !exigir(PONTOEVIRGULA, ";")) return 0;
    }
    return 1;
}
static int comando(void) {
    if (aceitar(ECHO)) {
        if (!exigir(ABREPARENTESES, "(")) return 0;
        expressao(); emitir("IMPR");
        while (aceitar(VIRGULA)) { expressao(); emitir("IMPR"); }
        return exigir(FECHAPARENTESES, ")");
    }
    if (aceitar(GET)) {
        if (!exigir(ABREPARENTESES, "(")) return 0;
        if (tok.atomo != IDENTIFICADOR) { erro_sem("esperado identificador", tok.texto); return 0; }
        TSimbolo *s = variavel(tok.texto); avancar();
        if (!exigir(FECHAPARENTESES, ")")) return 0;
        emitir("LEIT"); acesso("ARMZ", s); return 1;
    }
    if (aceitar(CASE)) {
        if (!exigir(ABREPARENTESES, "(")) return 0;
        verificar(expressao(), TIPO_LOGIC, "case");
        if (!exigir(FECHAPARENTESES, ")")) return 0;
        int falso = rotulo(), fim = rotulo(); emitir("DSVF L%d", falso);
        if (!comando() || !exigir(PONTOEVIRGULA, ";")) return 0;
        if (aceitar(OTHERWISE)) {
            emitir("DSVS L%d", fim); marca(falso);
            if (!comando() || !exigir(PONTOEVIRGULA, ";")) return 0;
            marca(fim);
        } else marca(falso);
        return exigir(END, "end");
    }
    if (aceitar(WHILE)) {
        if (!exigir(ABREPARENTESES, "(")) return 0;
        int inicio = rotulo(), fim = rotulo(); marca(inicio);
        verificar(expressao(), TIPO_LOGIC, "while");
        if (!exigir(FECHAPARENTESES, ")") || !exigir(DO, "do")) return 0;
        emitir("DSVF L%d", fim);
        if (!bloco_comandos()) return 0;
        emitir("DSVS L%d", inicio); marca(fim);
        return exigir(END, "end");
    }
    if (aceitar(REPEAT)) {
        int inicio = rotulo(); marca(inicio);
        while (tok.atomo != UNTIL && tok.atomo != FIMARQUIVO) {
            if (!comando() || !exigir(PONTOEVIRGULA, ";")) return 0;
        }
        if (!exigir(UNTIL, "until") || !exigir(ABREPARENTESES, "(")) return 0;
        verificar(expressao(), TIPO_LOGIC, "until");
        if (!exigir(FECHAPARENTESES, ")")) return 0;
        emitir("DSVF L%d", inicio); return 1;
    }
    if (tok.atomo == IDENTIFICADOR) {
        char nome[TAM_NOME]; snprintf(nome, sizeof(nome), "%s", tok.texto);
        TSimbolo *s = variavel(nome); avancar();
        if (!exigir(ATRIBUICAO, "<<")) return 0;
        TTipo rhs = expressao();
        if (s && rhs != TIPO_NAO_DEFINIDO && s->tipo != rhs)
            erro_sem("atribuicao com tipos incompativeis", nome);
        acesso("ARMZ", s); return 1;
    }
    erro_sem("comando nao suportado pela geracao MEPA", tok.texto);
    return 0;
}
static int contar_memoria(void) {
    TSimbolo *s;
    globais = locais = 0;
    for (s = simbolos->primeiro; s; s = s->ordem_proximo) {
        if (s->categoria != VARIAVEL) continue;
        if (s->escopo == GLOBAL) {
            if (s->tamanho_vetor) { erro_sem("vetores ainda nao suportados", s->lexema); continue; }
            s->endereco = globais++;
        } else if (s->escopo == LOCAL && strcmp(s->contexto, "main") == 0) {
            if (s->tamanho_vetor) { erro_sem("vetores ainda nao suportados", s->lexema); continue; }
            s->endereco = globais + locais++;
        } else if (s->escopo == PARAMETRO && strcmp(s->contexto, "main") == 0) {
            erro_sem("main nao pode ter parametros nesta versao", s->lexema);
        }
    }
    return erros == 0;
}
static void descartar_declaracoes(void) {
    /* Os tipos e identificadores ja foram validados pelo ASDR da Parte 1.
       Aqui apenas avancamos ate a abertura da proxima producao. */
    while (tok.atomo != PROC && tok.atomo != FUNC && tok.atomo != FIMARQUIVO) avancar();
}
int gera_mepa(const char *fonte) {
    char destino[1024];
    entrada = fopen(fonte, "r");
    if (!entrada) { diag_error(0, "arquivo", "nao foi possivel abrir fonte", fonte); return 0; }
    snprintf(destino, sizeof(destino), "%s", fonte);
    char *ponto = strrchr(destino, '.');
    if (ponto) *ponto = '\0';
    if (strlen(destino) + 6 >= sizeof(destino)) { fclose(entrada); return 0; }
    strcat(destino, ".mepa");
    codigo = tmpfile();
    if (!codigo) { fclose(entrada); return 0; }
    simbolos = obtem_tabela_parser();
    erros = rotulos = 0;
    strcpy(contexto, "main");
    inicializa_lex(entrada); tok = proximo_atomo(); look = proximo_atomo();
    contar_memoria();
    emitir("INPP"); emitir("AMEM %d", globais + locais);
    descartar_declaracoes();
    /* A versao parcial admite apenas proc main(), sem subrotinas. */
    if (!exigir(PROC, "proc main") || tok.atomo != IDENTIFICADOR || strcmp(tok.texto, "main") != 0) {
        erro_sem("apenas proc main() e suportado nesta etapa", tok.texto);
        goto fim;
    }
    avancar();
    if (!exigir(ABREPARENTESES, "(") || !exigir(FECHAPARENTESES, ")")) goto fim;
    if (aceitar(LOCVARS)) {
        while (tok.atomo == IDENTIFICADOR) {
            while (tok.atomo != PONTOEVIRGULA && tok.atomo != FIMARQUIVO) avancar();
            if (!exigir(PONTOEVIRGULA, ";")) goto fim;
        }
    }
    if (!exigir(START, "start") || !bloco_comandos() || !exigir(END, "end")) goto fim;
    if (tok.atomo != FIMARQUIVO) erro_sem("tokens apos main", tok.texto);
    emitir("DMEM %d", globais + locais); emitir("PARA");
fim:
    fclose(entrada);
    if (erros == 0) {
        FILE *saida = fopen(destino, "w");
        if (!saida) { diag_error(0, "arquivo", "nao foi possivel gravar MEPA", destino); erros++; }
        else {
            int c; rewind(codigo);
            while ((c = fgetc(codigo)) != EOF) fputc(c, saida);
            fclose(saida);
            printf("Codigo MEPA gerado: %s\n", destino);
        }
    }
    fclose(codigo);
    return erros == 0;
}
