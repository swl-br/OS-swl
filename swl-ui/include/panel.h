#ifndef SWL_PANEL_H
#define SWL_PANEL_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>

#define SWL_PANEL_HISTORY_LEN 28

/*
 * Barra superior: logo/versão, menu (visual por enquanto), uso de CPU/RAM
 * lido de /proc (sem libs externas) e data/hora.
 */
struct swl_panel {
	struct wlr_scene_tree *tree;
	struct wlr_scene_buffer *buffer;
	struct wl_event_source *timer;
	int width;

	/* estado acumulado para calcular % de CPU por delta entre leituras */
	unsigned long long prev_idle;
	unsigned long long prev_total;

	/* histórico (0..1) pra desenhar o sparkline estilo equalizador —
	 * índice 0 é a amostra mais antiga, o último índice é a mais
	 * recente (desenhado da esquerda pra direita nessa ordem). */
	double cpu_history[SWL_PANEL_HISTORY_LEN];
	double mem_history[SWL_PANEL_HISTORY_LEN];

	/* opções alternáveis pelo popup "Opções do painel" (SISTEMA no
	 * menu) — só em memória, resetam a cada reinício do compositor
	 * (mesma limitação de tudo que não tem persistência ainda). */
	bool show_cpu;
	bool show_mem;
	bool clock_show_seconds;

	/* área clicável do item "SISTEMA" no menu do painel, recalculada a
	 * cada desenho — swlwm.c usa isso pro hit-test (abrir as opções). */
	int sistema_x, sistema_w;
};

struct swl_panel *swl_panel_create(struct wl_event_loop *loop,
	struct wlr_scene_tree *parent, int width);
void swl_panel_resize(struct swl_panel *panel, int width);
void swl_panel_destroy(struct swl_panel *panel);

/* true se (x,y) caiu em cima do item "SISTEMA" do menu do painel (em
 * coordenadas de layout — mesmo espaço de server->cursor->x/y). Já
 * checa a faixa Y do painel internamente, não precisa filtrar antes. */
bool swl_panel_hit_test_sistema(struct swl_panel *panel, double x, double y);

/* Força um redesenho imediato (usado depois de mudar show_cpu/show_mem/
 * clock_show_seconds pelo popup de opções — sem isso, só atualizaria no
 * próximo tick de 1s). */
void swl_panel_redraw_now(struct swl_panel *panel);

#endif /* SWL_PANEL_H */
