#ifndef SWL_DESKTOP_H
#define SWL_DESKTOP_H

#include <stdbool.h>
#include <wlr/types/wlr_scene.h>

enum swl_icon_kind {
	SWL_ICON_TERMINAL,
	SWL_ICON_EDITOR,
	SWL_ICON_FOLDER,
	SWL_ICON_BOOK,
	SWL_ICON_CONFIG,
	SWL_ICON_CHIP,
	SWL_ICON_NETWORK,
	SWL_ICON_MONITOR,
	SWL_ICON_MEDIA,
	SWL_ICON_IMAGE,
	SWL_ICON_GAME,
	SWL_ICON_ARCHIVE,
	SWL_ICON_TRASH,
};

struct swl_desktop;

/* `top_offset` é o Y onde a área de trabalho começa (normalmente
 * SWL_PANEL_HEIGHT, pra não desenhar ícones por baixo do painel). */
struct swl_desktop *swl_desktop_create(struct wlr_scene_tree *parent, int top_offset);
void swl_desktop_destroy(struct swl_desktop *desktop);

/* Retorna o comando associado ao ícone clicado, ou NULL se não acertou
 * nenhum. A string retornada é interna (não precisa dar free). */
const char *swl_desktop_hit_test(struct swl_desktop *desktop, double x, double y);

/* Igual swl_desktop_hit_test, mas devolve o ÍNDICE do ícone (não o
 * comando) — usado por swlwm.c pra decidir entre lançar (clique
 * simples), arrastar (segurar e mover) ou abrir o menu de contexto
 * (botão direito) no mesmo ícone. Ignora ícones escondidos
 * (swl_desktop_hide_icon). Retorna -1 se não acertou nenhum ícone
 * visível. */
int swl_desktop_hit_test_index(struct swl_desktop *desktop, double x, double y);

const char *swl_desktop_icon_label(struct swl_desktop *desktop, int index);
const char *swl_desktop_icon_command(struct swl_desktop *desktop, int index);

/* Move o ícone `index` pra nova posição de layout (canto superior
 * esquerdo da célula). Não faz clamping pros limites da tela — quem
 * chama decide se quer restringir. Sem efeito se index for inválido. */
void swl_desktop_move_icon(struct swl_desktop *desktop, int index, int x, int y);

/* Posição atual (canto superior esquerdo) do ícone `index`, usada por
 * quem inicia um arrasto pra calcular o deslocamento a partir daí.
 * Não escreve em out_x/out_y se index for inválido. */
void swl_desktop_icon_pos(struct swl_desktop *desktop, int index, int *out_x, int *out_y);

/* Esconde/restaura um ícone (usado pelo menu de contexto: "Remover do
 * desktop" / "Restaurar ícones removidos"). Ícone escondido não
 * aparece, não recebe clique nem hit-test — mas continua existindo
 * internamente, então "restaurar" traz de volta na mesma posição. */
void swl_desktop_hide_icon(struct swl_desktop *desktop, int index);
void swl_desktop_show_all_icons(struct swl_desktop *desktop);
bool swl_desktop_has_hidden_icons(struct swl_desktop *desktop);

/* Acessores do catálogo de apps padrão (default_icons[], em desktop.c) —
 * fonte única de verdade compartilhada pelos ícones da área de trabalho e
 * pelo menu iniciar. Qualquer app adicionado/removido ali aparece
 * automaticamente nos dois lugares. label/command retornam NULL para
 * índice fora do range; as strings são internas (não precisa dar free). */
int swl_desktop_app_count(void);
const char *swl_desktop_app_label(int index);
const char *swl_desktop_app_command(int index);

#endif /* SWL_DESKTOP_H */
