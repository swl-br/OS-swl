# Sessão — 2026-09-03 (2)

IA: Claude (Anthropic)
Data: 2026-09-03
Responsável: implementar maximizar/minimizar de janelas (swl-ui)
Branch: main (a confirmar com o usuário)
Commit: (a preencher no momento do commit — aplicado via patch, não push direto)

## Objetivo

Implementar a primeira tarefa da lista de "Próximos passos sugeridos (GUI)"
em `docs/ai/PROJECT_STATE.md`: maximizar/minimizar janela pelos botões da
decoração (que já existiam visualmente, mas sem lógica por trás).

## Alterações

Arquivos modificados:
- `swl-ui/src/swlwm.c`

Nenhum arquivo de header (`decorations.h`, etc.) precisou mudar — os botões
e o hit-test (`swl_decoration_hit_test`) já existiam e já retornavam
`SWL_DECO_MAXIMIZE`/`SWL_DECO_MINIMIZE` corretamente; só faltava a lógica no
compositor.

## Implementação

1. **Novos campos em `struct tinywl_toplevel`**: `bool maximized`,
   `bool minimized`, `struct wlr_box saved_geo` (posição do wrapper +
   tamanho do conteúdo de antes de maximizar, pra restaurar exatamente
   igual).

2. **`toplevel_set_maximized(toplevel, bool maximize)`** (nova função,
   logo após `focus_toplevel`):
   - Ao maximizar: salva geometria atual, move o `scene_tree` (wrapper)
     para `(0, SWL_PANEL_HEIGHT)`, redimensiona via
     `wlr_xdg_toplevel_set_size()` para a área disponível (tela inteira
     menos painel, taskbar e a própria barra de título), chama
     `wlr_xdg_toplevel_set_maximized(true)` e redimensiona a decoração na
     hora (não espera o próximo commit).
   - Ao restaurar: volta pra `saved_geo` (posição + tamanho) e chama
     `wlr_xdg_toplevel_set_maximized(false)`.
   - Idempotente: chamar com o mesmo estado atual não faz nada.

3. **`toplevel_set_minimized(toplevel, bool minimize)`** (nova função,
   logo depois):
   - Ao minimizar: desabilita o nó `scene_tree` inteiro
     (`wlr_scene_node_set_enabled`), o que já esconde decoração + conteúdo
     juntos (estão na mesma wrapper tree). Limpa foco de teclado se a
     janela estava focada, desativa (`set_activated(false)`) e marca a
     decoração como não-focada. A janela continua na lista de
     `server->toplevels`, então continua aparecendo na taskbar — só some
     da área de trabalho.
   - Ao restaurar: reabilita o nó e chama `focus_toplevel()` de novo
     (mesmo caminho de quando o usuário clica numa janela normalmente).

4. **Ligações feitas**:
   - Clique no botão maximizar/minimizar da decoração
     (`server_cursor_button`, case `SWL_DECO_MAXIMIZE`/`SWL_DECO_MINIMIZE`):
     chamam as funções acima em vez do `TODO` anterior.
   - Clique numa janela na taskbar: se a janela clicada estava
     minimizada, agora restaura (`toplevel_set_minimized(target, false)`)
     em vez de só tentar focar uma janela invisível.
   - Arrastar pela barra de título (`SWL_DECO_DRAG`): se a janela estava
     maximizada, desmaximiza primeiro — comportamento padrão de qualquer
     gerenciador de janelas (puxar o título "desgruda" a maximização).
   - `xdg_toplevel_request_maximize` (pedido vindo do próprio cliente, não
     só do nosso botão): agora chama `toplevel_set_maximized()` de verdade,
     usando `toplevel->xdg_toplevel->requested.maximized` (campo que o
     wlroots já preenche antes de disparar o evento) em vez do
     `wlr_xdg_surface_schedule_configure()` vazio que só existia pra
     conformar com o protocolo sem fazer nada.

## Decisões

Nenhuma decisão arquitetural nova — segue a arquitetura modular já
estabelecida em DEC-004 (nada foi movido de arquivo, só lógica nova dentro
de `swlwm.c`, que é onde o estado de cada toplevel já vive).

## Testes

Ambiente: container Linux isolado (não é o Xubuntu/Mint XFCE de
desenvolvimento de vocês), sem sessão gráfica real. Testado o que dá pra
testar sem interface:

- ✅ `meson setup build && ninja -C build`: compila limpo contra wlroots
  0.17.1 (mesma versão que o `README.md` do swl-ui já documentava ter
  testado), zero warning/erro no código novo (os warnings existentes no
  build são todos pré-existentes, em `panel.c` e parâmetros `data` não
  usados em callbacks — nada relacionado a esta mudança).
- ✅ Execução headless (`WLR_BACKENDS=headless WLR_RENDERER=pixman`):
  inicializa sem crash.
- ✅ Rodando com `-s foot` (cliente Wayland real): o `foot` conecta, cria
  `wl_surface`/`xdg_surface`, mapeia — ou seja, o caminho
  `xdg_toplevel_map` → `focus_toplevel` → criação de decoração passa sem
  travar com os novos campos (`maximized`/`minimized`/`saved_geo`)
  adicionados ao struct.

### Validado interativamente pelo usuário (ambiente real, fora deste container)

Testado por vocês em `WLR_BACKEND=x11` (log real anexado: build limpo,
`foot` mapeando via GLES2/Mesa, sem nenhum erro/segfault):

- ✅ **Maximizar**: confirmado.
- ✅ **Minimizar**: confirmado.
- ✅ **Arrastar** (inclusive o caso de arrastar uma janela maximizada, que
  desmaximiza primeiro): confirmado.
- ✅ **Fechar pelo botão da decoração**: confirmado (já funcionava antes,
  continua funcionando — não foi tocado nesta entrega).
- ⚠️ **Restaurar clicando na janela minimizada na taskbar**: não foi
  mencionado explicitamente no relato do teste — vale um clique a mais
  pra confirmar esse caminho específico (`toplevel_set_minimized(..., false)`
  chamado a partir do clique na taskbar), já que é o único dos caminhos
  novos que não foi citado nominalmente.

## Problemas conhecidos

- Se a janela estiver maximizada e o output for redimensionado (usuário
  redimensiona a janela do X11 host, por exemplo), o tamanho da janela
  maximizada **não** se readapta automaticamente — só o painel/taskbar/
  fundo recalculam (isso já existia antes, em `output_request_state`).
  Ficou fora do escopo desta entrega; se for importante, dá pra guardar
  quais toplevels estão maximizados e re-chamar `toplevel_set_maximized`
  neles quando a resolução mudar.
- Fullscreen (`xdg_toplevel_request_fullscreen`) não foi tocado — continua
  só respondendo com um configure vazio, sem lógica real. Não estava no
  escopo desta tarefa.
- Multi-monitor: mesma limitação de sempre (documentada em DEC/PROJECT_STATE
  anteriores) — a área "maximizada" é calculada só com base no primeiro
  output.

## TODO

1. ~~Validar interativamente no Xubuntu/Mint XFCE (clique real nos
   botões).~~ **Feito** — maximizar, minimizar, arrastar (inclusive
   desmaximizando ao arrastar) e fechar confirmados pelo usuário. Só falta
   um clique explícito de confirmação no caminho "restaurar minimizada
   pela taskbar" (ver ressalva em "Testes" acima).
2. Próxima tarefa: menu iniciar.
3. Depois: integração DRM/KMS (rodar como sessão gráfica real).

## Integração

Qualquer IA que mexer em `swlwm.c` depois disso deve saber:
- `toplevel->maximized`/`toplevel->minimized` agora existem e **precisam**
  ser considerados em qualquer lógica nova de resize/drag/foco — ex.: se
  alguém adicionar suporte a redimensionar o output com janelas
  maximizadas abertas, essa é a lacuna descrita acima em "Problemas
  conhecidos".
- Novo padrão: pedidos de maximizar do cliente (`request_maximize`) e do
  nosso próprio botão de decoração agora convergem pra mesma função
  (`toplevel_set_maximized`) — não duplicar essa lógica em outro lugar.

## Observações

Como combinado, antes de mexer em qualquer coisa eu re-puxei o repositório
(`git fetch origin` + comparei com `HEAD`) pra confirmar que nenhuma outra
IA tinha mudado `swl-ui/` ou `docs/` desde o último clone — sem diferença,
então segui direto pra implementação.

Não tenho credenciais de push para `github.com/swl-br/OS-swl` (acesso só
leitura). A mudança está entregue como patch
(`maximizar-minimizar-swlwm.patch`) pra vocês aplicarem com
`git apply maximizar-minimizar-swlwm.patch` a partir da raiz do repo, ou
simplesmente substituir `swl-ui/src/swlwm.c` pelo arquivo completo se
preferirem revisar direto.
