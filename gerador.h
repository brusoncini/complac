#ifndef GERADOR_H
#define GERADOR_H

/* Emissao formatada de instrucoes MEPA e gerenciamento de rotulos.
   As instrucoes sao acumuladas em um arquivo temporario e so gravadas
   no arquivo .mepa definitivo quando a compilacao termina sem erros. */

#define TAM_ROTULO 16

typedef char TRotulo[TAM_ROTULO];

int gerador_inicia(void);
void gera_instr_mepa(const char *rotulo, const char *mnemonico,
                     const char *parametro1, const char *parametro2);
void novo_rotulo(TRotulo rotulo);
int gerador_finaliza(const char *destino, int sucesso);

/* Atalhos usados pelo parser */
void gera0(const char *mnemonico);
void gera1(const char *mnemonico, int parametro);
void gera2(const char *mnemonico, int parametro1, int parametro2);
void gera_desvio(const char *mnemonico, const char *rotulo);
void gera_rotulo(const char *rotulo);

#endif
