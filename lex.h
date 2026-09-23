#ifndef LEX_H
#define LEX_H

#include <stdio.h>

typedef enum
{
    IDENTIFICADOR,
    CONSTANTE_INTEIRA,
    CONSTANTE_CARACTERE,
    STRING,
    PROC,
    FUNC,
    START,
    END,
    GLOBVARS,
    LOCVARS,
    ECHO,
    GET,
    CASE,
    OTHERWISE,
    CHOOSE,
    MATCH,
    OTHERS,
    FOR,
    FROM,
    TO,
    BY,
    DO,
    WHILE,
    REPEAT,
    UNTIL,
    RETURN,
    INT,
    LOGIC,
    CHR,
    ATRIBUICAO,
    SOMA,
    SUBTRACAO,
    MULTIPLICACAO,
    DIVISAO,
    IGUAL,
    DIFERENTE,
    MAIOR,
    MENOR,
    MAIORIGUAL,
    MENORIGUAL,
    E_LOGICO,
    OU_LOGICO,
    NEGACAO,
    ABREPARENTESES,
    FECHAPARENTESES,
    ABRECOLCHETE,
    FECHACOLCHETE,
    VIRGULA,
    PONTOEVIRGULA,
    DOISPONTOS,
    ERRO,
    FIMARQUIVO
    
} TAtomo;

typedef struct
{
    TAtomo atomo;
    char texto[256];
    int linha;

} TInfoAtomo;

void inicializa_lex(FILE *arquivo);

TInfoAtomo proximo_atomo();

char *nome_atomo(TAtomo atomo);

#endif