CC=gcc
CFLAGS=-Wall -Wextra -std=c99
ARQUIVOS=main.c lex.c symtab.c parser.c diag.c opts.c

all: complac

complac: $(ARQUIVOS)
	$(CC) $(CFLAGS) $(ARQUIVOS) -o complac

clean:
	rm -f complac *.tk *.ts *.trc

.PHONY: all clean
