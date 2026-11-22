CC = gcc
CFLAGS = -Wall -Wextra -std=c99
ARQUIVOS = main.c lex.c symtab.c parser.c diag.c opts.c mepa.c

# make compila e executa os testes automaticamente.
all: complac test

complac: $(ARQUIVOS) lex.h parser.h symtab.h diag.h opts.h mepa.h
	$(CC) $(CFLAGS) $(ARQUIVOS) -o complac

# Os testes usam apenas o proprio compilador e comandos basicos do terminal.
test: complac
	@echo "===== Teste 1: expressoes aritmeticas ====="
	@./complac testes/aritmetica.slac
	@test -f testes/aritmetica.mepa
	@grep -q '^INPP$$' testes/aritmetica.mepa
	@grep -q '^MULT$$' testes/aritmetica.mepa
	@grep -q '^SOMA$$' testes/aritmetica.mepa
	@grep -q '^PARA$$' testes/aritmetica.mepa
	@echo "OK: aritmetica"
	@echo "===== Teste 2: fluxo de controle ====="
	@./complac testes/controle.slac
	@test -f testes/controle.mepa
	@grep -q '^DSVF L' testes/controle.mepa
	@echo "OK: controle"
	@echo "===== Teste 3: erro semantico esperado ====="
	@rm -f testes/erro_semantico.mepa
	@if ./complac testes/erro_semantico.slac; then \
		echo "FALHA: o compilador aceitou um programa com erro semantico"; \
		exit 1; \
	else \
		echo "OK: erro semantico rejeitado"; \
	fi
	@test ! -f testes/erro_semantico.mepa
	@echo "===== Todos os testes basicos passaram ====="

clean:
	rm -f complac *.tk *.ts *.trc *.mepa testes/*.tk testes/*.ts testes/*.trc testes/*.mepa

.PHONY: all test clean
