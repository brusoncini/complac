# COMPLAC - Compilador da Linguagem SLAC²

<p align="center">
  <img src="https://img.shields.io/badge/C-99-blue.svg">
  <img src="https://img.shields.io/badge/GCC-Linux-green.svg">
  <img src="https://img.shields.io/badge/status-em%20desenvolvimento-yellow">
  <img src="https://img.shields.io/badge/tipo-projeto%20acadêmico-purple">
</p>

Compilador desenvolvido em C para a linguagem **SLAC²**, criado para a disciplina de Compiladores.
A Parte 1 implementa a frente de análise do compilador:

- análise léxica;
- análise sintática descendente recursiva (ASDR);
- tabela de símbolos;
- diagnósticos;
- interface de linha de comando;
- geração opcional de logs.

## Requisitos

- Linux ou WSL;
- GCC;
- `make`.

O projeto deve ser compilado com:

```bash
make
```

### Módulos

- `main`: processa a CLI, abre os arquivos, inicializa a análise e controla a geração dos logs.
- `lex`: realiza a varredura sequencial do fonte e devolve um token por chamada, com categoria, lexema e linha.
- `parser`: implementa o ASDR e consome os tokens produzidos pelo lexer.
- `symtab`: mantém a tabela de símbolos, incluindo categoria, tipo, escopo, contexto, tamanho de vetor e quantidade de parâmetros.
- `diag`: centraliza mensagens de erro, informações e rastreamento.
- `opts`: interpreta os parâmetros da linha de comando.

A comunicação entre módulos ocorre por meio dos arquivos `.h`.

## Uso

O formato principal é:

```bash
./complac <arquivo.slac>
```

Também é possível ativar um dos logs:

```bash
./complac <arquivo.slac> --tokens
./complac <arquivo.slac> --symtab
./complac <arquivo.slac> --trace
```

### `--tokens`

Gera um arquivo `.tk` com a lista dos tokens reconhecidos:

```text
<L> <CATEGORIA> "<LEXEMA>"
```

Exemplo:

```text
1 sPROC "proc"
1 sIDENTIF "main"
1 sABREPAREN "("
```

### `--symtab`

Gera um arquivo `.ts` com a tabela de símbolos em ordem de inserção:

```text
SCOPE=<escopo> id="<lexema>" cat=<categoria> tipo=<tipo> extra=<atributo>
```

Os escopos registrados incluem globais, parâmetros e locais de cada sub-rotina.

Exemplo:

```text
SCOPE=global id="idade" cat=VARIAVEL tipo=INT extra=-
SCOPE=proc:soma.params id="a" cat=VARIAVEL tipo=INT extra=-
SCOPE=proc:soma.locals id="resultado" cat=VARIAVEL tipo=INT extra=-
```

Para vetores, `extra` registra o tamanho:

```text
extra=VETOR=10
```

Para sub-rotinas, registra a quantidade de parâmetros:

```text
extra=PARAMETROS=2
```

### `--trace`

Gera um arquivo `.trc` com o rastreamento da entrada e saída das principais produções do parser.

Exemplo:

```text
ENTRA prg linha=1
ENTRA gvars linha=1
SAI gvars linha=6
ENTRA subs linha=6
SAI subs linha=...
ENTRA princ linha=...
SAI princ linha=...
SAI prg linha=...
```

## Limpeza

Para remover o binário e os logs gerados:

```bash
make clean
```

## 🚧 Status

Projeto em desenvolvimento!!!

## 👩🏻‍💻 Feito por

| Integrante     | RA       |
| -------------- | -------- |
| Bruna Soncini  | 10428267 |
| Gabriel Medina | 10426931 |
