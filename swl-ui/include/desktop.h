#ifndef SWL_DESKTOP_H
#define SWL_DESKTOP_H

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

/* Acessores do catálogo de apps padrão (default_icons[], em desktop.c) —
 * fonte única de verdade compartilhada pelos ícones da área de trabalho e
 * pelo menu iniciar. Qualquer app adicionado/removido ali aparece
 * automaticamente nos dois lugares. label/command retornam NULL para
 * índice fora do range; as strings são internas (não precisa dar free). */
int swl_desktop_app_count(void);
const char *swl_desktop_app_label(int index);
const char *swl_desktop_app_command(int index);

#endif /* SWL_DESKTOP_H */
