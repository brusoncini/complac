CC=gcc
CFLAGS=-Wall -Wextra -std=c99

ARQUIVOS=main.c lex.c symtab.c

complac:
	$(CC) $(CFLAGS) $(ARQUIVOS) -o complac

clean:
	rm -f complac complac.exe