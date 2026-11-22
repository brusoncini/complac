# COMPLAC — Compilador da Linguagem SLAC²

Compilador desenvolvido em **C99** para a disciplina de Compiladores da Universidade Presbiteriana Mackenzie.

O compilador lê um programa SLAC², realiza as análises léxica, sintática e semântica e gera um arquivo equivalente com instruções da **MEPA** (Máquina de Execução de Pascal).

## Requisitos

- Linux (Ubuntu) ou WSL;
- GCC com suporte a C99 e `make`;
- opcional: Python 3 e o interpretador MEPA da disciplina, para executar os programas gerados nos testes.

## Compilação e testes

```bash
make          # compila o compilador e executa todos os testes
make test     # executa somente os testes
make clean    # remove o executável e os arquivos gerados
```

O compilador é construído com `gcc -Wall -Wextra -std=c99`, sem avisos.

Para que os testes também **executem** os programas gerados, copie os arquivos do interpretador (`mepa.py`, `mepa_defs.py`, `mepa_instr.py`, `mepa_interp.py` e `mepa_strings.py`) para a pasta `interpretador/` ou informe o caminho:

```bash
make test MEPA=/caminho/para/mepa.py
```

Sem o interpretador, os programas válidos são apenas compilados.

## Uso

```bash
./complac programa.slac
```

Se não houver erros, é gerado `programa.mepa` na mesma pasta. Havendo qualquer erro léxico, sintático ou semântico, nenhum arquivo é gerado e um `.mepa` antigo com o mesmo nome é removido.

Para executar o código gerado:

```bash
python3 interpretador/mepa.py --progfile programa.mepa
```

Opções de diagnóstico da Parte 1 (continuam disponíveis):

| Opção | Arquivo gerado |
|---|---|
| `--tokens` | `.tk` — lista de tokens |
| `--symtab` | `.ts` — tabela de símbolos |
| `--trace` | `.trc` — rastreamento do parser |

## Estrutura do projeto

| Arquivo | Responsabilidade |
|---|---|
| `main.c` | CLI e coordenação das etapas; grava o `.mepa` apenas em caso de sucesso. |
| `lex.c` / `lex.h` | Analisador léxico. |
| `parser.c` / `parser.h` | ASDR com as ações semânticas e a geração de código (tradução dirigida pela sintaxe). |
| `symtab.c` / `symtab.h` | Tabela de símbolos (hash) com escopos global, parâmetros e locais; guarda nível, deslocamento, tipo, dimensão e parâmetros. |
| `gerador.c` / `gerador.h` | Emissão formatada das instruções MEPA (`gera_instr_mepa`) e criação de rótulos (`novo_rotulo`). |
| `diag.c` / `diag.h` | Mensagens de erro e rastreamento. |
| `opts.c` / `opts.h` | Processamento dos argumentos de linha de comando. |
| `testes/` | Programas de teste positivos e negativos. |

## Tradução dirigida pela sintaxe

A análise semântica e a geração de código acontecem **durante a análise sintática**, em uma única passagem: cada função do ASDR, ao reconhecer sua produção, consulta e atualiza a tabela de símbolos, valida os tipos e emite as instruções MEPA correspondentes. As instruções ficam em um arquivo temporário e só são gravadas no `.mepa` se não houver erros.

### Organização da memória

- **Nível 0:** variáveis globais (deslocamentos `0..G-1`), seguidas das variáveis locais de `main`.
- **Nível 1:** sub-rotinas. Variáveis locais a partir do deslocamento `0`; parâmetros com deslocamentos negativos. Sendo `P` o número de posições ocupadas pelos parâmetros:

| Deslocamento | Conteúdo |
|---|---|
| `-(P+5)` | valor de retorno (funções) |
| `-(P+4)` … `-5` | parâmetros, na ordem de declaração |
| `-4` … `-1` | endereço de retorno, D antigo, nível e elo (empilhados por `CHPR` e `ENPR`) |

Vetores ocupam uma posição por elemento (índices de `0` a `tamanho-1`). Parâmetros `ref` ocupam uma posição com o endereço da variável; vetores passados por valor são copiados com `CRVM`.

### Recursos implementados

| Construção | Tradução MEPA |
|---|---|
| Expressões aritméticas, relacionais e lógicas | notação pós-fixa (`SOMA`, `MULT`, `CMME`, `CONJ`, `NEGA`, `INVR`...) |
| Atribuição `<<` | `ARMZ` / `ARMI` (ref) / `ARVM 1` (elemento de vetor) |
| `case ... otherwise ... end` | `DSVF` / `DSVS` |
| `while`, `repeat ... until` | rótulos e desvios |
| `for i from a to b [by k] do ... end` | inicialização, teste com `CMEG`, incremento e desvio |
| `choose (e) match c1, c2 -> ... others -> ... end` | seletor avaliado uma vez em temporário, comparações com `CMIG`/`DISJ` |
| Vetores | `CREN` + índice + `INDX 1`, leitura com `CONT`, escrita com `ARVM 1` |
| `proc` e `func`, chamadas e recursão | `CHPR`, `ENPR 1`, `RTPR n`; funções reservam o retorno com `AMEM 1` |
| Parâmetros por valor e por referência (`ref`) | valor, endereço (`CREN`) ou cópia de vetor (`CRVM`); acesso com `CRVI`/`ARMI` |
| `return` | grava o valor de retorno e desvia para o fim da sub-rotina |
| `get(...)` | `LEIT` + armazenamento (escalares e elementos de vetor) |
| `echo(...)` | `IMPR` (int/logic), `IMPC` (chr), `CRST` + `IMPS` (string literal), `CRVM` + `IMPS` (vetor `chr`) |
| Strings em vetores `chr` | `v << "texto"` grava `[tamanho, c1, c2, ...]` com `CRST` + `ARVM` |

### Verificações semânticas

- identificadores não declarados e declarações duplicadas no mesmo escopo (parâmetros e locais formam um único escopo);
- visibilidade: locais e parâmetros escondem globais; locais de outra sub-rotina não são visíveis;
- tipos em expressões, atribuições, índices e condições (`case`, `while`, `repeat` exigem `logic`; `for` exige variável e limites `int`; `choose` exige seletor `int`);
- chamadas: sub-rotina declarada, quantidade e tipos de argumentos, argumento `ref` deve ser variável, vetores com tipo e dimensão compatíveis, procedimento não pode ser usado em expressão;
- vetores: uso sem índice, indexação de escalar, índice constante fora dos limites, dimensão inválida, string maior que o vetor;
- retorno: tipo do valor retornado, função sem `return`, `return` com valor em procedimento, função não pode retornar vetor;
- `main` sem parâmetros e não pode ser chamada; constantes repetidas em `choose`.

## Exemplo de tradução

```text
func dobro(n: int): int
start
    return n * 2;
end

proc main()
locvars
    x is int;
start
    x << dobro(21);
    echo(x);
end
```

```text
      INPP
      DSVS L1
L2:   ENPR 1
      CRVL 1,-5
      CRCT 2
      MULT
      ARMZ 1,-6
      DSVS L3
L3:   NADA
      RTPR 1
L1:   NADA
      AMEM 1
      AMEM 1
      CRCT 21
      CHPR L2,1
      ARMZ 0,0
      CRVL 0,0
      IMPR
      DMEM 1
      PARA
      FIM
```

Mais exemplos estão em `testes/validos/` (cada `.slac` acompanha a saída esperada em `.esperado`).

## Testes

- `testes/validos/`: 11 programas cobrindo expressões, controle de fluxo, `for`, `choose`, vetores, sub-rotinas, recursão, `ref`, strings, `return` e escopo. Cada um é compilado e, com o interpretador disponível, executado e comparado com o `.esperado` (a entrada vem do `.entrada`, quando existe).
- `testes/invalidos/`: 30 programas com um erro cada (semântico ou sintático). O teste confirma que o compilador os rejeita e que nenhum `.mepa` é deixado.

## Autores

| Integrante | RA |
|---|---|
| Bruna Soncini | 10428267 |
| Gabriel Medina | 10426931 |

## Referência

Material da disciplina de Compiladores: **Geração de Código / MEPA**, Prof. Dr. Leandro Carlos Fernandes.
