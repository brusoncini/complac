#include <stdio.h>
#include "lex.h"
#include "symtab.h"
#include "parser.h"

static void teste_lexer(void)
{
    char arquivo_nome[256];
    printf("Arquivo .slac: ");
    scanf("%255s", arquivo_nome);
    FILE *arquivo = fopen(arquivo_nome, "r");
    if (arquivo == NULL)
    {
        printf("Erro ao abrir arquivo.\n");
        return;
    }
    inicializa_lex(arquivo);
    TInfoAtomo atomo;
    do
    {
        atomo = proximo_atomo();
        printf("%d | %-25s | %s\n", atomo.linha, nome_atomo(atomo.atomo), atomo.texto);
    } while (atomo.atomo != FIMARQUIVO);
    fclose(arquivo);
}

static void teste_tabela_simbolos(void)
{
    TabelaSimbolos tabela;
    inicializa_tabela(&tabela);
    insere_simbolo(&tabela, "idade", VARIAVEL, TIPO_INT, GLOBAL);
    insere_simbolo(&tabela, "nome", VARIAVEL, TIPO_CHR, LOCAL);
    insere_simbolo(&tabela, "soma", SUBROTINA, TIPO_INT, GLOBAL);
    imprime_tabela(&tabela);
}

static void teste_parser(void)
{
    char arquivo_nome[256];
    printf("Arquivo .slac: ");
    scanf("%255s", arquivo_nome);
    if (analisa_sintaxe(arquivo_nome))
        printf("Programa valido.\n");
    else
        printf("Programa invalido.\n");
}

static void teste_parser_tabela(void)
{
    char arquivo_nome[256];
    printf("Arquivo .slac: ");
    scanf("%255s", arquivo_nome);

    if (analisa_sintaxe(arquivo_nome))
    {
        printf("Programa valido.\n");
        imprime_tabela(obtem_tabela_parser());
    }
    else
    {
        printf("Programa invalido.\n");
    }
}

int main(void)
{
    int opcao;
    do
    {
        printf("\n===== COMPLAC =====\n");
        printf("1 - Testar analisador lexico\n");
        printf("2 - Testar tabela de simbolos\n");
        printf("3 - Testar analise sintatica e semantica\n");
        printf("4 - Testar parser + tabela de simbolos\n");
        printf("0 - Sair\n");
        printf("\nOpcao: ");
        scanf("%d", &opcao);

        switch (opcao)
        {
        case 1: teste_lexer(); break;
        case 2: teste_tabela_simbolos(); break;
        case 3: teste_parser(); break;
        case 4: teste_parser_tabela(); break;
        case 0: printf("Encerrando...\n"); break;
        default: printf("Opcao invalida.\n");
        }
    } while (opcao != 0);
    return 0;
}
