# AFAZERES — Lista-mestra de trabalho

Ordenado por prioridade dentro de cada status. "Desbloqueada" = dá pra
começar agora, sem depender de outra coisa. Owner = quem sugerimos pegar
(ajustar no DIARIO quando mudar).

## 🔓 DESBLOQUEADAS — prontas para começar

| # | Tarefa | Por quê agora | Owner sugerido |
|---|---|---|---|
| A1 | **Corrigir bugs críticos do TSWL** (corrupção de memória via `CSI r`; null deref do `xkb_ctx` no startup; CSI sem clamp → DoS). Ver `docs/revisao/2026-09-05-revisao-01.md` R-01/R-02/R-03 | CONCLUÍDA em 2026-09-08: Grok corrigiu (sessão no repo), orquestrador verificou com build + harness ASan/UBSan (antigo crashava, novo passa). R-01/R-02/R-03 → CORRIGIDO. | — |
| A2 | **Corrigir bugs médios do swl-ui** (hit-test de decoração com minimizada/montada; foco da taskbar após minimizar/fechar; `read_cpu_usage` com variável não inicializada; `mem_available` ausente). Ver R-04/R-05/R-06/R-07 | CONCLUÍDA em 2026-09-08: Grok corrigiu R-04/R-05/R-07 (sessão no repo) e R-06 (double-buffer do shm no TSWL — sessão `2026-09-08-grok-tswl-r06-double-buffer.md`); orquestrador verificou com build + harness e leitura. R-04/R-05/R-06/R-07 → CORRIGIDO. | — |
| B1 | **Ícones PNG do desktop caem no glifo vetorial** — caso encerrado no código atual (2026-09-08, Claude): `desktop.c` tem `resolve_icon_path()` + 13 PNGs em `swl-ui/assets/icons/` (ver sessão `2026-09-08-claude-assets-visuais.md`). | — | — |
| B2 | **Reset do cursor de resize** (`else → "default"` em `swlwm.c`) — presente no código atual (`process_cursor_motion`); bug do "gruda" permanece fechado desde 2026-09-07. Não re-remover. | — | — |
| B3 | **`fetch-deps.sh` gera bash/busybox do host (x86_64)** — num clone limpo em máquina 64-bit, `/bin/sh` nasce quebrado de novo (ver sessão 2026-09-07 §3). Precisa busybox i386 estático reproduzível (ex.: `gcc -m32` com checagem, ou binário pinado). | CONCLUÍDA em 2026-09-08: Grok refez o script (`-m32 -static` fora de host i386, `require_i386_cc` fail-fast, `assert_elf32` via readelf/file, `ensure_elf32_or_rebuild`, `--host=i386-pc-linux-gnu` no bash, flags i386 no busybox, validação final). Orquestrador verificou: `bash -n` OK, funções testadas isoladamente (require/assert/ensure nos 3 caminhos), build do swl-ui com o resto do repo OK. Nota: `ensure_elf32_or_rebuild` tem detalhe de locale (grep `Class:` vs readelf pt_BR `Classe:` + `elif` inalcançável) que causa rebuild redundante em sistema pt_BR — sem impacto no resultado (nenhum 64-bit chega ao rootfs). Correção futura de 1 linha (`LC_ALL=C` + `if` separado). Sessão no repo. | — |
| A3 | **Remover artefatos de build commitados do git** — SUPERADA em 2026-09-07: o histórico foi recomeçado limpo (só fontes; tag `main-arquivo` guarda o antigo com kernel/rootfs). Nada a fazer. | — | — |
| A4 | **Integrar a GUI ao boot real (DRM/KMS)** — CONCLUÍDA em 2026-09-07: swlwm como sessão principal no initramfs, verificado headless e pelo usuário. | — | — |
| A5 | **Readaptar janelas maximizadas quando o output redimensiona** | CONCLUÍDA em 2026-09-08: Grok extraiu `toplevel_apply_maximized_layout()` e reaplica em `output_request_state` + `server_new_output` (`maximized && !minimized`, `saved_geo` intacto). Orquestrador verificou: build OK sem warnings novos, B2 intacto, lógica conferida. Sessão no repo. | — |
| A6 | **Fullscreen real** (hoje só responde ao protocolo, sem lógica) | CONCLUÍDA em 2026-09-08: Grok entregou fullscreen + complemento do aviso (reflow no `server_new_output` estendido p/ fullscreen + re-raise do shell). Orquestrador verificou: só os 2 itens, build 11/11. Aviso marcado ATENDIDO. | — |
| A7 | **Correções de build/leveza + parser ANSI** (tswl: remover `-lm`/`-lrt` sem uso; swl-ui: `wayland-protocols`, fallback wlroots, `xdg-shell` via scanner; tswl: ESC cancela CSI, C1 ignorados) — ver R-09/R-10/R-11 | CONCLUÍDA em 2026-09-08: Grok fez R-09/R-10 (build) e R-11 (parser, sessão no repo; harness comportamental + regressão R-01/R-03 OK). **Solução definitiva do `.h` (lote Claude):** gerado via `wayland-scanner`, nada comitado. | — |
| C1 | **Resize unificado + extras de janela** — `resize_pending*` no frame/release; snap-maximizar no painel; margens anti-saída de tela; encaixe de ícones na grade; bandeja com leitura real (bateria/wifi/volume) | CONCLUÍDA em 2026-09-08: pacote `janelas-icones-bandeja` verificado (build OK, encaixe 4/4 sanitizer, base A5/A6/B2 intacta) e integrado. **Ressalva**: repor guarda R-13 (5 linhas) — ver `docs/revisao/2026-09-08-aviso-janelas-r13.md` (PENDENTE); sessão da entrega pendente. | — |
| C2 | **swlc: `extern` não emitido p/ builtins usados só dentro de `for`** — causa raiz achada (`collect_externs_stmt` sem `case S_FOR`); fix do Grok **verificado e aprovado** (suíte 53/53, jogo `for` roda). **Integração SUSPENSA por DEC-009**: linguagem é área ativa do Buffy (já construía isso "off") — contribuição guardada em `revisao-de-entrada_LOCAL/c2-files/`, entra quando ele concluir ou pedir ajuda. | Dono: Buffy. | Buffy |
| C3 | **swlc: sem leitura de teclado (jogos interativos impossíveis)** — builtins cobrem só saída/tempo/rand (`sema.c` + `swlrt.asm`); pedido do usuário testando o par/ímpar: quer digitar os números. Falta ex. `swl_read_i32` (ler linha do stdin → i32) no runtime + registro no compilador + exemplo. Área do Buffy (DEC-009). | Sem isso a linguagem só "fala", não "escuta". | Buffy |
| V1 | **Identidade visual do terminal (pacote shell)** — prompt `SWL:~$`, splash Neo, `swl_help`, `swl_clear`, spinner/progresso, instalação no rootfs, TSWL com `ENV` | CONCLUÍDA em 2026-09-08: Grok entregou (`shell-rc.sh`, `etc-profile`, rootfs, `ENV` no pty, `swlfetch` com fallback de arte); orquestrador testou funcional no dash + busybox ash (prompt, help, spinner, progresso, splash em tty) + sintaxe. Desdobramentos futuros: `swlfetch` nativo C/ASM, boot com padrão OK (ver IDEIAS). | — |
| T1 | **TSWL: cursor barra piscando (fatia de T4)** — barra 2–3px no lugar certo, blink no timer | CONCLUÍDA em 2026-09-09: Grok trocou bloco invertido por barra sobreposta; orquestrador provou em pixel (surface headless: barra presente, resto vazio, off limpo). Validação visual no QEMU pendente. | tswl |
| T2 | **TSWL: scroll por wheel + teclas (fatia de T4)** — `wl_pointer axis` → ±3 linhas; Shift+PgUp/Dn + KP + Shift+setas; discrete ignorado. | CONCLUÍDA em 2026-09-09: Grok; orquestrador verificou build, direção e ciclo de vida (não tinha subido antes por falta de aprovação — QEMU antigo sem o código). Validação no QEMU pendente. | tswl |
| T5 | **TSWL: seleção com mouse + copiar/colar** | CONCLUÍDA em 2026-09-09: pacote fix-t4-t5 (Grok) — dirty-row + barra T1 restaurada + seleção/highlight + forward decls; build limpo, T5/R-11/R-14 OK no sanitizer. | tswl |
| T3 | **TSWL/QEMU: glifo █ (U+2588) vira tofu** — causa-raiz: sem `fonts.conf` no rootfs a fonte nem carregava. Base integrada (fonts.conf + alias + cópia do host via `build-gui-rootfs.sh`, verificado). **Falta 1 linha**: lista de fontes no `render.c` (arquivo veio pré-T1; rebase mínimo) — ver `docs/revisao/2026-09-09-aviso-t3-fontline.md` (PENDENTE). | Grok |
| T4 | **TSWL terminal completo (dono a definir — alguém adota)** — cursor barra `|` piscando no lugar certo; mouse funcionando (clique + wheel/scroll); otimizado; texto reajusta ao resize; `swlfetch`/Neo auto-ajusta à largura; inclui T1+T2. Pedido do usuário testando no QEMU: "terminal completo". Progresso 2026-09-09: T1 (cursor), T2 (scroll), autofit do swlfetch v5 (3 layouts testados) prontos; falta o resto. | Escopo grande, uma IA só de preferência (DEC-009). | — |
| M1 | **Barra de menu por app (Arquivo, Editar, Ver, Configurar…)** — padrão clássico dentro de cada janela, como todo sistema tem; hoje só o painel global tem menu. Componente `apps/swlappkit/` + integração no TSWL feitas (orquestrador, preservando T2+R-14; build + suites ok). Falta validação no QEMU (clique/Alt). | Dono a definir | — |
| S1 | **App Configurações (Claude)** — casca 12 seções conforme especificado (só Sobre funcional; demais honestamente marcadas — por etapa). AGUARDANDO `apps/swlappkit/` (menubar M1) que falta na entrega — sem ela não compila. sysinfo parsers verificados (ASan ok, sistema real). | Claude |
| W1 | **Janela TSWL deforma no resize do output (larga e baixa)** — print QEMU 2026-09-10: após redimensionar a saída, janela fica larga e com 2 linhas + farelos na borda esquerda. Sem erro no log (só benignos). Suspeitos: reflow de maximizada (A5) vs janela normal, configure com dims erradas, decoração vs render. Reproduzir no QEMU redimensionando. | QEMU/teste. |
| W2 | **rootfs sem dados do libinput** — log QEMU: `/usr/share/libinput` ausente (device quirks não carregam). Empacotar data files no `build-gui-rootfs.sh` (mesmo padrão das fontes T3 / xkb). | rootfs. |
| S2 | **swlsysinfo: janela maior que a tela + CPU zerado + fantasma na borda** — app abre 640x400 fixo sem respeitar o configure (conteúdo cortado); CPU% tudo 0% (amostra inicial ou cálculo); possível cópia vazando na borda direita. App já rastreado (`apps/swlsysinfo/`); fix do primeiro quadro já feito pelo orquestrador. | Visto em print no aninhado. | — |
| A8 | **Atualizar `swl-ui/README.md`** (diz que menu e maximizar/minimizar "não existem", mas já estão implementados) — ver R-12 | CONCLUÍDA: R-12 CORRIGIDO desde 2026-09-05; README atual já lista menu/maximizar/minimizar como implementados e a seção de TODO reescrita (conferido em 2026-09-08). | — |

## 🚧 EM CURSO / AGUARDANDO

| # | Tarefa | Estado | Nota |
|---|---|---|---|
| — | `.gitignore` | Corrigido: foi subido como `gitignore` (sem ponto) e o orquestrador renomeou para `.gitignore` em 2026-09-05. | Falta A3 (`git rm --cached` dos já rastreados) |
| — | Deps externas | `userland/fetch-deps.sh` criado por outra IA (kernel + bash/busybox estáticos). | Não commitar a árvore do kernel no futuro |
| — | Documento mestre / estado | Admin edita (esta pasta + `docs/ai/*`) | Contínuo |
| — | Sessões de implementação 09-05 | Usuário ainda não subiu documentos de sessão do dia | A gente atualiza este arquivo quando subirem |
| — | R-06 (TSWL shm double-buffer) | CONCLUÍDA em 2026-09-08 — fechou o A2. Ver A2. | R-06 → CORRIGIDO |

## 🧭 PLANEJADAS (próximas fases — não bloqueadas por nada, só por ordem)

1. **Linguagem SWL / compilador `swlc`** (área ativa do FreeBuffy —
   DEC-009; orquestrador só verifica) — MVP–M7 ENTREGUE em
   2026-09-07 (notebook) e verificado pelo admin (46/46). Restos
   possíveis em `swl-compiler/ROADMAP-V1-REMAINING.md`.
2. **Restante do catálogo de apps** (SWLPad, file manager, SEBRE, config…)
   — hoje só placeholders no `desktop.c`; TSWL é o padrão de arquitetura.
3. Instalar/integrar o TSWL de verdade no catálogo (PATH + instalação),
   em vez do comando quebrado atual.
4. TSWL iterações: clipboard/seleção, cores 256, restore de alt-screen,
   scrollback não zerar no resize, modo application-cursor (DECCKM).
5. Testes de regressão — ainda não existe infra de teste no repo; primeiro
   passo razoável é unit test do parser ANSI (ele é o maior risco hoje).
6. Fases longas do roadmap: Electronics Studio, Game/Pixel/3D, browser.

## ⚠️ REGRA PARA QUEM PEGAR TAREFA

Ao abrir uma tarefa da lista: escreva a sessão em `docs/ai/sessions/`,
mexa **só** no escopo da tarefa, rode os testes/montagens relevantes e
marque no DIARIO (não edite este arquivo — quem move aqui é o admin).

---

Histórico de movimentação: este arquivo vive em `docs/orquestracao/` e é
atualizado pelos orquestradores. Conferir sempre o repo real (`git
status`, `git log`) antes de confiar na lista.