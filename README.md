# COMPLAC — Compilador da Linguagem SLAC²

Compilador desenvolvido em **C99** para a disciplina de Compiladores da Universidade Presbiteriana Mackenzie.

A primeira parte do projeto implementou a análise léxica, a análise sintática descendente recursiva (ASDR), a tabela de símbolos e os relatórios de diagnóstico. Na segunda parte, o objetivo é complementar a análise semântica e traduzir programas SLAC² para instruções da **MEPA** (Máquina de Execução de Pascal).

> **Status:** implementação parcial da Parte 2. A geração de código já contempla programas com variáveis escalares e comandos básicos, mas ainda não cobre toda a linguagem SLAC².

## Requisitos

- Linux ou WSL;
- compilador GCC com suporte a C99;
- `make`.

## Estrutura do projeto

| Arquivo | Responsabilidade |
|---|---|
| `main.c` | Interface de linha de comando e integração das etapas do compilador. |
| `lex.c` / `lex.h` | Analisador léxico e reconhecimento dos tokens. |
| `parser.c` / `parser.h` | Análise sintática descendente recursiva (ASDR). |
| `symtab.c` / `symtab.h` | Tabela de símbolos, tipos, escopos e endereços das variáveis. |
| `mepa.c` / `mepa.h` | Verificações semânticas adicionais e geração de instruções MEPA. |
| `diag.c` / `diag.h` | Mensagens de erro, informações e rastreamento. |
| `opts.c` / `opts.h` | Processamento dos argumentos de linha de comando. |
| `makefile` | Compilação do projeto. |
| `testes/` | Exemplos e verificações básicas. |

O compilador permanece escrito em **C**. Não é necessário Python nem um arquivo `.patch`.

## Instalação e compilação

Caso esteja partindo da Parte 1, crie uma branch para desenvolver a Parte 2:

```bash
git switch main
git pull
git switch -c feat/slac2-mepa
```

Copie os arquivos deste pacote para a raiz do repositório, substituindo `main.c`, `symtab.h` e `makefile` e adicionando `mepa.c`, `mepa.h` e a pasta `testes/`. Os outros módulos da Parte 1 permanecem no repositório.

Compile e execute os testes com:

```bash
make
```

O alvo principal `all` compila o executável e roda os três testes básicos automaticamente. Para executar somente os testes novamente, use `make test`. Para remover arquivos gerados, use `make clean`.

## Como usar

Para analisar um programa SLAC² e gerar instruções MEPA:

```bash
./complac testes/aritmetica.slac
```

Quando a análise e a geração forem bem-sucedidas, o arquivo de saída terá o mesmo nome-base da entrada, com extensão `.mepa`. Por exemplo, `testes/aritmetica.slac` gera `testes/aritmetica.mepa`.

As opções de diagnóstico da Parte 1 continuam disponíveis:

```bash
./complac testes/aritmetica.slac --tokens
./complac testes/aritmetica.slac --symtab
./complac testes/aritmetica.slac --trace
```

Essas opções geram, respectivamente, arquivos `.tk` (tokens), `.ts` (tabela de símbolos) e `.trc` (rastreamento).

## Exemplo de tradução

Código SLAC² (`testes/aritmetica.slac`):

```text
globvars
    a,b,c is int;
proc main()
start
    b << 5;
    c << 10;
    a << b+c*2;
    echo(a);
end
```

Código MEPA esperado para esse exemplo:

```text
INPP
AMEM 3
CRCT 5
ARMZ 0,1
CRCT 10
ARMZ 0,2
CRVL 0,1
CRVL 0,2
CRCT 2
MULT
SOMA
ARMZ 0,0
CRVL 0,0
IMPR
DMEM 3
PARA
```

A MEPA utiliza uma pilha para calcular expressões: `CRCT` empilha constantes, `CRVL` carrega variáveis, `MULT` e `SOMA` realizam as operações e `ARMZ` guarda o resultado.

## Recursos contemplados nesta versão

- Geração MEPA para constantes inteiras e de caractere e variáveis escalares;
- operações aritméticas (`+`, `-`, `*`, `//`) e menos unário;
- comparações e operadores lógicos;
- atribuição (`<<`), leitura (`get`) e impressão (`echo`) de valores compatíveis;
- controle de fluxo com `case`, `while` e `repeat`;
- alocação de memória para variáveis globais e variáveis locais da `main`;
- detecção de alguns erros semânticos, como identificadores não declarados e atribuições com tipos incompatíveis.

## Limitações conhecidas

A implementação **ainda não conclui o escopo integral da Parte 2**. Em especial, faltam:

- `for` e `choose`;
- vetores e acesso indexado;
- funções, procedimentos auxiliares, parâmetros e passagem por referência;
- suporte de saída para strings;
- validação completa de todos os cenários da linguagem e execução em um interpretador MEPA.

A geração é realizada em uma passagem separada após a análise sintática da Parte 1. O arquivo `.mepa` só é gravado quando essa etapa termina sem os erros detectados pelo módulo.

## Testes

Os testes são executados automaticamente com `make`, sem scripts `.sh`. Para repeti-los sem recompilar, execute:

```bash
make test
```

A pasta contém os arquivos `.slac` usados pelo alvo `test` do Makefile:

- `aritmetica.slac`: expressões aritméticas, atribuição e saída;
- `controle.slac`: laço `while` e condicional `case`;
- `erro_semantico.slac`: exemplo com atribuição entre tipos incompatíveis, que deve ser rejeitado.

Esses testes verificam parte da saída e o tratamento de um erro semântico; **não substituem uma suíte completa de testes ou testes de execução da MEPA**.

## Limpeza

```bash
make clean
```

O comando remove o executável e arquivos gerados (`.tk`, `.ts`, `.trc` e `.mepa`) da raiz do projeto. O alvo `clean` também remove os arquivos gerados dentro de `testes/`.

## Autores

| Integrante | RA |
|---|---|
| Bruna Soncini | 10428267 |
| Gabriel Medina | 10426931 |

## Referência

Material da disciplina de Compiladores: **Geração de Código / MEPA**, Prof. Dr. Leandro Carlos Fernandes. A implementação toma como referência as instruções da MEPA apresentadas em aula.
