# DIARIO — Mudanças e atualizações

Registro diário das rodadas de orquestração. Entrada mais recente em
cima. **Conferir sempre o repo real** — este diário é um índice, não a
verdade de fonte.

## 2026-09-11 — OSC 52 clipboard integrado (admin)

Veredito padrão: BOM (decoder + handoff corretos); base ATUAL;
47/47 com e sem sanitizer; build 9/9. Aprovado pelo usuário e
integrado (com ida pro `main`).

## 2026-09-11 — CSI 3J + DECSTR integrados (admin)

Veredito padrão: BOM (lógica correta e útil); base ATUAL (só adições);
44/44 com e sem sanitizer; build ok. Aprovado pelo usuário e integrado.

## 2026-09-11 — Bracketed paste integrado (admin)

CSI ?2004 (cola sem interpretar metachars): base atual, 38/38 com e
sem sanitizer, build limpo. Aprovado pelo usuário e integrado.

## 2026-09-11 — W1 geometria reconciliado (admin)

`origin/notebook`: fix W1 (dims nos buffers shm, memset anti-farelo,
clamp de configure). FF limpo. Revisão do admin: causa-raiz endereçada
nos 3 pontos; build 9/9 + suite 35/35 aqui. Subido.

## 2026-09-10 — T5-word (rebase) integrado; entrada limpa (admin)

Rebase exato (só palavra, OSC intacto) verificado e integrado; aviso
t5word ATENDIDO. Entrada limpa do resolvido (restam: c2/Buffy,
swlpad/decisão, swl-about-neo/aviso, s2).

## 2026-09-11 — W1-geom integrado; entrada limpa (admin)

Fix exato do aviso (campos buf + set + pick-guard + memset + mínimos)
verificado com build limpo e integrado; aviso ATENDIDO. W1 agora
aguarda validação no QEMU. Entrada limpa do resolvido (restam: c2,
s2, about, swlpad + backup W1 do usuário).

## 2026-09-11 — Ctrl+Shift+A integrado, aviso atendido (admin)

Fix exato (remove `pressed`) verificado com build 9/9 e integrado;
aviso selectall ATENDIDO. T5 seleção agora completa (arrasto, palavra,
linha, tudo + atalhos).

## 2026-09-11 — Visual bell integrado (admin)

Pacote completo (base atual conferida marcador por marcador — sem o
problema de base velha): BEL → flash 120ms, OSC-BEL não pisca.
Verificado 4/4 sanitizer + suíte 35/35. Aviso ATENDIDO.

## 2026-09-10 — T5 triplo-clique integrado (admin)

Rebase limpo (só adições) verificado: build OK, 3/3 sanitizer.
Atenção: `main.c` do W1 que estava na pasta (cópia de teste do
usuário) foi guardada em `/tmp` antes de sobrescrever. T5 → inclui
duplo + triplo.

## 2026-09-10 — Visão do novo visual das janelas (usuário) → V2 (admin)

Usuário definiu o redesign: cantos levemente arredondados, barra fina,
menubar oculta com botão-seta animado, vidro fosco sutil, toggles
Apple-like, botões redondos com cores atuais, sem perder leveza.
Registrado como V2 (dono a definir).

## 2026-09-10 — Truecolor (rebase) integrado; aviso t5-word (admin)

Rebase somando (truecolor + OSC) verificado 30/30 e integrado; aviso
truecolor ATENDIDO. T5-word (duplo clique) com lógica aprovada mas
base sem OSC — devolvido com aviso preciso
(`2026-09-10-aviso-t5word.md`).

## 2026-09-09 — T3 + R-13 reconciliados (admin)

`origin/notebook`: linha DejaVu no `render.c` (T3, a que faltava) +
guard R-13 no `swlwm.c`, sessões e 2 avisos novos (t4-dirty e
tswl-catalog, ambos p/ outros — rebase necessário). Merge limpo.
Subido.

## 2026-09-09 — KVM resolve a fluidez (admin)

`make run-gui` com `-accel kvm` (fallback TCG): usuário confirma
boot e uso "voando". Travamentos eram emulação pura, não o sistema.
QEMU 8.2 rejeita a sintaxe `kvm:tcg` — Makefile usa `||` com fallback.

## 2026-09-08 — Tarefas T4/M1/S1 (pedidos do usuário no QEMU) (admin)

Usuário testando no sistema pediu: terminal completo (T4, absorve T1+T2:
cursor barra, mouse, otimização, resize de texto, neofetch auto-fit),
barra de menu por app estilo clássico (M1) e app Configurações completo
(S1: wifi/rede, bluetooth, acessibilidade, apps, personalizar,
teclado+mouse, display, painel, notificações, sobre, idiomas, disco).
Donos a definir.

## 2026-09-08 — alt-screen reconciliado (admin)

`origin/notebook`: alt-screen real `?1049` (Grok) + 4 testes.
Merge limpo, sem conflitos. Verificação do admin: suite rodada
aqui → 19 passed. Subido.

## 2026-09-08 — Reconciliação no main + app novo avistado (admin)

Outra máquina fez merge `901b3667d` (lote notebook: Neo, T4/M1/S1) no
`main`; `main` local em fast-forward, tudo sincronizado. Doc de
sugestão do Claude pendente de envio (usuário avisa depois).
Novo na pasta: `apps/swlsysinfo/` (~1000 linhas, monitor de sistema
Wayland direto, com `build/` — ainda untracked, testes do usuário;
revisão formal quando pedirem).

## 2026-09-09 — T4-autofit + base T3 integrados; aviso de 1 linha (admin)

`swlfetch` v5 (3 layouts: lado, estreito, pilha — todos testados com
`COLUMNS` simulado + sintaxe) integrado. Base do T3
(`build-gui-rootfs.sh`: `fonts.conf` com aliases + cópia do host;
XML validado, sintaxe OK) integrada. `render.c` do T3 NÃO entrou
(viria pré-T1 e apagaria a barra): falta só a linha `TSWL_FONT` —
aviso em `docs/revisao/2026-09-09-aviso-t3-fontline.md` (PENDENTE).
Aprovado pelo usuário.

## 2026-09-09 — T2 (scroll) integrada; T2 nunca tinha subido (admin)

Usuário testou scroll no QEMU e não rolava — porque o T2 aguardava
aprovação e o disco foi construído sem o código (confirmado: 0 refs
no repo; compositor repassa axis, ok). Aprovado agora e integrado:
`wl_pointer` + teclas KP/shift. T2 → CONCLUÍDA (validação no QEMU
pendente; reconstruir disco com o código novo).

## 2026-09-09 — C1 duplicado recusado (conta Claude errada) (admin)

Outra conta do Claude refez o C1 (`files/swlwm.c`) a partir do aviso
antigo — mas o C1 já está integrado COM MAIS (snap, margens, launcher,
icon-snap). O arquivo é bom (R-13/A5 intactos, `apply_pending_resize`
bem fatorado), porém redundante e menor: integrar apagaria o resto.
RECUSADO sem aviso (nada a corrigir, só não é preciso). Lição p/
usuário nomear as contas.

## 2026-09-09 — Catalog integrado; render do rebase rejeitado (admin)

Pacote `rebase-avisos-files/`: `desktop.c` (só `/bin`), `swlwm.c` (só
helper + 3 usos) e `build-gui-rootfs.sh` (só checagem + PATH) exatos —
build 12/12, sintaxe OK, integrados; aviso-tswl-catalog ATENDIDO. O
`render.c` do mesmo pacote veio de outra linhagem (menubar
inexistente, sem T1, encoding quebrado) — rejeitado com aviso novo
(`aviso-render-linhagem.md`); t4-dirty segue pendente.

## 2026-09-09 — Correção de processo: fontline entrou sem rito (admin)

Auditoria achou que a linha TSWL_FONT entrou no repo via varredura de
`git add -A` dentro de commit rotulado "docs" (9edfe33ab) — sem o rito
verificar→reportar→aprovar→integrar. Conteúdo auditado depois:
idêntico ao arquivo verificado (T1 intacto, build OK). Sem dano, mas
fica a lição: `git add -A` só com `git status` conferido antes, e
commit de docs nunca carrega código. Aviso T3 marcado ATENDIDO;
pastas fontline (1) e (2) removidas da entrada (resolvidas).

## 2026-09-10 — swlconfig (S1 casca) integrado (admin)

Bloqueador resolvido (`apps/swlappkit/` no repo): entrega do Claude
re-verificada e integrada — `apps/swlconfig/` (casca 12 seções, Sobre
funcional, menu Arquivo/Ajuda via swlappkit), screenshots, entrada de
diário adaptada. Verificação no repo: build 10/10 + sysinfo 8/8. S1 →
CONCLUÍDA (casca); seções seguintes por etapa. (É avanço sim: quarto
app real no catálogo.)

## 2026-09-10 — OSC title (rebase) integrado, aviso atendido (admin)

Rebase exato (só adições do aviso) verificado: suíte 27/27 com e sem
sanitizer + build 9/9 (rodados no repo). Aprovado pelo usuário e
integrado: shell pode titular a janela (`ESC]0;texto`).

## 2026-09-10 — TSWL 256 cores integrado (admin)

Entrega do Grok (`term.c` SGR 38;5/48;5 + `render.c` color_256 + 3
asserts): base compatível, mapeamento xterm correto, build 9/9,
suíte 22/22 com e sem sanitizer (rodada no repo). Aprovado pelo
usuário e integrado.

## 2026-09-09 — Pacote fix-t4-t5 integrado; mea-culpa do orquestrador (admin)

Rejeição anterior revertida: o veredito usou árvore velha (menubar já
estava no `main` via outra máquina). Pacote re-verificado contra a
árvore atual: build limpo, T5/R-11/R-14 OK sanitizer. Integrado:
dirty-row + **barra T1 restaurada** + seleção. Achado no caminho: a
integração do menubar apagou a barra do T1 no `main` sem aviso
(regressão silenciosa — coberta por este pacote). Lição: fetch + diff
contra o `main` atual antes de todo veredito. T5 CONCLUÍDA; avisos
t4-dirty e t5-build ATENDIDOS.

## 2026-09-09 — T1 (cursor barra) testada em pixel e integrada (admin)

Entrega do Grok (`render.c` + sessão): barra 2–3px sobreposta em vez
de bloco invertido. Verificação além do build: harness headless que
desenha e lê pixels (barra presente, resto da célula vazio, `off`
limpo — blink ok). Aprovado pelo usuário (condicionado ao teste) e
integrado: T1 → CONCLUÍDA (visual no QEMU pendente).

## 2026-09-08 — Pacote janelas-icones-bandeja integrado, C1 concluída (admin)

Entrega sem sessão (`janelas-icones-bandeja/`: swlwm+desktop+taskbar+header):
resize unificado, snap-maximizar, margens, encaixe de ícones, bandeja
real. Verificação: build OK sem warnings novos; encaixe 4/4 sanitizer;
leitores da bandeja conferidos (fallbacks); base A5/A6/B2 intacta, só
R-13 a repor (aviso novo). Aprovado pelo usuário e integrado: C1 →
CONCLUÍDA; aviso resize antigo SUPERADO. Sessão da entrega pendente
(autor gera). Caixa de entrada limpa do já-integrado (resta c2 do
Buffy + swlpad aguardando decisão).

## 2026-09-08 — Alt-screen real no TSWL (admin)

Entrega do Grok: buffer `main_save` (`?1049h` salva+limpa, `?1049l`
restaura; scroll em alt não alimenta histórico; resize migra junto;
entradas/saídas repetidas idempotentes). Verificação: alloc/free
tratados, lógica conferida, suíte 19/19 com e sem sanitizer (rodada
no repo). Aprovado pelo usuário e integrado.

## 2026-09-08 — Scrollback sobrevive ao resize (admin)

Entrega do Grok (`term.c` + 2 asserts): ring de histórico migra no
`tswl_term_resize` (truncate/pad por coluna, ordem preservada,
`offset` clampado) em vez de zerar. Verificação: fórmula do anel igual
à da leitura; suíte 15/15 com e sem sanitizer (rodada no repo).
Aprovado pelo usuário e integrado.

## 2026-09-08 — V1 (shell identity) integrada e concluída (admin)

Entrega do Grok em `revisao-de-entrada_LOCAL/v1-shell-files/`:
`shell-rc.sh` + `etc-profile` (novos), `build-rootfs.sh` (instala),
`swlfetch` (fallback de arte), `pty.c` (`ENV` no filho). Verificação
funcional de verdade: dash + busybox ash (prompt, help colorido,
spinner, progresso, splash em tty via `script`), sintaxe `sh -n`,
`setenv` no lugar certo sem overwrite. Aprovado pelo usuário e
integrado: V1 → CONCLUÍDA (desdobramentos: nativo C/ASM, boot OK).

## 2026-09-08 — parser unit tests + swlfetch reconciliados (admin)

`origin/notebook`: testes unitários do parser tswl (13 casos, inclui
R-14) + `userland/swlfetch` + `neo-face.txt` (V1 identidade visual).
Merge limpo. Verificação do admin: suite rodada aqui → 13 passed;
`bash -n` no swlfetch OK. Subido.

## 2026-09-08 — swlfetch + Neo no repo, tarefa V1 (admin)

A pedido do usuário: `userland/swlfetch` (infos à esquerda, Neo à
direita, cores via `printf`, coluna alinhada) + `userland/neo-face.txt`
(arte ANSI 52 col do mascote) commitados; testado de outro CWD (arte
resolve relativo ao script). Tarefa V1 criada (splash, prompt, help,
versão nativa C/ASM). C1/C2/C3 já estavam no AFAZERES — confirmado ao
usuário.

## 2026-09-08 — B3 locale integrado + dono da linguagem explicitado (admin)

Fix do Grok p/ ressalva do B3 (`LC_ALL=C` + grep `Class(e)?:`) em
`revisao-de-entrada_LOCAL/b3-locale-files.tar.gz`: diff mínimo (2
trechos), funções retestadas isoladamente (caso que falhava agora
reutiliza), sintaxe OK. Aprovado pelo usuário e integrado.

Docs: a pedido do usuário, explicitado que **a linguagem é área ativa
do FreeBuffy** (`PROJECT_STATE.md` seção swlc + `AFAZERES.md` fase 1 +
C2/C3 com dono; DEC-009 já proibia patch externo). Ajuste fino:
limitação `for sem var` removida (v2 já tem) + C3 listado.

## 2026-09-08 — Testes unitários do parser TSWL integrados (admin)

Entrega do Grok (`apps/tswl/tests/` + sessão): 13 asserts (básicos +
R-01/R-03/R-11/R-14) + `run_parser_tests.sh` (sem Wayland/Cairo, modo
sanitizer via `TSWL_SANITIZE=1`). Verificado no repo: 13/13 com e sem
sanitizer. Item do IDEIAS marcado ATENDIDO.

## 2026-09-08 — R-13 integrada, catálogo R encerrado (admin)

Entrega do Grok em `revisao-de-entrada_LOCAL/r13-files/` (`swlwm.c` +
sessão): guarda NULL de 5 linhas em `desktop_toplevel_at`. Verificação:
diff só a guarda; ambos os chamadores NULL-safe; build OK sem warnings
novos. Aprovado pelo usuário e integrado: R-13 → CORRIGIDO.
**R-01 a R-14 todos encerrados.**

## 2026-09-08 — R-14 reconciliado (admin)

`origin/notebook`: DECCKM + F1-F12 (Grok, com o rebase pedido no
aviso — ver sessão `2026-09-08-grok-r14-decckm-rebase.md`). Merge
limpo, sem conflitos. Revisão do admin: sequências F1–F12 corretas
(xterm), `app_cursor` plugado no `keysym_to_seq`. Subido.

## 2026-09-08 — R-14 (rebase) integrada, aviso atendido (admin)

Rebase do Grok em `revisao-de-entrada_LOCAL/r14-rebase-files/`:
diff com só os 3 acréscimos sobre o `term.c` com R-11 (verificado);
`main.c`/`term.h` aditivos. Build OK; harness combinado 8/8
(DECCKM + R-11 + R-01) limpo no sanitizer. Aprovado pelo usuário e
integrado: R-14 → CORRIGIDO, aviso marcado ATENDIDO. Resta R-13 como
único R aberto.

## 2026-09-08 — R-11 integrada, A7 concluída (admin)

Entrega do Grok em `revisao-de-entrada_LOCAL/r11-files/` (`term.c` +
sessão). Verificação no código: ESC em ST_CSI cancela antes de tudo
(reentrada limpa via memset — sem stale); C1 antes do caminho UTF-8.
Harness ASan/UBSan comportamental: cancel sobe 1, `0x9B` com/sem
params, C1 sem glyph/cursor; regressão R-01/R-03 limpa; build sem
warnings. Aprovado pelo usuário e integrado: R-11 → CORRIGIDO, A7
CONCLUÍDA (último aberto dela).

## 2026-09-08 — Primeiro programa SWL rodando na máquina do usuário (admin)

Jogo par/ímpar (`swl-compiler/examples/parimpar-demo.swl`, modo demo com
`while` por causa do C2) compilado e executado pelo usuário na própria
máquina: 6 rodadas, 3-3, campeão PC (desempate do `else`). Primeira
validação ponta a ponta da linguagem fora do sandbox do orquestrador.

## 2026-09-08 — lote do notebook reconciliado (admin)

Quatro commits (`0ebcbd5` A7 parcial, `cec64cb4` A6 fullscreen,
`cb8d42d` A6 conclusão + xdg-shell restaurado, `da21340` lote Claude:
min 260x180, xdg via scanner, taskbar, wallpaper novo + swlcat) +
branch `notebook` criada (= main). WIP local não-aprovado
(term.c/desktop.c/swlwm.c) descartado por ordem do usuário (backup em
/tmp); B2 continua valendo pelo código do repo (reset presente).
AFAZERES: A6 concluída.

## 2026-09-08 — Lote Claude (min 260, build scanner, taskbar) + aviso resize (admin)

Entrega do Claude (`revisao-de-entrada_LOCAL/`, sessão
`2026-09-08-claude-resize-taskbar-buildfix.md`). A caixa tinha 3
conjuntos; após diffs contra o HEAD, separação feita:

- **Integrados** (testados em `/tmp`, build 14/14): `theme.h` 260×180
  (tamanho confirmado pelo usuário), `meson.build` com geração do
  `xdg-shell-protocol` via `wayland-scanner` (solução definitiva pro
  build quebrado — `.h` comitado removido de novo, agora com geração;
  README com deps de build atualizadas), `taskbar.c` com divisórias +
  traço de foco (só desenho; visual a confirmar pelo usuário em
  sessão gráfica).
- **Não subiu**: resize unificado (`resize_pending*`) — arquivo em base
  pré-A5/A6, subir apagaria fullscreen/maximizadas. Virou aviso em
  `docs/revisao/2026-09-08-aviso-claude-resize.md` (PENDENTE, com pedido
  de rebase + teste interativo).
- Lição de processo (sugerida na sessão e aprendida hoje no A7):
  consolidação entre cópias exige build limpo do zero antes de subir.

## 2026-09-08 — A6 concluída + conserto do build A7 (admin)

**A6 complemento** (Grok, resposta ao aviso): diff com exatamente os 2
itens pedidos (reflow no `server_new_output` estendido p/ fullscreen +
re-raise do shell ao cancelar fullscreen via maximize), build 11/11 sem
warnings novos. Integrado: A6 → CONCLUÍDA, aviso marcado ATENDIDO.

**Conserto A7** (erro da revisão do orquestrador): a deleção do par
`xdg-shell-protocol` quebrou o build — o header do wlroots faz
`#include "xdg-shell-protocol.h"` (só o `.c` era morto). O teste do A7
passou porque o `.h` ainda estava no disco na hora. `.h` restaurado do
histórico (`git show`), `.c` segue deletado; build 11/11 confirmado
antes de subir. Lição registrada: testar deleção deletando de verdade
(em `/tmp` limpo).

## 2026-09-08 — A7 parcial (R-09/R-10) integrada (admin)

Entrega do Grok em `revisao-de-entrada_LOCAL/a7-files/` (2 `meson.build`
+ sessão + `DELETE_THESE.txt`). Verificação no código: tswl sem
`<math.h>` (só `clock_gettime`, libc); `wayland_protos` declarada e
nunca usada no meson do swl-ui; nenhum fonte inclui
`xdg-shell-protocol.h`; swl-ui usa `M_PI`/`sin`/`cos` (math mantido,
correto). Builds em `/tmp` com os arquivos novos: tswl OK sem
`libm`/`librt` diretos no `readelf -d`; swl-ui 11/11 OK com fallback
0.18 → genérico (achou 0.17.1). Arquivos mortos deletados do git
(`git rm`). R-11 ficou de fora com razão (bug do parser, separado) e
segue aberto. Aprovado pelo usuário e integrado: R-09/R-10 →
CORRIGIDO, A7 PARCIAL.

## 2026-09-08 — A6 (fullscreen) integrada com ressalva + aviso ao Grok (admin)

Entrega do Grok em `revisao-de-entrada_LOCAL/a6-files/` (`swlwm.c` +
sessão). Verificação no código: lógica do fullscreen correta (flag,
layout tela cheia sem titlebar, salva/restaura, exclusão mútua com
maximize, guards de resize/decor, commit não desfaz posição); build
`meson`+`ninja` OK sem warnings novos. Aprovada pelo usuário e
integrada — **com um "mas"**: o arquivo veio de base pré-A5 e não traz
o reflow do `server_new_output` (mergeado no A5); subir direto
regrediria esse caminho. Para não travar o fullscreen, subiu assim
mesmo e ficou registrado o complemento pendente em
`docs/revisao/2026-09-08-aviso-grok-a6.md` (repor bloco estendido p/
fullscreen + detalhe opcional de z-order). A6 → PARCIAL.

## 2026-09-08 — A5 + B3 integradas (admin)

Duas frentes do Grok em `revisao-de-entrada_LOCAL/`, verificadas e
aprovadas pelo usuário:

**A5** (maximizadas no resize do output): `toplevel_apply_maximized_layout()`
extraída e reaplicada em `output_request_state` + `server_new_output`
(`maximized && !minimized`, `saved_geo` intacto). Build `meson`+`ninja`
OK sem warnings novos; B2 intacto. A5 → CONCLUÍDA.

**B3** (`fetch-deps.sh` i386): script refeito (`-m32 -static`,
`require_i386_cc`, `assert_elf32`, `ensure_elf32_or_rebuild`,
`--host=i386-pc-linux-gnu`, flags i386 no busybox, validação final).
Verificação: `bash -n` OK; funções testadas isoladamente (require OK,
assert aceita ELF32 e rejeita 64-bit, ensure nos 3 caminhos); veredito:
**resolve o B3** — nenhum 64-bit chega ao rootfs em nenhum caminho.
Ressalva registrada no AFAZERES: detalhe de locale no `ensure`
(readelf pt_BR imprime `Classe:`, grep espera `Class:`, `elif`
inalcançável) causa rebuild redundante em sistema pt_BR, sem impacto
no resultado; correção futura de 1 linha. B3 → CONCLUÍDA.

## 2026-09-08 — Revisão completa: builds + testes (admin)

Rodada de verificação geral do sistema (builds em `/tmp`, repo intacto):

- `apps/tswl`: `meson`+`ninja` OK, **0 warnings**.
- `apps/swlpad`: `meson`+`ninja` OK, **0 warnings**.
- `swl-ui`: `meson`+`ninja` OK (12/12); 16 warnings — 15×
  `-Wunused-parameter` (benignos, listeners Wayland) + 1×
  `-Wformat-truncation` em `panel.c:204` (`snprintf` do label MEM,
  cosmético: só estouraria com valores absurdos de RAM).
- `swl-compiler`: `make all rt` OK, 0 warnings; `make test` =
  **51 passed, 0 failed** (22 exemplos + 29 rejeições — bate com o
  README). Exemplos cobrem v2 (`for_loop.swl`, `hex_demo.swl`, etc.).
- Harness adversarial do parser TSWL (ASan+UBSan, 21 casos: CSI `r`
  malicioso, 1500 params, SGR/cursor absurdos, resize, OSC/DCS lixo,
  feed de 100KB, UTF-8 quebrado): **limpo, sem sanitizer**.
- Scripts (`fetch-deps.sh`, `build-*.sh`, `runtests.sh`): `bash -n` OK.
- `tools/gen-cursors`: compila com a receita do `generate.sh`
  (`gcc -O2`); com `-std=c11` estrito reclama de `M_PI` — falso alarme,
  receita oficial funciona.

Observações (sem ação, aguardando decisão do usuário):

1. `tswl_term_resize` não valida `cols/rows < 1`. Fora de contrato e
   inalcançável em produção (`cols/rows_for` prendem em ≥1 e o
   `toplevel_configure` exige `w>0 && h>0`; overflow p/ wrap exigiria
   janela > 2^33 px). Endurecimento de 1 linha possível, mas não é bug
   vivo — não virou achado formal.
2. Não testável aqui: `disk.img` completo (precisa da árvore do kernel
   + rootfs, externos), GUI em runtime (precisa de display/compositor)
   e `fetch-deps.sh` de ponta a ponta (baixaria toolchain; B3 segue
   aberto e é o único item desse tipo).

Docs: `AFAZERES.md`/`PROJECT_STATE.md` conferidos contra o repo — sem
divergência restante (abertos reais: A5, A6, A7/R-09..11, R-13, R-14,
B3). Nenhuma doc precisou de correção nesta rodada além deste registro.

## 2026-09-08 — A2 completa: R-06 (TSWL double-buffer) corrigida e integrada (admin)

Usuário colocou em `revisao-de-entrada_LOCAL/tswl-R06-files/` a correção
da **R-06** pelo Grok (`apps/tswl/src/main.c` + sessão). Orquestrador
verificou: diff só no `main.c` do tswl (base `baae84b`, pós A1/A2);
build `meson`+`ninja` OK; A1 preservado (`xkb_ctx` guard intacto);
lógica do double-buffer conferida (`busy`/`stale`, recriação no
`release`, nunca destrói buffer em uso, `need_redraw` mantido quando
sem slot livre); zero refs ao antigo buffer único. Aprovado pelo
usuário e integrado: R-06 → CORRIGIDO, A2 COMPLETA.

## 2026-09-08 — Revisão geral + docs sincronizadas (admin)

Revisão geral do estado real (repo é a fonte de verdade) e atualização
das docs que estavam defasadas — nenhum código alterado:

- `PROJECT_STATE.md`: "Revisões/QA" reflete R-01..R-07 e R-12
  corrigidos; "Em investigação" passa B1 (ícones PNG) e B2 (reset do
  cursor) para **encerrados no código atual**; árvore do swl-ui ganhou
  `context_menu.c/h`; próximos passos GUI atualizados (ícones saem).
- `AFAZERES.md`: B1/B2 → encerrados no código; A8 → CONCLUÍDA (R-12
  CORRIGIDO desde 09-05, README conferido). R-06 entrou como em curso
  (Grok) — desde então a R-06 subiu e foi integrada (ver entrada A2 completa acima).

## 2026-09-08 — A2 (swl-ui bugs médios) parcial corrigida e verificada (admin)

Usuário colocou em `revisao-de-entrada_LOCAL/swl-ui-A2-files/` a correção
da **A2** pelo Grok (`swlwm.c` + `panel.c` + sessão). Orquestrador
verificou: base == `HEAD` atual (nada novo tocou swl-ui desde então),
build `meson`+`ninja` (wlroots 0.17.1) OK, lógica R-04 (hit-test pula
minimizadas), R-05 (taskbar usa foco real do teclado; minimizar a focada
passa foco à próxima) e R-07 (contadores zerados + fallback de memória
com clamp) conferidas — harness contra o `/proc` real e teste do fallback
passaram. Aprovado pelo usuário (só sobe o que ajuda) e integrado:
R-04/R-05/R-07 → CORRIGIDO; A2 parcial — **R-06 (TSWL shm) segue em
aberto**.

## 2026-09-08 — A1 (TSWL bugs críticos) corrigida e verificada (admin)

Usuário colocou em `revisao-de-entrada_LOCAL/tswl-A1-files/` a correção
da **A1** pelo Grok (`term.c` + `main.c` + sessão). Orquestrador:
compilou o tswl com os fontes novos (`meson`+`ninja`, OK) e rodou um
harness de parser ANSI com ASan/UBSan — o `term.c` antigo reproduzia o
crash R-01 (`negative-size-param` no `memmove`), o novo passa limpo em
todas as sequências maliciosas (R-01/R-03) sem UB. R-02 confirmado por
leitura (xkb_ctx antes do primeiro roundtrip + guard). Aprovado pelo
usuário e integrado: R-01/R-02/R-03 → CORRIGIDO, A1 concluída.

## 2026-09-08 — swlc v1 reconciliado + verificado (admin)

Notebook subiu `swl-compiler/` (MVP–M7) + sessão de fechamento.
Admin: FF limpo, WIP local intacto, `make -C swl-compiler test`
rodado aqui → **46 passed, 0 failed** (entrega confirmada de forma
independente). Docs: `PROJECT_STATE` (swlc) + `AFAZERES` (fase 1
entregue, ver `ROADMAP-V1-REMAINING.md` pros restos).

## 2026-09-08 — Notebook reconciliado (admin)

Usuário subiu via web o trabalho do notebook (`origin/main` 93071b7 +
branch `origin/arquivos`): wallpaper novo (1,2 MB), `swlcat.png`,
`swl_desktop_draw_glyph_for_title()` (ícones da taskbar por keyword),
`SWL_MIN_WINDOW_*` + throttle de resize (`resize_deco_dirty`, redraw
por frame). FF do `main` local + merge do WIP local (term.c, desktop.c,
swlwm.c) SEM conflitos — regiões disjuntas. B2 segue aberto (reset do
cursor ainda fora). `swlcat.png` sem referência (asset futuro).

## 2026-09-07 — Rodada 2 (admin)

Pasta oficial apagada pelo usuário no meio do dia; reconstruída dos
snapshots + histórico, com 4 fixes de boot no caminho (initrd no topo
da RAM, `/bin/sh`→busybox i386, xkb-data+`/dev/shm`, `devpts` pro
tswl). GUI verificada ponta a ponta (headless+usuário) e subida:
`main` recomeçado limpo (só fontes), tag `main-arquivo` guarda o
histórico antigo. Ver `docs/ai/sessions/2026-09-07-recuperacao-pasta-boot-gui.md`.

Docs atualizadas nesta rodada: `PROJECT_STATE.md` (estado real),
`AFAZERES.md` (A3/A4 concluídas; novas B1/B2/B3), `swl-ui/README.md`
(A8: menu/maximizar/PNG não são mais TODO).

Em curso por outras IAs (não mexer no código delas): PNG dos ícones
(B1), resize do terminal (term.c), compat wlroots (`WLR_BUTTON_`
→ `WL_POINTER_BUTTON_STATE_`). ATENÇÃO B2: removeram o reset do
cursor de resize (`else → "default"` em `swlwm.c`) — reabre bug
confirmado pelo usuário; reaplicar antes de fechar.

Nenhum código alterado pelo admin nesta rodada (só docs).

## 2026-09-05 — Rodada 1 (admin)

Estado do repo conferido na prática: clone íntegro (95.222 arquivos),
index do git reconstruído após checkout interrompido, `git status`
limpo.

Entregas desta rodada:

- **`docs/orquestracao/`** criado (README, AFAZERES, IDEIAS, DIARIO) —
  área exclusiva dos orquestradores (eu + GPT).
- **`docs/revisao/2026-09-05-revisao-01.md`** criado — 12 achados da
  revisão de `swl-ui` + `apps/tswl`, com responsáveis e pedido de
  correção.
- **`docs/ai/PROJECT_STATE.md`** atualizado — novas seções:
  "Repositório (higiene)" e "Revisões / QA", com links para a revisão.
- **`docs/ai/DECISIONS.md`** não precisou de mudança nesta rodada.

Descobertas que viraram tarefas em `AFAZERES.md`:

- A1: 3 bugs críticos no TSWL (parser ANSI) — R-01 a R-03.
- A2: 4 bugs médios no swl-ui — R-04 a R-07.
- A3: limpeza manual de artefatos já commitados (`.gitignore` foi criado
  por outra IA e subido, mas não remove o que já está rastreado).
- A4: integração GUI ao boot real (DRM/KMS) — grande, desbloqueada.
- A5/A6: bugs conhecidos da GUI (maximize/resize, fullscreen).
- A7: leveza de build (dependências mortas, ordem wlroots, arquivo morto).
- A8: `swl-ui/README.md` desatualizado.

Processo: houve sessão do usuário em paralelo (ids não subidos ainda).
Quando subirem, atualizar `AFAZERES.md` se algo mudar de status.

Nenhum código foi alterado nesta rodada (regra: orquestrador não mexe em
código por padrão).