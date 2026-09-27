CC=gcc
CFLAGS=-Wall -Wextra -std=c99
ARQUIVOS=main.c lex.c symtab.c parser.c semantic.c diag.c opts.c

complac:
	$(CC) $(CFLAGS) $(ARQUIVOS) -o complac

clean:
	rm -f complac *.tk *.ts *.trc
