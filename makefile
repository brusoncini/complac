CC = gcc
CFLAGS = -Wall -Wextra -std=c99
ARQUIVOS = main.c lex.c symtab.c parser.c diag.c opts.c gerador.c
CABECALHOS = lex.h parser.h symtab.h diag.h opts.h gerador.h

# Interpretador MEPA fornecido na disciplina (Python 3). Se nao existir,
# os testes validos apenas verificam a compilacao.
MEPA ?= interpretador/mepa.py
PYTHON ?= python3

# make compila e executa os testes automaticamente.
all: complac test

complac: $(ARQUIVOS) $(CABECALHOS)
	$(CC) $(CFLAGS) $(ARQUIVOS) -o complac

# testes/validos:   devem compilar; se o interpretador estiver disponivel,
#                   a saida da MEPA e comparada com o arquivo .esperado
#                   (a entrada padrao vem do .entrada, quando existir).
# testes/invalidos: devem ser rejeitados e nao podem deixar arquivo .mepa
#                   (um .mepa antigo e criado antes para verificar a remocao).
test: complac
	@falhas=0; executar=0; \
	if [ -f "$(MEPA)" ] && command -v $(PYTHON) >/dev/null 2>&1; then executar=1; \
	else echo "AVISO: interpretador '$(MEPA)' nao encontrado; execucao MEPA nao sera testada."; fi; \
	echo "===== Programas validos ====="; \
	for f in testes/validos/*.slac; do \
		b=$${f%.slac}; \
		if ! ./complac $$f >/dev/null; then echo "FALHA (nao compilou): $$f"; falhas=$$((falhas+1)); continue; fi; \
		if [ ! -f $$b.mepa ]; then echo "FALHA (sem .mepa): $$f"; falhas=$$((falhas+1)); continue; fi; \
		if [ $$executar -eq 1 ]; then \
			entrada=/dev/null; [ -f $$b.entrada ] && entrada=$$b.entrada; \
			$(PYTHON) $(MEPA) --silent --progfile $$b.mepa --infile $$entrada > $$b.saida 2>/dev/null; \
			if cmp -s $$b.saida $$b.esperado; then echo "OK: $$f"; \
			else echo "FALHA (saida diferente): $$f"; diff $$b.saida $$b.esperado | head -5; falhas=$$((falhas+1)); fi; \
			rm -f $$b.saida; \
		else echo "OK (compilado): $$f"; fi; \
	done; \
	echo "===== Programas invalidos ====="; \
	for f in testes/invalidos/*.slac; do \
		b=$${f%.slac}; \
		echo antigo > $$b.mepa; \
		if ./complac $$f >/dev/null 2>$$b.err; then echo "FALHA (aceito): $$f"; falhas=$$((falhas+1)); \
		elif [ -f $$b.mepa ]; then echo "FALHA (.mepa nao removido): $$f"; falhas=$$((falhas+1)); \
		else echo "OK: $$f -> $$(head -1 $$b.err)"; fi; \
		rm -f $$b.err $$b.mepa; \
	done; \
	if [ $$falhas -ne 0 ]; then echo "===== $$falhas teste(s) falharam ====="; exit 1; fi; \
	echo "===== Todos os testes passaram ====="

clean:
	rm -f complac *.tk *.ts *.trc *.mepa
	rm -f testes/*/*.tk testes/*/*.ts testes/*/*.trc testes/*/*.mepa testes/*/*.saida testes/*/*.err

.PHONY: all test clean
