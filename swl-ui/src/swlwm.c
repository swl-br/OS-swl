#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include "theme.h"
#include "swl_buffer.h"
#include "swl_draw_util.h"
#include "panel.h"
#include "taskbar.h"
#include "decorations.h"
#include "desktop.h"
#include "background.h"
#include "menu.h"
#include "context_menu.h"
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/version.h>
#include <xkbcommon/xkbcommon.h>

/* Compat wlroots 0.17 ↔ 0.18: o código foi escrito e validado originalmente
 * contra 0.17.1 (Xubuntu). A 0.18 mudou quatro APIs usadas aqui:
 *   - wlr_backend_autocreate() recebe o event loop, não o display;
 *   - wlr_output_layout_create() passa a receber o display;
 *   - wlr_seat_pointer_notify_axis() ganhou o parâmetro relative_direction;
 *   - xdg_shell: o evento new_surface passa a disparar com role NONE —
 *     toplevel/popup agora têm eventos próprios (new_toplevel/new_popup).
 * Os trechos dependentes ficam atrás deste guard pra compilar nas duas
 * versões (as duas máquinas de desenvolvimento usam versões diferentes). */
#if WLR_VERSION_NUM >= ((0 << 16) | (18 << 8) | 0)
#define SWL_WLR_0_18 1
#else
#define SWL_WLR_0_18 0
#endif

/* For brevity's sake, struct members are annotated where they are used. */
enum tinywl_cursor_mode {
	TINYWL_CURSOR_PASSTHROUGH,
	TINYWL_CURSOR_MOVE,
	TINYWL_CURSOR_RESIZE,
};

struct tinywl_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;

	struct wlr_xdg_shell *xdg_shell;
#if SWL_WLR_0_18
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;
#else
	struct wl_listener new_xdg_surface;
#endif
	struct wl_list toplevels;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;
	struct wl_list keyboards;
	enum tinywl_cursor_mode cursor_mode;
	struct tinywl_toplevel *grabbed_toplevel;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener new_output;

	/* Elementos de shell do SWL OS. Criados/redimensionados quando o
	 * primeiro output conecta (ou muda de modo). */
	struct swl_panel *panel;
	struct swl_taskbar *taskbar;
	struct swl_desktop *desktop;
	struct swl_menu *menu;
	struct swl_context_menu *ctx_menu;
	int ctx_menu_target; /* índice do ícone que o menu de contexto foi
	                      * aberto pra, ou -1 se foi aberto em área vazia
	                      * (nesse caso os itens são outros, ver uso). */

	/* Arrastar ícone da área de trabalho — pressionar não move de cara
	 * (pode ser só um clique); só vira "arrasto de verdade" depois de
	 * passar de um limiar de movimento, decidido em process_cursor_motion.
	 * Sem estado próprio no enum tinywl_cursor_mode de propósito: mover
	 * janela e mover ícone são coisas independentes, então ficam em
	 * variáveis separadas em vez de forçar tudo no mesmo enum. */
	bool desktop_drag_pending;
	bool desktop_drag_active;
	int desktop_drag_icon;
	double desktop_drag_press_x, desktop_drag_press_y;
	int desktop_drag_icon_orig_x, desktop_drag_icon_orig_y;

	/* Throttle do resize interativo — ver process_cursor_resize()/
	 * output_frame(). Posição, tamanho pedido ao cliente e redesenho da
	 * decoração são aplicados JUNTOS, no máximo uma vez por frame, nunca
	 * separadamente. A primeira versão deste fix throttava só a
	 * decoração e deixava a posição instantânea — isso criava um
	 * descompasso visível entre os dois (ao arrastar a borda esquerda/de
	 * cima, um lado da janela "atrasava" em relação ao outro, porque
	 * posição e largura mudavam em momentos diferentes). Motion events
	 * podem disparar muito mais rápido que a taxa real de frame; sem
	 * throttle nenhum, swl_decoration_resize() (realoca buffer + redesenha
	 * tudo com Cairo/Pango) enfileira trabalho mais rápido do que
	 * consegue processar. */
	bool resize_pending;
	int resize_pending_scene_x, resize_pending_scene_y;
	int resize_pending_width, resize_pending_height;

	/* Arrastar janela até encostar no painel = maximizar (soltar aplica,
	 * ver process_cursor_move() e o handler de WLR_BUTTON_RELEASED). */
	bool move_snap_maximize;

	struct wlr_scene_buffer *background;
	int screen_width, screen_height;
	const char *background_path; /* NULL = usa o fallback procedural (grade) */
};

struct tinywl_output {
	struct wl_list link;
	struct tinywl_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
};

struct tinywl_toplevel {
	struct wl_list link;
	struct tinywl_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;

	/* scene_tree é o nó "wrapper": mover/levantar este nó move a janela
	 * inteira (decoração + conteúdo) de uma vez. content_tree é a subárvore
	 * da superfície xdg propriamente dita, deslocada SWL_TITLEBAR_HEIGHT
	 * pixels abaixo do topo do wrapper. */
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_tree *content_tree;
	struct swl_decoration *decoration;
	char *deco_title;
	int deco_width;

	/* Estado de maximizar/minimizar/fullscreen. saved_geo guarda posição
	 * (wrapper) + tamanho (conteúdo) de antes de maximizar ou fullscreen,
	 * pra restaurar. Válido quando maximized || fullscreen. */
	bool maximized;
	bool minimized;
	bool fullscreen;
	struct wlr_box saved_geo;
	bool initial_configure_sent; /* wlroots 0.18: ver xdg_toplevel_commit */

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
};

struct tinywl_keyboard {
	struct wl_list link;
	struct tinywl_server *server;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

/* Reconstrói a lista de janelas mostrada na taskbar a partir de
 * server->toplevels. O índice focado é derivado do keyboard focus do
 * seat (não mais "sempre 0"): minimizar/fechar a focada deixa o
 * highlight correto (R-05). */
static void update_taskbar(struct tinywl_server *server) {
	if (!server->taskbar) {
		return;
	}
	const char *titles[SWL_TASKBAR_MAX_WINDOWS];
	int count = 0;
	int focused_index = -1;
	struct wlr_surface *focused_surf =
		server->seat ? server->seat->keyboard_state.focused_surface : NULL;
	struct tinywl_toplevel *t;
	wl_list_for_each(t, &server->toplevels, link) {
		if (count >= SWL_TASKBAR_MAX_WINDOWS) {
			break;
		}
		titles[count] = t->xdg_toplevel->title ? t->xdg_toplevel->title : "janela";
		if (focused_surf && !t->minimized &&
				t->xdg_toplevel->base->surface == focused_surf) {
			focused_index = count;
		}
		count++;
	}
	swl_taskbar_set_windows(server->taskbar, titles, count, focused_index);
}

static void focus_toplevel(struct tinywl_toplevel *toplevel, struct wlr_surface *surface) {
	/* Note: this function only deals with keyboard focus. */
	if (toplevel == NULL) {
		return;
	}
	struct tinywl_server *server = toplevel->server;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface) {
		/* Don't re-focus an already focused surface. */
		return;
	}
	if (prev_surface) {
		/*
		 * Deactivate the previously focused surface. This lets the client know
		 * it no longer has focus and the client will repaint accordingly, e.g.
		 * stop displaying a caret.
		 */
		struct wlr_xdg_toplevel *prev_toplevel =
			wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
		if (prev_toplevel != NULL) {
			wlr_xdg_toplevel_set_activated(prev_toplevel, false);
		}
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	/* Move the toplevel to the front */
	wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
	wl_list_remove(&toplevel->link);
	wl_list_insert(&server->toplevels, &toplevel->link);
	/* Activate the new surface */
	wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);

	/* Reflete o foco visual nas decorações (barra de título ativa/inativa)
	 * e mantém painel/taskbar sempre por cima de qualquer janela. */
	if (toplevel->decoration) {
		swl_decoration_set_focused(toplevel->decoration, true);
	}
	if (prev_surface) {
		struct wlr_xdg_toplevel *prev_xdg_toplevel =
			wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
		if (prev_xdg_toplevel != NULL) {
			struct tinywl_toplevel *iter;
			wl_list_for_each(iter, &server->toplevels, link) {
				if (iter->xdg_toplevel == prev_xdg_toplevel && iter->decoration) {
					swl_decoration_set_focused(iter->decoration, false);
					break;
				}
			}
		}
	}
	if (server->panel) {
		wlr_scene_node_raise_to_top(&server->panel->tree->node);
	}
	if (server->taskbar) {
		wlr_scene_node_raise_to_top(&server->taskbar->tree->node);
	}
	update_taskbar(server);
	/*
	 * Tell the seat to have the keyboard enter this surface. wlroots will keep
	 * track of this and automatically send key events to the appropriate
	 * clients without additional work on your part.
	 */
	if (keyboard != NULL) {
		wlr_seat_keyboard_notify_enter(seat, toplevel->xdg_toplevel->base->surface,
			keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
	}
}

/* Aplica layout maximizado ao tamanho atual do output. Não mexe em
 * saved_geo. Usado no maximize e no reflow quando o output muda (A5). */
static void toplevel_apply_maximized_layout(struct tinywl_toplevel *toplevel) {
	struct tinywl_server *server = toplevel->server;
	int avail_w = server->screen_width;
	int avail_h = server->screen_height - SWL_PANEL_HEIGHT -
		SWL_TASKBAR_HEIGHT - SWL_TITLEBAR_HEIGHT;
	if (avail_w < 1) {
		avail_w = 1;
	}
	if (avail_h < 1) {
		avail_h = 1;
	}
	wlr_scene_node_set_position(&toplevel->scene_tree->node, 0, SWL_PANEL_HEIGHT);
	wlr_scene_node_set_position(&toplevel->content_tree->node, 0, SWL_TITLEBAR_HEIGHT);
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, avail_w, avail_h);
	wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, true);
	if (toplevel->decoration) {
		wlr_scene_node_set_enabled(&toplevel->decoration->tree->node, true);
		swl_decoration_resize(toplevel->decoration, avail_w);
	}
}

/* Fullscreen real (A6): cobre o output inteiro, sem titlebar. */
static void toplevel_apply_fullscreen_layout(struct tinywl_toplevel *toplevel) {
	struct tinywl_server *server = toplevel->server;
	int w = server->screen_width;
	int h = server->screen_height;
	if (w < 1) {
		w = 1;
	}
	if (h < 1) {
		h = 1;
	}
	wlr_scene_node_set_position(&toplevel->scene_tree->node, 0, 0);
	/* Conteúdo colado no topo do wrapper — sem espaço pra titlebar. */
	wlr_scene_node_set_position(&toplevel->content_tree->node, 0, 0);
	if (toplevel->decoration) {
		wlr_scene_node_set_enabled(&toplevel->decoration->tree->node, false);
	}
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, w, h);
	wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, true);
}

static void toplevel_save_geometry(struct tinywl_toplevel *toplevel) {
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo);
	toplevel->saved_geo.x = toplevel->scene_tree->node.x;
	toplevel->saved_geo.y = toplevel->scene_tree->node.y;
	toplevel->saved_geo.width = geo.width;
	toplevel->saved_geo.height = geo.height;
}

static void toplevel_restore_geometry(struct tinywl_toplevel *toplevel) {
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
		toplevel->saved_geo.x, toplevel->saved_geo.y);
	wlr_scene_node_set_position(&toplevel->content_tree->node, 0, SWL_TITLEBAR_HEIGHT);
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel,
		toplevel->saved_geo.width, toplevel->saved_geo.height);
	if (toplevel->decoration) {
		wlr_scene_node_set_enabled(&toplevel->decoration->tree->node, true);
		if (toplevel->saved_geo.width > 0) {
			swl_decoration_resize(toplevel->decoration, toplevel->saved_geo.width);
		}
	}
}

/* Maximiza (ou restaura) o toplevel. Idempotente. A área disponível é a
 * tela menos painel e taskbar; a titlebar SWL continua visível. */
static void toplevel_set_maximized(struct tinywl_toplevel *toplevel, bool maximize) {
	if (maximize == toplevel->maximized) {
		return;
	}
	/* Fullscreen e maximizado são mutuamente exclusivos na UI. */
	bool left_fullscreen = false;
	if (maximize && toplevel->fullscreen) {
		toplevel->fullscreen = false;
		wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, false);
		left_fullscreen = true;
	}

	if (maximize) {
		if (!toplevel->fullscreen) {
			toplevel_save_geometry(toplevel);
		}
		toplevel_apply_maximized_layout(toplevel);
	} else {
		wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, false);
		toplevel_restore_geometry(toplevel);
	}
	toplevel->maximized = maximize;

	/* Aviso A6 item 2: ao sair de fullscreen via maximize, re-raise do
	 * shell (painel/taskbar) — senão só volta no próximo focus. */
	if (left_fullscreen) {
		struct tinywl_server *server = toplevel->server;
		if (server->panel) {
			wlr_scene_node_raise_to_top(&server->panel->tree->node);
		}
		if (server->taskbar) {
			wlr_scene_node_raise_to_top(&server->taskbar->tree->node);
		}
	}
}

/* Fullscreen real (A6). Cobre o output inteiro; titlebar some. Ao sair,
 * se estava maximizado restaura maximizado; senão restored_geo. */
static void toplevel_set_fullscreen(struct tinywl_toplevel *toplevel, bool fs) {
	if (fs == toplevel->fullscreen) {
		return;
	}

	if (fs) {
		if (!toplevel->maximized) {
			toplevel_save_geometry(toplevel);
		}
		/* Sai do modo maximizado "lógico" na geometria, mas lembra a flag
		 * pra voltar maximizado ao sair do fullscreen. */
		if (toplevel->maximized) {
			wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, false);
		}
		toplevel_apply_fullscreen_layout(toplevel);
		/* Por cima de painel/taskbar/desktop — fullscreen cobre tudo. */
		wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
		toplevel->fullscreen = true;
	} else {
		wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, false);
		toplevel->fullscreen = false;
		if (toplevel->maximized) {
			toplevel_apply_maximized_layout(toplevel);
		} else {
			toplevel_restore_geometry(toplevel);
		}
		/* Shell de volta por cima das janelas (mesmo que focus_toplevel). */
		struct tinywl_server *server = toplevel->server;
		if (server->panel) {
			wlr_scene_node_raise_to_top(&server->panel->tree->node);
		}
		if (server->taskbar) {
			wlr_scene_node_raise_to_top(&server->taskbar->tree->node);
		}
	}
}

/* Minimiza (esconde, mas mantém na taskbar) ou restaura o toplevel.
 * Minimizar não destrói nada: só desabilita o nó da scene (o que já cobre
 * decoração + conteúdo, por estarem dentro do mesmo scene_tree wrapper) e
 * tira o foco de teclado se estava focado. Restaurar reabilita e chama
 * focus_toplevel() de novo, igual clicar na janela normalmente. */
static void toplevel_set_minimized(struct tinywl_toplevel *toplevel, bool minimize) {
	if (minimize == toplevel->minimized) {
		return;
	}
	toplevel->minimized = minimize;
	wlr_scene_node_set_enabled(&toplevel->scene_tree->node, !minimize);

	if (minimize) {
		struct tinywl_server *server = toplevel->server;
		bool was_focused = (server->seat->keyboard_state.focused_surface ==
				toplevel->xdg_toplevel->base->surface);
		if (was_focused) {
			wlr_seat_keyboard_clear_focus(server->seat);
		}
		wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, false);
		if (toplevel->decoration) {
			swl_decoration_set_focused(toplevel->decoration, false);
		}
		/* Se a janela minimizada era a focada, passa o foco para a
		 * próxima não-minimizada (cabeça da lista após raise). Sem
		 * isso a taskbar continua destacando a invisível (R-05). */
		if (was_focused) {
			struct tinywl_toplevel *cand;
			wl_list_for_each(cand, &server->toplevels, link) {
				if (cand != toplevel && !cand->minimized) {
					focus_toplevel(cand, cand->xdg_toplevel->base->surface);
					return;
				}
			}
		}
		update_taskbar(server);
	} else {
		focus_toplevel(toplevel, toplevel->xdg_toplevel->base->surface);
	}
}

static void keyboard_handle_modifiers(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	struct tinywl_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	/*
	 * A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same seat. You can swap out the underlying wlr_keyboard like this and
	 * wlr_seat handles this transparently.
	 */
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
		&keyboard->wlr_keyboard->modifiers);
}


static struct tinywl_toplevel *focused_toplevel(struct tinywl_server *server) {
	struct wlr_surface *focused_surf =
		server->seat ? server->seat->keyboard_state.focused_surface : NULL;
	if (!focused_surf) {
		return NULL;
	}
	struct tinywl_toplevel *t;
	wl_list_for_each(t, &server->toplevels, link) {
		if (!t->minimized && t->xdg_toplevel->base->surface == focused_surf) {
			return t;
		}
	}
	return NULL;
}


/* Meia-tela esquerda (side=0) ou direita (side=1). */
static void toplevel_snap_half(struct tinywl_toplevel *toplevel, int side) {
	struct tinywl_server *server = toplevel->server;
	int avail_w = server->screen_width;
	int avail_h = server->screen_height - SWL_PANEL_HEIGHT -
		SWL_TASKBAR_HEIGHT - SWL_TITLEBAR_HEIGHT;
	if (avail_w < 80 || avail_h < 40) {
		return;
	}
	int half = avail_w / 2;
	int x = (side == 0) ? 0 : half;
	int w = (side == 0) ? half : (avail_w - half);

	if (toplevel->fullscreen) {
		toplevel_set_fullscreen(toplevel, false);
	}
	if (!toplevel->maximized) {
		toplevel_save_geometry(toplevel);
	} else {
		wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, false);
		toplevel->maximized = false;
	}

	wlr_scene_node_set_position(&toplevel->scene_tree->node, x, SWL_PANEL_HEIGHT);
	wlr_scene_node_set_position(&toplevel->content_tree->node, 0, SWL_TITLEBAR_HEIGHT);
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, w, avail_h);
	if (toplevel->decoration) {
		wlr_scene_node_set_enabled(&toplevel->decoration->tree->node, true);
		swl_decoration_resize(toplevel->decoration, w);
	}
}

static void cycle_toplevel(struct tinywl_server *server, bool reverse) {
	if (wl_list_length(&server->toplevels) < 2) {
		return;
	}
	/* Lista: focada na cabeça. Frente = prev (fim); trás = next. */
	struct tinywl_toplevel *next_toplevel;
	if (reverse) {
		next_toplevel = wl_container_of(server->toplevels.next->next,
			next_toplevel, link);
		/* se só 2, next->next é a cabeça de novo — pega next */
		if (&next_toplevel->link == &server->toplevels) {
			next_toplevel = wl_container_of(server->toplevels.next,
				next_toplevel, link);
		}
	} else {
		next_toplevel = wl_container_of(server->toplevels.prev,
			next_toplevel, link);
	}
	/* Pula minimizadas */
	struct tinywl_toplevel *start = next_toplevel;
	for (;;) {
		if (!next_toplevel->minimized) {
			focus_toplevel(next_toplevel,
				next_toplevel->xdg_toplevel->base->surface);
			return;
		}
		if (reverse) {
			next_toplevel = wl_container_of(next_toplevel->link.next,
				next_toplevel, link);
			if (&next_toplevel->link == &server->toplevels) {
				next_toplevel = wl_container_of(server->toplevels.next,
					next_toplevel, link);
			}
		} else {
			next_toplevel = wl_container_of(next_toplevel->link.prev,
				next_toplevel, link);
			if (&next_toplevel->link == &server->toplevels) {
				next_toplevel = wl_container_of(server->toplevels.prev,
					next_toplevel, link);
			}
		}
		if (next_toplevel == start) {
			return;
		}
	}
}

static bool handle_keybinding(struct tinywl_server *server, xkb_keysym_t sym,
		uint32_t modifiers) {
	/*
	 * Keybindings do compositor. Assume Alt pressionado.
	 */
	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(server->wl_display);
		break;
	case XKB_KEY_F1:
	case XKB_KEY_Tab:
		/* Alt+Tab / Alt+F1: cicla janelas; Shift reverte. */
		cycle_toplevel(server, modifiers & WLR_MODIFIER_SHIFT);
		break;
	case XKB_KEY_ISO_Left_Tab:
		/* alguns layouts emitem isto com Shift+Tab */
		cycle_toplevel(server, true);
		break;
	case XKB_KEY_F4: {
		/* Alt+F4 — pede ao cliente fechar (mesmo da decoracao) */
		struct tinywl_toplevel *t = focused_toplevel(server);
		if (t) {
			wlr_xdg_toplevel_send_close(t->xdg_toplevel);
		}
		break;
	}
	case XKB_KEY_F9: {
		/* Alt+F9 — minimizar */
		struct tinywl_toplevel *t = focused_toplevel(server);
		if (t) {
			toplevel_set_minimized(t, true);
		}
		break;
	}
	case XKB_KEY_F10: {
		/* Alt+F10 — maximizar/restaurar */
		struct tinywl_toplevel *t = focused_toplevel(server);
		if (t) {
			toplevel_set_maximized(t, !t->maximized);
		}
		break;
	}
	default:
		return false;
	}
	return true;
}

static void keyboard_handle_key(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a key is pressed or released. */
	struct tinywl_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct tinywl_server *server = keyboard->server;
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *seat = server->seat;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		if (modifiers & WLR_MODIFIER_ALT) {
			/* Alt+… — atalhos do compositor */
			for (int i = 0; i < nsyms; i++) {
				handled = handle_keybinding(server, syms[i], modifiers);
			}
		}
		/* Super+setas: meia-tela / maximizar / restaurar|minimizar */
		if (!handled && (modifiers & WLR_MODIFIER_LOGO)) {
			struct tinywl_toplevel *ft = focused_toplevel(server);
			for (int i = 0; i < nsyms && ft; i++) {
				switch (syms[i]) {
				case XKB_KEY_Left:
					toplevel_snap_half(ft, 0);
					handled = true;
					break;
				case XKB_KEY_Right:
					toplevel_snap_half(ft, 1);
					handled = true;
					break;
				case XKB_KEY_Up:
					toplevel_set_maximized(ft, true);
					handled = true;
					break;
				case XKB_KEY_Down:
					if (ft->maximized) {
						toplevel_set_maximized(ft, false);
					} else {
						toplevel_set_minimized(ft, true);
					}
					handled = true;
					break;
				default:
					break;
				}
			}
		}
	}

	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
	/* This event is raised by the keyboard base wlr_input_device to signal
	 * the destruction of the wlr_keyboard. It will no longer receive events
	 * and should be destroyed.
	 */
	struct tinywl_keyboard *keyboard =
		wl_container_of(listener, keyboard, destroy);
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->link);
	free(keyboard);
}

static void server_new_keyboard(struct tinywl_server *server,
		struct wlr_input_device *device) {
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

	struct tinywl_keyboard *keyboard = calloc(1, sizeof(*keyboard));
	keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

	/* We need to prepare an XKB keymap and assign it to the keyboard. This
	 * assumes the defaults (e.g. layout = "us"). */
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	/* Here we set up listeners for keyboard events. */
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);

	/* And add the keyboard to our list of keyboards */
	wl_list_insert(&server->keyboards, &keyboard->link);
}

static void server_new_pointer(struct tinywl_server *server,
		struct wlr_input_device *device) {
	/* We don't do anything special with pointers. All of our pointer handling
	 * is proxied through wlr_cursor. On another compositor, you might take this
	 * opportunity to do libinput configuration on the device to set
	 * acceleration, etc. */
	wlr_cursor_attach_input_device(server->cursor, device);
}

static void server_new_input(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct tinywl_server *server =
		wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		server_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		server_new_pointer(server, device);
		break;
	default:
		break;
	}
	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In TinyWL we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability. */
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

static void seat_request_cursor(struct wl_listener *listener, void *data) {
	struct tinywl_server *server = wl_container_of(
			listener, server, request_cursor);
	/* This event is raised by the seat when a client provides a cursor image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		server->seat->pointer_state.focused_client;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. */
	if (focused_client == event->seat_client) {
		/* Once we've vetted the client, we can tell the cursor to use the
		 * provided surface as the cursor image. It will set the hardware cursor
		 * on the output that it's currently on and continue to do so as the
		 * cursor moves between outputs. */
		wlr_cursor_set_surface(server->cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
	}
}

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the selection,
	 * usually when the user copies something. wlroots allows compositors to
	 * ignore such requests if they so choose, but in tinywl we always honor
	 */
	struct tinywl_server *server = wl_container_of(
			listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static struct tinywl_toplevel *desktop_toplevel_at(
		struct tinywl_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	/* This returns the topmost node in the scene at the given layout coords.
	 * We only care about surface nodes as we are specifically looking for a
	 * surface in the surface tree of a tinywl_toplevel. */
	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;
	/* Find the node corresponding to the tinywl_toplevel at the root of this
	 * surface tree, it is the only one for which we set the data field. */
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	/* R-13: se o buffer não está sob um toplevel (painel, desktop,
	 * menu…), tree pode ser NULL — não ler tree->node.data. */
	if (tree == NULL) {
		return NULL;
	}
	return tree->node.data;
}

/* Catálogo: PATH mínimo ao lançar apps do desktop/menu (aviso tswl-catalog). */
static void swl_launch_command(const char *cmd) {
	if (!cmd || !cmd[0]) {
		return;
	}
	pid_t pid = fork();
	if (pid == 0) {
		setenv("PATH", "/bin:/usr/bin:/sbin", 1);
		execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)NULL);
		_exit(1);
	}
}

static void reset_cursor_mode(struct tinywl_server *server) {
	/* Reset the cursor mode to passthrough. */
	server->cursor_mode = TINYWL_CURSOR_PASSTHROUGH;
	server->grabbed_toplevel = NULL;
}

/* Margem mínima de janela que sempre fica visível/agarrável na tela
 * durante um arrasto — sem isso, dava pra arrastar uma janela pra fora
 * da tela por completo e não ter como trazer de volta (não tem atalho
 * de teclado nem "organizar janelas" ainda). */
#define SWL_DRAG_VISIBLE_MARGIN 60

static void process_cursor_move(struct tinywl_server *server, uint32_t time) {
	/* Move the grabbed toplevel to the new position. */
	struct tinywl_toplevel *toplevel = server->grabbed_toplevel;
	if (toplevel->maximized || toplevel->fullscreen) {
		/* Não faz sentido arrastar a posição de uma janela maximizada ou
		 * em fullscreen — as duas ocupam a área inteira por definição. */
		return;
	}

	double new_x = server->cursor->x - server->grab_x;
	double new_y = server->cursor->y - server->grab_y;

	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo);
	int width = geo.width > 0 ? geo.width : 200;

	/* Nunca deixa a janela sair da tela por completo — sempre sobra pelo
	 * menos SWL_DRAG_VISIBLE_MARGIN de barra de título visível pra
	 * conseguir arrastar de volta depois. */
	if (new_x + width < SWL_DRAG_VISIBLE_MARGIN) {
		new_x = SWL_DRAG_VISIBLE_MARGIN - width;
	}
	if (new_x > server->screen_width - SWL_DRAG_VISIBLE_MARGIN) {
		new_x = server->screen_width - SWL_DRAG_VISIBLE_MARGIN;
	}
	/* Nunca deixa a barra de título atravessar o painel por cima — pode
	 * encostar nele, não passar por trás. */
	if (new_y < SWL_PANEL_HEIGHT) {
		new_y = SWL_PANEL_HEIGHT;
	}
	/* Não deixa sumir embaixo da taskbar por completo. */
	if (new_y > server->screen_height - SWL_TASKBAR_HEIGHT - SWL_DRAG_VISIBLE_MARGIN) {
		new_y = server->screen_height - SWL_TASKBAR_HEIGHT - SWL_DRAG_VISIBLE_MARGIN;
	}

	wlr_scene_node_set_position(&toplevel->scene_tree->node, new_x, new_y);

	/* "Encostou" no painel — arma o snap-pra-maximizar (estilo "Aero
	 * Snap"), aplicado só no release (ver WLR_BUTTON_RELEASED). Não
	 * aplica aqui: soltar o grab no meio do arrasto ficaria estranho. */
	server->move_snap_maximize = (new_y <= SWL_PANEL_HEIGHT);
}

static void process_cursor_resize(struct tinywl_server *server, uint32_t time) {
	/*
	 * Resizing the grabbed toplevel can be a little bit complicated, because we
	 * could be resizing from any corner or edge. This not only resizes the
	 * toplevel on one or two axes, but can also move the toplevel if you resize
	 * from the top or left edges (or top-left corner).
	 *
	 * Note that some shortcuts are taken here. In a more fleshed-out
	 * compositor, you'd wait for the client to prepare a buffer at the new
	 * size, then commit any movement that was prepared.
	 */
	struct tinywl_toplevel *toplevel = server->grabbed_toplevel;
	double border_x = server->cursor->x - server->grab_x;
	double border_y = server->cursor->y - server->grab_y;
	int new_left = server->grab_geobox.x;
	int new_right = server->grab_geobox.x + server->grab_geobox.width;
	int new_top = server->grab_geobox.y;
	int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom) {
			new_top = new_bottom - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top) {
			new_bottom = new_top + 1;
		}
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right) {
			new_left = new_right - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left) {
			new_right = new_left + 1;
		}
	}

	/* Tamanho mínimo de verdade — antes disso, o único limite era "não
	 * inverter as bordas" (mínimo de 1px), o que deixa a janela encolher
	 * até sumir visualmente. Aplica o mínimo a partir da borda que está
	 * sendo arrastada, pra não fazer a janela "pular" de posição quando
	 * bate no limite (se está arrastando a borda esquerda, é o lado
	 * esquerdo que para; a borda direita nunca se mexe nesse caso). */
	if (new_right - new_left < SWL_MIN_WINDOW_WIDTH) {
		if (server->resize_edges & WLR_EDGE_LEFT) {
			new_left = new_right - SWL_MIN_WINDOW_WIDTH;
		} else {
			new_right = new_left + SWL_MIN_WINDOW_WIDTH;
		}
	}
	if (new_bottom - new_top < SWL_MIN_WINDOW_HEIGHT) {
		if (server->resize_edges & WLR_EDGE_TOP) {
			new_top = new_bottom - SWL_MIN_WINDOW_HEIGHT;
		} else {
			new_bottom = new_top + SWL_MIN_WINDOW_HEIGHT;
		}
	}

	struct wlr_box geo_box;
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo_box);

	/* Posição, tamanho pedido ao cliente e redesenho da decoração NÃO são
	 * aplicados aqui — só guardados. Os três precisam mudar juntos, no
	 * mesmo instante, ou um fica visualmente "atrasado" em relação ao
	 * outro. Tudo isso é aplicado de uma vez só, no máximo uma vez por
	 * frame, em output_frame() (ou no release, se soltar entre um frame e
	 * outro). Ver campo resize_pending no struct do servidor. */
	server->resize_pending = true;
	server->resize_pending_scene_x = new_left - geo_box.x;
	server->resize_pending_scene_y = new_top - geo_box.y - SWL_TITLEBAR_HEIGHT;
	server->resize_pending_width = new_right - new_left;
	server->resize_pending_height = new_bottom - new_top;
}

static void process_cursor_motion(struct tinywl_server *server, uint32_t time) {
	/* Arrasto de ícone da área de trabalho tem prioridade sobre tudo mais
	 * neste função enquanto estiver pendente/ativo — nem passa pro
	 * dispatch de MOVE/RESIZE de janela nem pro passthrough normal.
	 * Fica fora do enum tinywl_cursor_mode de propósito (ver campo no
	 * struct do servidor). */
	if (server->desktop_drag_pending) {
		double dx = server->cursor->x - server->desktop_drag_press_x;
		double dy = server->cursor->y - server->desktop_drag_press_y;
		if (!server->desktop_drag_active) {
			/* Só vira arrasto de verdade depois de um pequeno limiar —
			 * abaixo disso, ainda pode ser só um clique (decidido no
			 * WLR_BUTTON_RELEASED). Sem isso, TODO clique morreria virando
			 * um "arrasto de 0px" e nunca lançaria o app. */
			if (fabs(dx) < 4 && fabs(dy) < 4) {
				return;
			}
			server->desktop_drag_active = true;
		}
		int nx = server->desktop_drag_icon_orig_x + (int)dx;
		int ny = server->desktop_drag_icon_orig_y + (int)dy;
		/* Clamp simples: não deixa o ícone sumir embaixo do painel/
		 * taskbar nem sair da tela por completo. */
		if (nx < 0) {
			nx = 0;
		}
		if (ny < SWL_PANEL_HEIGHT) {
			ny = SWL_PANEL_HEIGHT;
		}
		if (nx > server->screen_width - 24) {
			nx = server->screen_width - 24;
		}
		if (ny > server->screen_height - SWL_TASKBAR_HEIGHT - 24) {
			ny = server->screen_height - SWL_TASKBAR_HEIGHT - 24;
		}
		swl_desktop_move_icon(server->desktop, server->desktop_drag_icon, nx, ny);
		return;
	}

	/* If the mode is non-passthrough, delegate to those functions. */
	if (server->cursor_mode == TINYWL_CURSOR_MOVE) {
		process_cursor_move(server, time);
		return;
	} else if (server->cursor_mode == TINYWL_CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	/* Otherwise, find the toplevel under the pointer and send the event along. */
	double sx, sy;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = NULL;
	struct tinywl_toplevel *toplevel = desktop_toplevel_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);
	if (!toplevel) {
		/* If there's no toplevel under the cursor, set the cursor image to a
		 * default. This is what makes the cursor image appear when you move it
		 * around the screen, not over any toplevels. */
		wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
	} else if (!toplevel->maximized && !toplevel->fullscreen) {
		/* Resize visual: cursor sobre o anel de borda da janela vira a
		 * setinha de resize correspondente (mesmo hit-test do clique,
		 * SWL_RESIZE_MARGIN). Maximizada/fullscreen não mostram (borda
		 * fora da tela / tela cheia). */
		struct wlr_box geo2;
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo2);
		double wx2 = server->cursor->x - toplevel->scene_tree->node.x;
		double wy2 = server->cursor->y - toplevel->scene_tree->node.y;
		double ww2 = geo2.x + geo2.width;
		double wh2 = SWL_TITLEBAR_HEIGHT + geo2.y + geo2.height;
		bool l2 = wx2 >= 0 && wx2 < SWL_RESIZE_MARGIN;
		bool r2 = wx2 >= ww2 - SWL_RESIZE_MARGIN && wx2 < ww2;
		bool t2 = wy2 >= 0 && wy2 < SWL_RESIZE_MARGIN;
		bool b2 = wy2 >= wh2 - SWL_RESIZE_MARGIN && wy2 < wh2;
		const char *rname = NULL;
		if (t2 && l2) {
			rname = "top_left_corner";
		} else if (t2 && r2) {
			rname = "top_right_corner";
		} else if (b2 && l2) {
			rname = "bottom_left_corner";
		} else if (b2 && r2) {
			rname = "bottom_right_corner";
		} else if (l2) {
			rname = "left_side";
		} else if (r2) {
			rname = "right_side";
		} else if (t2) {
			rname = "top_side";
		} else if (b2) {
			rname = "bottom_side";
		}
		if (rname) {
			wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, rname);
		} else {
			/* Dentro da janela mas fora do anel: volta pra seta padrão.
			 * Sem isso, a setinha de resize "gruda" — ela foi setada na
			 * borda e nada a trocaria ao entrar na janela (o cliente
			 * tswl/swlpad não define cursor próprio). */
			wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
		}
	}
	if (surface) {
		/*
		 * Send pointer enter and motion events.
		 *
		 * The enter event gives the surface "pointer focus", which is distinct
		 * from keyboard focus. You get pointer focus by moving the pointer over
		 * a window.
		 *
		 * Note that wlroots will avoid sending duplicate enter/motion events if
		 * the surface has already has pointer focus or if the client is already
		 * aware of the coordinates passed.
		 */
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	} else {
		/* Clear pointer focus so future button events and such are not sent to
		 * the last client to have the cursor over it. */
		wlr_seat_pointer_clear_focus(seat);
	}
}

static void server_cursor_motion(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a _relative_
	 * pointer motion event (i.e. a delta) */
	struct tinywl_server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;
	/* The cursor doesn't move unless we tell it to. The cursor automatically
	 * handles constraining the motion to the output layout, as well as any
	 * special configuration applied for the specific input device which
	 * generated the event. You can pass NULL for the device if you want to move
	 * the cursor around without any input. */
	wlr_cursor_move(server->cursor, &event->pointer->base,
			event->delta_x, event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

static void server_cursor_motion_absolute(
		struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an _absolute_
	 * motion event, from 0..1 on each axis. This happens, for example, when
	 * wlroots is running under a Wayland window rather than KMS+DRM, and you
	 * move the mouse over the window. You could enter the window from any edge,
	 * so we have to warp the mouse there. There is also some hardware which
	 * emits these events. */
	struct tinywl_server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x,
		event->y);
	process_cursor_motion(server, event->time_msec);
}

static void begin_interactive(struct tinywl_toplevel *toplevel,
	enum tinywl_cursor_mode mode, uint32_t edges);
static void server_cursor_button(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a button
	 * event. */
	struct tinywl_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;
	/* Notify the client with pointer focus that a button press has occurred */
	wlr_seat_pointer_notify_button(server->seat,
			event->time_msec, event->button, event->state);
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct tinywl_toplevel *toplevel = desktop_toplevel_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);
	if (event->state == WLR_BUTTON_RELEASED) {
		/* Resolve o arrasto de ícone pendente: se o cursor nunca passou do
		 * limiar de movimento (desktop_drag_active continua false), foi só
		 * um clique — lança o app agora (não no press, pra dar chance do
		 * motion virar arrasto antes de decidir). Se passou do limiar, o
		 * ícone já foi reposicionado ao vivo em process_cursor_motion; só
		 * falta soltar o estado. */
		if (server->desktop_drag_pending) {
			bool was_click = !server->desktop_drag_active;
			int icon = server->desktop_drag_icon;
			server->desktop_drag_pending = false;
			server->desktop_drag_active = false;
			if (was_click && server->desktop) {
				const char *cmd = swl_desktop_icon_command(server->desktop, icon);
				if (cmd && cmd[0]) {
					swl_launch_command(cmd);
				}
			} else if (server->desktop) {
				/* Foi um arrasto de verdade (não clique) — encaixa na
				 * célula de grade livre mais próxima de onde soltou, em
				 * vez de deixar na posição de pixel exata (que podia
				 * ficar em cima de outro ícone). */
				int cur_x = 0, cur_y = 0;
				swl_desktop_icon_pos(server->desktop, icon, &cur_x, &cur_y);
				int snap_x, snap_y;
				swl_desktop_find_free_slot(server->desktop, icon,
					cur_x, cur_y, &snap_x, &snap_y);
				swl_desktop_move_icon(server->desktop, icon, snap_x, snap_y);
			}
		}
		/* Descarrega o resize pendente (posição+tamanho+decoração juntos)
		 * ANTES de soltar server->grabbed_toplevel — reset_cursor_mode()
		 * zera esse ponteiro, e é ele quem output_frame() usa pra saber
		 * qual toplevel aplicar. Sem isso, soltar o botão entre o último
		 * motion e o próximo frame perderia o ajuste final pra sempre. */
		if (server->resize_pending && server->grabbed_toplevel) {
			struct tinywl_toplevel *t = server->grabbed_toplevel;
			wlr_scene_node_set_position(&t->scene_tree->node,
				server->resize_pending_scene_x, server->resize_pending_scene_y);
			wlr_xdg_toplevel_set_size(t->xdg_toplevel,
				server->resize_pending_width, server->resize_pending_height);
			if (t->decoration && server->resize_pending_width > 0) {
				swl_decoration_resize(t->decoration, server->resize_pending_width);
			}
			server->resize_pending = false;
		}
		/* Snap-pra-maximizar: se estava movendo (não redimensionando) e a
		 * janela ficou encostada no painel quando soltou o botão. */
		if (server->cursor_mode == TINYWL_CURSOR_MOVE &&
				server->move_snap_maximize && server->grabbed_toplevel) {
			toplevel_set_maximized(server->grabbed_toplevel, true);
		}
		server->move_snap_maximize = false;
		/* If you released any buttons, we exit interactive move/resize mode. */
		reset_cursor_mode(server);
		return;
	}

	/* 0) Menu iniciar aberto: flutua por cima de tudo, então este bloco
	 *    precisa vir antes até da taskbar — é ele quem decide se o clique
	 *    vira lançamento de app, é absorvido, ou "vaza" pro resto. */
	if (server->menu && swl_menu_is_open(server->menu)) {
		int mhit = swl_menu_hit_test(server->menu,
			server->cursor->x, server->cursor->y);
		if (mhit >= 0) {
			/* Clicou num app: lança (mesmo padrão dos ícones do desktop)
			 * e fecha o menu. */
			const char *cmd = swl_menu_item_command(server->menu, mhit);
			swl_menu_close(server->menu);
			if (cmd && cmd[0]) {
				swl_launch_command(cmd);
			}
			return;
		}
		if (mhit == SWL_MENU_HIT_INSIDE) {
			/* Dentro do menu mas fora de qualquer linha: absorve o clique
			 * (não fecha, não deixa vazar pra janela/desktop embaixo). */
			return;
		}
		/* Fora do menu: fecha e deixa o clique seguir o processamento
		 * normal — exceto dentro da faixa Y da taskbar, onde o próprio
		 * botão MENU (bloco abaixo) cuida do toggle; se fechássemos aqui,
		 * o mesmo clique fecharia e reabriria o menu, cancelando o toggle. */
		if (server->cursor->y < server->screen_height - SWL_TASKBAR_HEIGHT) {
			swl_menu_close(server->menu);
		}
	}

	/* 0.5) Menu de contexto (botão direito) aberto: mesma prioridade
	 *      máxima do menu iniciar — também flutua por cima de tudo. Ao
	 *      contrário do menu iniciar, não tem um botão fixo próprio (não
	 *      existe "botão de contexto" na taskbar), então qualquer clique
	 *      fora simplesmente fecha, sem a exceção de faixa Y que o menu
	 *      iniciar precisa pra não se auto-cancelar. */
	if (server->ctx_menu && swl_context_menu_is_open(server->ctx_menu)) {
		int chit = swl_context_menu_hit_test(server->ctx_menu,
			server->cursor->x, server->cursor->y);
		if (chit >= 0) {
			int target = server->ctx_menu_target;
			swl_context_menu_close(server->ctx_menu);
			if (target >= 0) {
				/* Menu aberto em cima de um ícone: item 0 = Abrir,
				 * item 1 = Remover do desktop. */
				if (chit == 0) {
					const char *cmd = swl_desktop_icon_command(server->desktop, target);
					if (cmd && cmd[0]) {
						swl_launch_command(cmd);
					}
				} else if (chit == 1) {
					swl_desktop_hide_icon(server->desktop, target);
				}
			} else if (target == -2) {
				/* Opções do painel: item 0 = CPU, 1 = MEM, 2 = segundos —
				 * cada clique alterna aquela opção e fecha (reabrir pra
				 * mexer em outra, mesma UX simples do resto dos menus). */
				if (chit == 0) {
					server->panel->show_cpu = !server->panel->show_cpu;
				} else if (chit == 1) {
					server->panel->show_mem = !server->panel->show_mem;
				} else if (chit == 2) {
					server->panel->clock_show_seconds = !server->panel->clock_show_seconds;
				}
				swl_panel_redraw_now(server->panel);
			} else {
				/* Menu aberto em área vazia: item 0 = Restaurar ícones
				 * removidos (só aparece se houver algum removido). */
				if (chit == 0) {
					swl_desktop_show_all_icons(server->desktop);
				}
			}
			return;
		}
		if (chit == SWL_CTXMENU_HIT_INSIDE) {
			return;
		}
		swl_context_menu_close(server->ctx_menu);
		/* Continua o processamento normal — clicar fora só fecha, o
		 * próprio clique ainda vale pro que estiver embaixo. */
	}

	/* 0.7) Painel — clique em SISTEMA abre "Opções do painel" (reaproveita
	 *      o mesmo menu de contexto genérico, com um alvo diferente:
	 *      -2 em vez de índice de ícone ou -1 de área vazia). Cai antes da
	 *      taskbar porque o painel também deve ter prioridade sobre
	 *      qualquer coisa embaixo dele (nunca tem toplevel embaixo do
	 *      painel de qualquer forma, mas mantém a mesma disciplina). */
	if (server->panel && swl_panel_hit_test_sistema(server->panel,
			server->cursor->x, server->cursor->y)) {
		char label_cpu[32], label_mem[32], label_sec[40];
		snprintf(label_cpu, sizeof(label_cpu), "CPU: %s",
			server->panel->show_cpu ? "LIGADO" : "DESLIGADO");
		snprintf(label_mem, sizeof(label_mem), "MEM: %s",
			server->panel->show_mem ? "LIGADO" : "DESLIGADO");
		snprintf(label_sec, sizeof(label_sec), "Segundos no relogio: %s",
			server->panel->clock_show_seconds ? "LIGADO" : "DESLIGADO");
		const char *labels[] = { label_cpu, label_mem, label_sec };
		server->ctx_menu_target = -2; /* opções do painel */
		swl_context_menu_open(server->ctx_menu,
			(int)server->cursor->x, SWL_PANEL_HEIGHT,
			server->screen_width, server->screen_height,
			labels, 3);
		return;
	}

	/* 1) Taskbar tem prioridade — clicar nela nunca deve atravessar pra uma
	 *    janela por baixo. */
	if (server->taskbar) {
		int hit = swl_taskbar_hit_test(server->taskbar,
			server->cursor->x, server->cursor->y);
		if (hit == -1) {
			if (server->menu) {
				swl_menu_toggle(server->menu);
			}
			return;
		} else if (hit >= 0) {
			int idx = 0;
			struct tinywl_toplevel *target = NULL, *t_iter;
			wl_list_for_each(t_iter, &server->toplevels, link) {
				if (idx == hit) {
					target = t_iter;
					break;
				}
				idx++;
			}
			if (target) {
				if (target->minimized) {
					/* toplevel_set_minimized(false) já chama focus_toplevel
					 * por dentro, então não faz isso de novo. */
					toplevel_set_minimized(target, false);
				} else {
					focus_toplevel(target, target->xdg_toplevel->base->surface);
				}
			}
			return;
		}
		/* hit == -2: clique fora da taskbar, continua o processamento normal. */
	}

	/* 2) Decoração das janelas (fechar/maximizar/minimizar/arrastar).
	 *    Pula minimizadas (nó desabilitado / invisível) — sem isso um
	 *    clique pode fechar/arrastar janela que o usuário não vê (R-04).
	 *    A lista toplevels tem a focada na cabeça (raise_to_top), então
	 *    o primeiro hit em Z-order é o correto. */
	struct tinywl_toplevel *t_iter;
	wl_list_for_each(t_iter, &server->toplevels, link) {
		if (t_iter->minimized || t_iter->fullscreen || !t_iter->decoration) {
			continue;
		}
		double lx = server->cursor->x - t_iter->scene_tree->node.x;
		double ly = server->cursor->y - t_iter->scene_tree->node.y;
		enum swl_deco_button btn = swl_decoration_hit_test(t_iter->decoration, lx, ly);
		if (btn == SWL_DECO_NONE) {
			continue;
		}
		switch (btn) {
		case SWL_DECO_CLOSE:
			wlr_xdg_toplevel_send_close(t_iter->xdg_toplevel);
			return;
		case SWL_DECO_MAXIMIZE:
			toplevel_set_maximized(t_iter, !t_iter->maximized);
			return;
		case SWL_DECO_MINIMIZE:
			toplevel_set_minimized(t_iter, true);
			return;
		case SWL_DECO_DRAG:
			/* Arrastar a barra de título de uma janela maximizada restaura
			 * o tamanho original primeiro (comportamento padrão de
			 * qualquer WM: puxar pelo título "desgruda" a maximização). */
			if (t_iter->maximized) {
				toplevel_set_maximized(t_iter, false);
			}
			focus_toplevel(t_iter, t_iter->xdg_toplevel->base->surface);
			begin_interactive(t_iter, TINYWL_CURSOR_MOVE, 0);
			return;
		default:
			break;
		}
	}

	/* 2.5) Borda da janela → RESIZE (server-side: a decoração SWL é
	 *      desenhada pelo compositor, então o cliente nunca vê esse
	 *      clique — sem este bloco não há como redimensionar com o mouse.
	 *      Usa o mesmo anel de SWL_RESIZE_MARGIN do cursor visual em
	 *      process_cursor_motion. Janela maximizada é ignorada (a borda
	 *      está fora da tela; pra redimensionar, desmaximize arrastando
	 *      a barra de título primeiro). */
	if (toplevel && event->button == BTN_LEFT &&
			!toplevel->maximized && !toplevel->fullscreen) {
		struct wlr_box geo;
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo);
		double wx = server->cursor->x - toplevel->scene_tree->node.x;
		double wy = server->cursor->y - toplevel->scene_tree->node.y;
		double ww = geo.x + geo.width;
		double wh = SWL_TITLEBAR_HEIGHT + geo.y + geo.height;
		uint32_t edges = 0;
		if (wx >= 0 && wx < SWL_RESIZE_MARGIN) {
			edges |= WLR_EDGE_LEFT;
		} else if (wx >= ww - SWL_RESIZE_MARGIN && wx < ww) {
			edges |= WLR_EDGE_RIGHT;
		}
		if (wy >= 0 && wy < SWL_RESIZE_MARGIN) {
			edges |= WLR_EDGE_TOP;
		} else if (wy >= wh - SWL_RESIZE_MARGIN && wy < wh) {
			edges |= WLR_EDGE_BOTTOM;
		}
		if (edges) {
			focus_toplevel(toplevel, surface);
			begin_interactive(toplevel, TINYWL_CURSOR_RESIZE, edges);
			return;
		}
	}

	/* 3) Se nada abaixo do cursor for uma janela, tenta os ícones da área
	 *    de trabalho — botão direito abre menu de contexto, esquerdo
	 *    arma um clique/arrasto pendente (decidido no motion/release, ver
	 *    process_cursor_motion e o bloco WLR_BUTTON_RELEASED acima). */
	if (toplevel == NULL && server->desktop) {
		int icon_idx = swl_desktop_hit_test_index(server->desktop,
			server->cursor->x, server->cursor->y);

		if (event->button == BTN_RIGHT) {
			if (icon_idx >= 0) {
				const char *labels[] = { "Abrir", "Remover do desktop" };
				server->ctx_menu_target = icon_idx;
				swl_context_menu_open(server->ctx_menu,
					(int)server->cursor->x, (int)server->cursor->y,
					server->screen_width, server->screen_height,
					labels, 2);
				return;
			} else if (swl_desktop_has_hidden_icons(server->desktop)) {
				const char *labels[] = { "Restaurar icones removidos" };
				server->ctx_menu_target = -1;
				swl_context_menu_open(server->ctx_menu,
					(int)server->cursor->x, (int)server->cursor->y,
					server->screen_width, server->screen_height,
					labels, 1);
				return;
			}
			/* Área vazia sem nada removido: botão direito não faz nada
			 * (ainda não existe "Novo atalho..." — ver ressalva na
			 * entrega). Cai pro caso padrão (foco) como qualquer clique
			 * em área vazia faria. */
		} else if (event->button == BTN_LEFT && icon_idx >= 0) {
			server->desktop_drag_pending = true;
			server->desktop_drag_active = false;
			server->desktop_drag_icon = icon_idx;
			server->desktop_drag_press_x = server->cursor->x;
			server->desktop_drag_press_y = server->cursor->y;
			swl_desktop_icon_pos(server->desktop, icon_idx,
				&server->desktop_drag_icon_orig_x,
				&server->desktop_drag_icon_orig_y);
			return;
		}
	}

	/* 4) Caso padrão: focar a janela (ou superfície) sob o cursor. */
	focus_toplevel(toplevel, surface);
}

static void server_cursor_axis(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */
	struct tinywl_server *server =
		wl_container_of(listener, server, cursor_axis);
	struct wlr_pointer_axis_event *event = data;
	/* Notify the client with pointer focus of the axis event. */
#if SWL_WLR_0_18
	wlr_seat_pointer_notify_axis(server->seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source, event->relative_direction);
#else
	wlr_seat_pointer_notify_axis(server->seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source);
#endif
}

static void server_cursor_frame(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at the
	 * same time, in which case a frame event won't be sent in between. */
	struct tinywl_server *server =
		wl_container_of(listener, server, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(server->seat);
}

static void output_frame(struct wl_listener *listener, void *data) {
	/* This function is called every time an output is ready to display a frame,
	 * generally at the output's refresh rate (e.g. 60Hz). */
	struct tinywl_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene *scene = output->server->scene;

	/* Resize interativo pendente (ver process_cursor_resize) — posição,
	 * pedido de tamanho ao cliente e redesenho da decoração aplicados
	 * juntos, no máximo uma vez por frame, nunca separadamente. */
	struct tinywl_server *server = output->server;
	if (server->resize_pending && server->grabbed_toplevel) {
		struct tinywl_toplevel *t = server->grabbed_toplevel;
		wlr_scene_node_set_position(&t->scene_tree->node,
			server->resize_pending_scene_x, server->resize_pending_scene_y);
		wlr_xdg_toplevel_set_size(t->xdg_toplevel,
			server->resize_pending_width, server->resize_pending_height);
		if (t->decoration && server->resize_pending_width > 0) {
			swl_decoration_resize(t->decoration, server->resize_pending_width);
		}
		server->resize_pending = false;
	}

	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(
		scene, output->wlr_output);

	/* Render the scene if needed and commit the output */
	wlr_scene_output_commit(scene_output, NULL);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_request_state(struct wl_listener *listener, void *data) {
	/* This function is called when the backend requests a new state for
	 * the output. For example, Wayland and X11 backends request a new mode
	 * when the output window is resized. */
	struct tinywl_output *output = wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->wlr_output, event->state);

	struct tinywl_server *server = output->server;
	int ow, oh;
	wlr_output_effective_resolution(output->wlr_output, &ow, &oh);
	if (ow > 0 && oh > 0 &&
			(ow != server->screen_width || oh != server->screen_height)) {
		server->screen_width = ow;
		server->screen_height = oh;
		if (server->panel != NULL) {
			swl_background_resize(server->background, ow, oh, server->background_path);
			swl_panel_resize(server->panel, ow);
			swl_taskbar_resize(server->taskbar, ow, oh - SWL_TASKBAR_HEIGHT);
			swl_menu_resize(server->menu, oh);
		}
		/* Reflow maximizadas (A5) e fullscreen (A6). */
		struct tinywl_toplevel *t;
		wl_list_for_each(t, &server->toplevels, link) {
			if (t->minimized) {
				continue;
			}
			if (t->fullscreen) {
				toplevel_apply_fullscreen_layout(t);
			} else if (t->maximized) {
				toplevel_apply_maximized_layout(t);
			}
		}
	}
}

static void output_destroy(struct wl_listener *listener, void *data) {
	struct tinywl_output *output = wl_container_of(listener, output, destroy);

	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);
	free(output);
}

static void server_new_output(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new output (aka a display or
	 * monitor) becomes available. */
	struct tinywl_server *server =
		wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	/* Configures the output created by the backend to use our allocator
	 * and our renderer. Must be done once, before commiting the output */
	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	/* The output may be disabled, switch it on. */
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);

	/* Some backends don't have modes. DRM+KMS does, and we need to set a mode
	 * before we can use the output. The mode is a tuple of (width, height,
	 * refresh rate), and each monitor supports only a specific set of modes. We
	 * just pick the monitor's preferred mode, a more sophisticated compositor
	 * would let the user configure it. */
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode != NULL) {
		wlr_output_state_set_mode(&state, mode);
	}

	/* Atomically applies the new output state. */
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	/* Allocates and configures our state for this output */
	struct tinywl_output *output = calloc(1, sizeof(*output));
	output->wlr_output = wlr_output;
	output->server = server;

	/* Sets up a listener for the frame event. */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	/* Sets up a listener for the state request event. */
	output->request_state.notify = output_request_state;
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);

	/* Sets up a listener for the destroy event. */
	output->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	wl_list_insert(&server->outputs, &output->link);

	/* Adds this to the output layout. The add_auto function arranges outputs
	 * from left-to-right in the order they appear. A more sophisticated
	 * compositor would let the user configure the arrangement of outputs in the
	 * layout.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
	struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(server->output_layout,
		wlr_output);
	struct wlr_scene_output *scene_output = wlr_scene_output_create(server->scene, wlr_output);
	wlr_scene_output_layout_add_output(server->scene_layout, l_output, scene_output);

	/* Cria (na primeira vez) ou redimensiona (em mudanças de modo) os
	 * elementos de shell do SWL OS com base na resolução efetiva do
	 * primeiro output. Um compositor multi-monitor "de verdade" trataria
	 * isso por output; aqui mantemos simples: um único conjunto de
	 * painel/taskbar/desktop cobrindo o layout inteiro. */
	int ow, oh;
	wlr_output_effective_resolution(wlr_output, &ow, &oh);
	if (ow > 0 && oh > 0) {
		server->screen_width = ow;
		server->screen_height = oh;
		if (server->panel == NULL) {
			struct wl_event_loop *loop = wl_display_get_event_loop(server->wl_display);
			server->background = swl_background_create(&server->scene->tree, ow, oh,
				server->background_path);
			server->desktop = swl_desktop_create(&server->scene->tree, SWL_PANEL_HEIGHT);
			server->panel = swl_panel_create(loop, &server->scene->tree, ow);
			server->taskbar = swl_taskbar_create(loop, &server->scene->tree,
				ow, oh - SWL_TASKBAR_HEIGHT);
			server->menu = swl_menu_create(&server->scene->tree);
			swl_menu_resize(server->menu, oh);
			server->ctx_menu = swl_context_menu_create(&server->scene->tree);
			server->ctx_menu_target = -1;
		} else {
			swl_background_resize(server->background, ow, oh, server->background_path);
			swl_panel_resize(server->panel, ow);
			swl_taskbar_resize(server->taskbar, ow, oh - SWL_TASKBAR_HEIGHT);
			swl_menu_resize(server->menu, oh);
			/* Mesmo reflow de maximizadas/fullscreen que em
			 * output_request_state (A5 + complemento A6 —
			 * docs/revisao/2026-09-08-aviso-grok-a6.md). */
			struct tinywl_toplevel *t;
			wl_list_for_each(t, &server->toplevels, link) {
				if (t->minimized) {
					continue;
				}
				if (t->fullscreen) {
					toplevel_apply_fullscreen_layout(t);
				} else if (t->maximized) {
					toplevel_apply_maximized_layout(t);
				}
			}
		}
	}
}

static void xdg_toplevel_commit(struct wl_listener *listener, void *data) {
	/* Chamado a cada commit da superfície. Usamos isso pra manter a barra de
	 * título com a largura certa e o texto do título atualizado — o
	 * xdg-shell não tem um evento dedicado de "mudança de título", o valor
	 * só fica garantidamente atualizado depois de um commit. */
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, commit);
	struct wlr_xdg_toplevel *xt = toplevel->xdg_toplevel;
#if SWL_WLR_0_18
	/* wlroots 0.18: o configure inicial precisa ser agendado pelo
	 * compositor, mas só pode ser enviado depois que a surface
	 * inicializou (primeiro commit com role). new_toplevel dispara cedo
	 * demais ("uninitialized"); aqui, no primeiro commit, a surface já
	 * é válida. Agendamos UMA vez (o ack do cliente marca a surface
	 * como initialized, então paramos de agendar depois disso — senão
	 * cria um loop configure→commit→schedule infinito). */
	if (!toplevel->initial_configure_sent) {
		toplevel->initial_configure_sent = true;
		wlr_xdg_surface_schedule_configure(xt->base);
	}
#endif
	if (!toplevel->decoration) {
		return;
	}
	int width = xt->base->surface->current.width;
	if (width <= 0) {
		width = toplevel->deco_width;
	}
	const char *title = xt->title ? xt->title : "janela";

	bool width_changed = (width != toplevel->deco_width);
	bool title_changed = (toplevel->deco_title == NULL || strcmp(title, toplevel->deco_title) != 0);

	if (width_changed) {
		toplevel->deco_width = width;
		swl_decoration_resize(toplevel->decoration, width);
	}
	if (title_changed) {
		free(toplevel->deco_title);
		toplevel->deco_title = strdup(title);
		swl_decoration_set_title(toplevel->decoration, title);
	}
}

static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
	/* Called when the surface is mapped, or ready to display on-screen. */
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, map);

	wl_list_insert(&toplevel->server->toplevels, &toplevel->link);

	focus_toplevel(toplevel, toplevel->xdg_toplevel->base->surface);
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, unmap);

	/* Reset the cursor mode if the grabbed toplevel was unmapped. */
	if (toplevel == toplevel->server->grabbed_toplevel) {
		reset_cursor_mode(toplevel->server);
	}

	wl_list_remove(&toplevel->link);
	update_taskbar(toplevel->server);
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
	/* Called when the xdg_toplevel is destroyed. */
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, destroy);

	wl_list_remove(&toplevel->map.link);
	wl_list_remove(&toplevel->unmap.link);
	wl_list_remove(&toplevel->destroy.link);
	wl_list_remove(&toplevel->commit.link);
	wl_list_remove(&toplevel->request_move.link);
	wl_list_remove(&toplevel->request_resize.link);
	wl_list_remove(&toplevel->request_maximize.link);
	wl_list_remove(&toplevel->request_fullscreen.link);

	swl_decoration_destroy(toplevel->decoration);
	free(toplevel->deco_title);
	/* scene_tree (o wrapper) ainda contém o content_tree como filho; ao
	 * destruir o wrapper toda a subárvore (conteúdo + decoração) some junto. */
	wlr_scene_node_destroy(&toplevel->scene_tree->node);

	free(toplevel);
}

static void begin_interactive(struct tinywl_toplevel *toplevel,
		enum tinywl_cursor_mode mode, uint32_t edges) {
	/* This function sets up an interactive move or resize operation, where the
	 * compositor stops propegating pointer events to clients and instead
	 * consumes them itself, to move or resize windows. */
	struct tinywl_server *server = toplevel->server;
	struct wlr_surface *focused_surface =
		server->seat->pointer_state.focused_surface;
	if (focused_surface != NULL &&
			toplevel->xdg_toplevel->base->surface !=
			wlr_surface_get_root_surface(focused_surface)) {
		/* Deny move/resize requests from unfocused clients. Quando o clique
		 * partiu da NOSSA barra de titulo (nao de uma superficie real do
		 * app), focused_surface vem NULL - nesse caso ja sabemos qual
		 * toplevel mover (veio do hit-test da decoracao), entao pulamos
		 * essa checagem em vez de arriscar comparar contra NULL. */
		return;
	}
	server->grabbed_toplevel = toplevel;
	server->cursor_mode = mode;

	if (mode == TINYWL_CURSOR_MOVE) {
		server->grab_x = server->cursor->x - toplevel->scene_tree->node.x;
		server->grab_y = server->cursor->y - toplevel->scene_tree->node.y;
	} else {
		struct wlr_box geo_box;
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo_box);

		/* +SWL_TITLEBAR_HEIGHT porque toplevel->scene_tree->node é o topo do
		 * wrapper (onde fica a barra de título), e o conteúdo real da janela
		 * começa SWL_TITLEBAR_HEIGHT pixels abaixo disso. */
		double border_x = (toplevel->scene_tree->node.x + geo_box.x) +
			((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
		double border_y = (toplevel->scene_tree->node.y + SWL_TITLEBAR_HEIGHT + geo_box.y) +
			((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
		server->grab_x = server->cursor->x - border_x;
		server->grab_y = server->cursor->y - border_y;

		server->grab_geobox = geo_box;
		server->grab_geobox.x += toplevel->scene_tree->node.x;
		server->grab_geobox.y += toplevel->scene_tree->node.y + SWL_TITLEBAR_HEIGHT;

		server->resize_edges = edges;
	}
}

static void xdg_toplevel_request_move(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provided serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, request_move);
	begin_interactive(toplevel, TINYWL_CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provided serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, request_resize);
	begin_interactive(toplevel, TINYWL_CURSOR_RESIZE, event->edges);
}

static void xdg_toplevel_request_maximize(
		struct wl_listener *listener, void *data) {
	/* Pedido de maximizar/restaurar vindo do próprio cliente (ex.: um app
	 * com seu próprio atalho ou botão que chama xdg_toplevel.set_maximized/
	 * unset_maximized). O estado desejado já vem populado pelo wlroots em
	 * xdg_toplevel->requested.maximized antes deste evento disparar — não
	 * precisamos inspecionar o request bruto. Reaproveita a mesma lógica
	 * usada pelo botão da nossa decoração, então os dois caminhos nunca
	 * divergem. */
	struct tinywl_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_maximize);
	toplevel_set_maximized(toplevel, toplevel->xdg_toplevel->requested.maximized);
}

static void xdg_toplevel_request_fullscreen(
		struct wl_listener *listener, void *data) {
	/* Pedido de fullscreen vindo do cliente (ex.: vídeo, jogo, F11).
	 * requested.fullscreen já vem preenchido pelo wlroots. A6: aplica
	 * layout real, não só schedule_configure vazio. */
	(void)data;
	struct tinywl_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_fullscreen);
	toplevel_set_fullscreen(toplevel,
		toplevel->xdg_toplevel->requested.fullscreen);
}

/* Cria a subárvore de cena de um popup xdg, pendurada no nó do popup pai.
 * Caminho comum das duas gerações de API do xdg_shell (ver guard de versão
 * no topo do arquivo). */
static void server_new_popup_tree(struct wlr_xdg_surface *xdg_surface,
		struct wlr_surface *parent_surface) {
	struct wlr_xdg_surface *parent =
		wlr_xdg_surface_try_from_wlr_surface(parent_surface);
	assert(parent != NULL);
	struct wlr_scene_tree *parent_tree = parent->data;
	xdg_surface->data = wlr_scene_xdg_surface_create(parent_tree, xdg_surface);
}

/* Inicializa um tinywl_toplevel (wrapper decoração+conteúdo, listeners) a
 * partir de um xdg_toplevel que já tem role atribuído. Caminho comum das
 * duas gerações de API do xdg_shell. */
static void server_new_toplevel(struct tinywl_server *server,
		struct wlr_xdg_toplevel *xdg_toplevel) {
	struct wlr_xdg_surface *xdg_surface = xdg_toplevel->base;

	/* Allocate a tinywl_toplevel for this surface */
	struct tinywl_toplevel *toplevel = calloc(1, sizeof(*toplevel));
	toplevel->server = server;
	toplevel->xdg_toplevel = xdg_toplevel;

	/* scene_tree é o wrapper (decoração + conteúdo); content_tree é a
	 * subárvore de fato da superfície xdg, deslocada para abrir espaço pra
	 * barra de título desenhada por cima. */
	toplevel->scene_tree = wlr_scene_tree_create(&toplevel->server->scene->tree);
	toplevel->scene_tree->node.data = toplevel;

	toplevel->content_tree = wlr_scene_xdg_surface_create(
			toplevel->scene_tree, xdg_surface);
	wlr_scene_node_set_position(&toplevel->content_tree->node, 0, SWL_TITLEBAR_HEIGHT);
	xdg_surface->data = toplevel->content_tree;

	const char *initial_title = xdg_toplevel->title;
	toplevel->deco_title = strdup(initial_title ? initial_title : "janela");
	toplevel->deco_width = 200;
	toplevel->decoration = swl_decoration_create(toplevel->scene_tree,
		toplevel->deco_width, toplevel->deco_title);

	/* Listen to the various events it can emit */
	toplevel->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_surface->surface->events.map, &toplevel->map);
	toplevel->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_surface->surface->events.unmap, &toplevel->unmap);
	toplevel->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &toplevel->destroy);
	toplevel->commit.notify = xdg_toplevel_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &toplevel->commit);

	toplevel->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
	toplevel->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);
	toplevel->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&xdg_toplevel->events.request_maximize,
		&toplevel->request_maximize);
	toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen,
		&toplevel->request_fullscreen);
}

#if SWL_WLR_0_18
/* wlroots 0.18: new_surface dispara com role NONE; toplevel e popup têm
 * eventos próprios, já com o role atribuído. */
static void server_new_xdg_toplevel(struct wl_listener *listener, void *data) {
	struct tinywl_server *server =
		wl_container_of(listener, server, new_xdg_toplevel);
	server_new_toplevel(server, data);
}

static void server_new_xdg_popup(struct wl_listener *listener, void *data) {
	struct wlr_xdg_popup *popup = data;
	server_new_popup_tree(popup->base, popup->parent);
}
#else
/* wlroots 0.17: um único evento new_surface, já disparado com o role
 * atribuído (toplevel ou popup). */
static void server_new_xdg_surface(struct wl_listener *listener, void *data) {
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup. */
	struct tinywl_server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;

	/* We must add xdg popups to the scene graph so they get rendered. The
	 * wlroots scene graph provides a helper for this, but to use it we must
	 * provide the proper parent scene node of the xdg popup. To enable this,
	 * we always set the user data field of xdg_surfaces to the corresponding
	 * scene node. */
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		server_new_popup_tree(xdg_surface, xdg_surface->popup->parent);
		return;
	}
	assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);
	server_new_toplevel(server, xdg_surface->toplevel);
}
#endif

int main(int argc, char *argv[]) {
	wlr_log_init(WLR_DEBUG, NULL);
	char *startup_cmd = NULL;
	char *background_path = NULL;

	int c;
	while ((c = getopt(argc, argv, "s:b:h")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		case 'b':
			background_path = optarg;
			break;
		default:
			printf("Usage: %s [-s startup command] [-b caminho/para/wallpaper.png]\n", argv[0]);
			return 0;
		}
	}
	if (optind < argc) {
		printf("Usage: %s [-s startup command] [-b caminho/para/wallpaper.png]\n", argv[0]);
		return 0;
	}

	/* Sem -b explícito, procura um wallpaper padrão em alguns lugares
	 * óbvios (pasta assets/ do repositório, ou instalado no sistema) antes
	 * de cair no fallback procedural (grade ciano) desenhado em background.c. */
	static char default_bg[PATH_MAX];
	if (!background_path) {
		const char *candidates[] = {
			"assets/wallpaper.png",
			"../assets/wallpaper.png",
			"/usr/local/share/swl-ui/wallpaper.png",
			"/usr/share/swl-ui/wallpaper.png",
		};
		for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
			if (access(candidates[i], R_OK) == 0) {
				snprintf(default_bg, sizeof(default_bg), "%s", candidates[i]);
				background_path = default_bg;
				break;
			}
		}
	}

	struct tinywl_server server = {0};
	server.background_path = background_path;
	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	server.wl_display = wl_display_create();
	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 server is running. */
#if SWL_WLR_0_18
	server.backend = wlr_backend_autocreate(
		wl_display_get_event_loop(server.wl_display), NULL);
#else
	server.backend = wlr_backend_autocreate(server.wl_display, NULL);
#endif
	if (server.backend == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_backend");
		return 1;
	}

	/* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
	 * can also specify a renderer using the WLR_RENDERER env var.
	 * The renderer is responsible for defining the various pixel formats it
	 * supports for shared memory, this configures that for clients. */
	server.renderer = wlr_renderer_autocreate(server.backend);
	if (server.renderer == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_renderer");
		return 1;
	}

	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	/* Autocreates an allocator for us.
	 * The allocator is the bridge between the renderer and the backend. It
	 * handles the buffer creation, allowing wlroots to render onto the
	 * screen */
	server.allocator = wlr_allocator_autocreate(server.backend,
		server.renderer);
	if (server.allocator == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_allocator");
		return 1;
	}

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces, the subcompositor allows to
	 * assign the role of subsurfaces to surfaces and the data device manager
	 * handles the clipboard. Each of these wlroots interfaces has room for you
	 * to dig your fingers in and play with their behavior if you want. Note that
	 * the clients cannot set the selection directly without compositor approval,
	 * see the handling of the request_set_selection event below.*/
	wlr_compositor_create(server.wl_display, 5, server.renderer);
	wlr_subcompositor_create(server.wl_display);
	wlr_data_device_manager_create(server.wl_display);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
#if SWL_WLR_0_18
	server.output_layout = wlr_output_layout_create(server.wl_display);
#else
	server.output_layout = wlr_output_layout_create();
#endif

	/* Configure a listener to be notified when new outputs are available on the
	 * backend. */
	wl_list_init(&server.outputs);
	server.new_output.notify = server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	/* Create a scene graph. This is a wlroots abstraction that handles all
	 * rendering and damage tracking. All the compositor author needs to do
	 * is add things that should be rendered to the scene graph at the proper
	 * positions and then call wlr_scene_output_commit() to render a frame if
	 * necessary.
	 */
	server.scene = wlr_scene_create();
	server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);

	/* Set up xdg-shell version 3. The xdg-shell is a Wayland protocol which is
	 * used for application windows. For more detail on shells, refer to
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html.
	 */
	wl_list_init(&server.toplevels);
	server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 3);
#if SWL_WLR_0_18
	server.new_xdg_toplevel.notify = server_new_xdg_toplevel;
	wl_signal_add(&server.xdg_shell->events.new_toplevel,
			&server.new_xdg_toplevel);
	server.new_xdg_popup.notify = server_new_xdg_popup;
	wl_signal_add(&server.xdg_shell->events.new_popup,
			&server.new_xdg_popup);
#else
	server.new_xdg_surface.notify = server_new_xdg_surface;
	wl_signal_add(&server.xdg_shell->events.new_surface,
			&server.new_xdg_surface);
#endif

	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	server.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

	/* Creates an xcursor manager, another wlroots utility which loads up
	 * Xcursor themes to source cursor images from and makes sure that cursor
	 * images are available at all scale factors on the screen (necessary for
	 * HiDPI support).
	 *
	 * Tema próprio do SWL OS ("swl", ver swl-ui/tools/gen-cursors/) em vez
	 * do tema instalado no sistema host — sem isso, o visual do cursor (e
	 * até se ele troca de imagem corretamente) dependia inteiramente de
	 * qual tema Xcursor estivesse instalado em cada máquina de
	 * desenvolvimento, com cobertura de nomes/aliases imprevisível. Mesmo
	 * padrão de resolução de caminho do wallpaper/ícones: candidatos
	 * relativos (rodando de dentro de build/) antes de instalado no
	 * sistema. Se não achar em nenhum candidato, cai pro tema do sistema
	 * (NULL) — degrada, não quebra. */
	{
		const char *cursor_candidates[] = {
			"../assets/cursors",
			"assets/cursors",
			"/usr/local/share/swl-ui/cursors",
			"/usr/share/swl-ui/cursors",
		};
		const char *theme_name = NULL;
		for (size_t i = 0; i < sizeof(cursor_candidates) / sizeof(cursor_candidates[0]); i++) {
			char probe[PATH_MAX];
			snprintf(probe, sizeof(probe), "%s/swl/cursors/default", cursor_candidates[i]);
			if (access(probe, R_OK) == 0) {
				setenv("XCURSOR_PATH", cursor_candidates[i], 1);
				theme_name = "swl";
				break;
			}
		}
		server.cursor_mgr = wlr_xcursor_manager_create(theme_name, 24);
	}

	/*
	 * wlr_cursor *only* displays an image on screen. It does not move around
	 * when the pointer moves. However, we can attach input devices to it, and
	 * it will generate aggregate events for all of them. In these events, we
	 * can choose how we want to process them, forwarding them to clients and
	 * moving the cursor around. More detail on this process is described in
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html.
	 *
	 * And more comments are sprinkled throughout the notify functions above.
	 */
	server.cursor_mode = TINYWL_CURSOR_PASSTHROUGH;
	server.cursor_motion.notify = server_cursor_motion;
	wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
	server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
	wl_signal_add(&server.cursor->events.motion_absolute,
			&server.cursor_motion_absolute);
	server.cursor_button.notify = server_cursor_button;
	wl_signal_add(&server.cursor->events.button, &server.cursor_button);
	server.cursor_axis.notify = server_cursor_axis;
	wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
	server.cursor_frame.notify = server_cursor_frame;
	wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener to
	 * let us know when new input devices are available on the backend.
	 */
	wl_list_init(&server.keyboards);
	server.new_input.notify = server_new_input;
	wl_signal_add(&server.backend->events.new_input, &server.new_input);
	server.seat = wlr_seat_create(server.wl_display, "seat0");
	server.request_cursor.notify = seat_request_cursor;
	wl_signal_add(&server.seat->events.request_set_cursor,
			&server.request_cursor);
	server.request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&server.seat->events.request_set_selection,
			&server.request_set_selection);

	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_backend_destroy(server.backend);
		return 1;
	}

	/* Start the backend. This will enumerate outputs and inputs, become the DRM
	 * master, etc */
	if (!wlr_backend_start(server.backend)) {
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.wl_display);
		return 1;
	}

	/* Set the WAYLAND_DISPLAY environment variable to our socket and run the
	 * startup command if requested. */
	setenv("WAYLAND_DISPLAY", socket, true);
	if (startup_cmd) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void *)NULL);
		}
	}
	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */
	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
			socket);
	wl_display_run(server.wl_display);

	/* Once wl_display_run returns, we destroy all clients then shut down the
	 * server. */
	wl_display_destroy_clients(server.wl_display);
	swl_panel_destroy(server.panel);
	swl_taskbar_destroy(server.taskbar);
	swl_desktop_destroy(server.desktop);
	swl_menu_destroy(server.menu);
	swl_context_menu_destroy(server.ctx_menu);
	wlr_scene_node_destroy(&server.scene->tree.node);
	wlr_xcursor_manager_destroy(server.cursor_mgr);
	wlr_output_layout_destroy(server.output_layout);
	wl_display_destroy(server.wl_display);
	return 0;
}
