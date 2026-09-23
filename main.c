#include <stdio.h>

#include "lex.h"
#include "symtab.h"

void teste_lexer()
{
    char arquivo_nome[256];

    printf("Arquivo .slac: ");
    scanf("%s", arquivo_nome);

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

        printf(
            "%d | %-25s | %s\n",
            atomo.linha,
            nome_atomo(atomo.atomo),
            atomo.texto);

    } while (atomo.atomo != FIMARQUIVO);

    fclose(arquivo);
}

void teste_tabela_simbolos()
{
    TabelaSimbolos tabela;

    inicializa_tabela(&tabela);

    insere_simbolo(
        &tabela,
        "idade",
        VARIAVEL,
        TIPO_INT,
        GLOBAL);

    insere_simbolo(
        &tabela,
        "nome",
        VARIAVEL,
        TIPO_CHR,
        LOCAL);

    insere_simbolo(
        &tabela,
        "soma",
        SUBROTINA,
        TIPO_INT,
        GLOBAL);

    imprime_tabela(&tabela);
}

int main()
{
    int opcao;

    do
    {
        printf("\n===== COMPLAC =====\n");
        printf("1 - Testar analisador lexico\n");
        printf("2 - Testar tabela de simbolos\n");
        printf("0 - Sair\n");
        printf("\nOpcao: ");

        scanf("%d", &opcao);

        switch (opcao)
        {

        case 1:
            teste_lexer();
            break;

        case 2:
            teste_tabela_simbolos();
            break;

        case 0:
            printf("Encerrando...\n");
            break;

        default:
            printf("Opcao invalida.\n");
        }

    } while (opcao != 0);

    return 0;
}