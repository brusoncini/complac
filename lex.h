#ifndef LEX_H
#define LEX_H

#define TAM_LEXEMA 256

typedef enum
{

    S_PROC,
    S_FUNC,
    S_MAIN,
    S_GLOBVARS,
    S_LOCVARS,
    S_START,
    S_END,
    S_ECHO,
    S_GET,
    S_CASE,
    S_OTHERWISE,
    S_CHOOSE,
    S_MATCH,
    S_OTHERS,
    S_FOR,
    S_FROM,
    S_TO,
    S_BY,
    S_WHILE,
    S_REPEAT,
    S_UNTIL,
    S_RETURN,
    S_INT,
    S_LOGIC,
    S_CHR,
    S_IDENTIF,
    S_CTEINT,
    S_STRING,
    S_CTECHAR,
    S_ATRIB,
    S_SOMA,
    S_SUBRAT,
    S_MULT,
    S_DIV,
    S_IGUAL,
    S_DIFERENTE,
    S_MAIOR,
    S_MENOR,
    S_MAIORIGUAL,
    S_MENORIGUAL,
    S_AND,
    S_OR,
    S_NEG,
    S_ABREPAR,
    S_FECHAPAR,
    S_ABRECOL,
    S_FECHACOL,
    S_VIRGULA,
    S_PONTOEVIRGULA,
    S_DOISPONTOS,
    S_ERRO,
    S_FIM

} TAtomo;

typedef struct
{
    TAtomo atomo;
    char lexema[TAM_LEXEMA];
    int linha;

} TInfoAtomo;

void lex_init(char *nomeArquivo);

TInfoAtomo lex_next();

char *lex_token_name(TAtomo token);

#endif