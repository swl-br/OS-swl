# DECISIONS — SWL OS

## DEC-001 — Kernel Linux como base

Status: ACCEPTED

Decisão: usar o kernel Linux (não escrever kernel próprio do zero).

Motivo: aproveitar driver/hardware/rede/filesystem já maduros, sem
reinventar o que já funciona bem.

Consequência: userland, GUI, shell, linguagem, apps são onde a
identidade própria do SWL OS realmente se constrói.

---

## DEC-002 — Bootloader próprio, sem GRUB

Status: ACCEPTED

Decisão: bootloader escrito do zero em Assembly x86 (16-bit → carrega
kernel em memória alta → entrega controle pro setup de 16-bit do
próprio Linux, que faz sua própria transição pra modo protegido).

Motivo: identidade própria do sistema, controle total do processo de
boot, e porque o protocolo de boot de 16-bit do Linux já resolve
sozinho detalhes delicados (como o mapa de memória E820 via BIOS) que
seria arriscado reimplementar na mão.

Consequência: já testado e funcional (ver PROJECT_STATE.md).

---

## DEC-003 — wlroots como base da interface gráfica

Status: ACCEPTED

Decisão: usar wlroots (biblioteca de compositor Wayland) como
alicerce, partindo do exemplo `tinywl` e evoluindo pra arquitetura
própria modular (`swl-ui/`).

Motivo: reescrever um compositor gráfico do zero (drivers de vídeo,
protocolo Wayland, renderização) é trabalho de anos; wlroots resolve
isso de forma madura e testada, deixando a identidade visual e
comportamento da interface 100% por nossa conta.

Consequência: interface roda sobre Wayland, não X11. Compatibilidade
com apps X11 legados (como `xterm`) precisaria de uma camada XWayland
separada — decisão de adicionar isso ou não ainda não foi tomada.

---

## DEC-004 — Arquitetura modular do compositor (swl-ui)

Status: ACCEPTED

Decisão: separar o compositor em arquivos por responsabilidade
(background, panel, taskbar, decorations, desktop/ícones,
swl_buffer para desenho via cairo) em vez de um único arquivo
monolítico.

Motivo: manutenção e colaboração entre múltiplas IAs/sessões ficam
muito mais seguras assim — evita o tipo de corrupção acidental que
ocorreu numa versão anterior monolítica (`swlwm.c` único, editado via
comandos de texto sucessivos, que acabou corrompido e precisou ser
descartado).

Consequência: qualquer IA trabalhando na GUI deve manter essa
separação, não voltar a concentrar tudo num arquivo só.

---

## DEC-005 — Nomenclatura de diretórios de assets em inglês

Status: ACCEPTED

Decisão: diretórios de assets dentro de `swl-ui/` (ícones, wallpaper,
etc.) usam nomes em inglês, batendo exatamente com os caminhos
hardcoded no código (`assets/icons/`, não `assets/icones/`;
`assets/wallpaper.png` na raiz de `assets/`, não dentro de uma
subpasta).

Motivo: um bug real aconteceu por causa disso — `desktop.c` procurava
ícones em `assets/icons/` mas a pasta real no repo era
`assets/icones/` (português), e o wallpaper era procurado em
`assets/wallpaper.png` mas estava em `assets/wallpapers/wallpaper.png`.
Os dois bugs eram silenciosos (sem crash, sem log de erro — só caía no
fallback vetorial/procedural sem avisar), e por isso passaram
despercebidos por várias sessões que testaram visualmente mas focadas
em outras coisas (resize, drag, maximizar). Corrigido renomeando as
pastas (2026-09-04, Claude — sessão "fix-caminhos-assets").

Consequência: qualquer IA que adicionar um ícone novo ou trocar o
wallpaper padrão usa `swl-ui/assets/icons/` (inglês) e
`swl-ui/assets/wallpaper.png` (raiz). A pasta `assets/wallpapers/`
(plural) guarda só as variações-fonte em SVG, não é lida em runtime.

---

## DEC-006 — Compatibilidade wlroots 0.17 e 0.18 via guard de compilação

Status: ACCEPTED

Decisão: o compositor precisa compilar e funcionar tanto contra
wlroots 0.17.1 (máquina de desenvolvimento Xubuntu) quanto 0.18.2
(máquina de desenvolvimento Mint), já que as duas são usadas em
paralelo por sessões/IAs diferentes. Onde a API do wlroots difere
entre as duas versões, o código usa um guard de compilação
(`#define SWL_WLR_0_18`, derivado de `WLR_VERSION_NUM` de
`wlr/version.h`, em `swlwm.c`) com os dois caminhos lado a lado, em vez
de escolher uma versão só e quebrar a outra máquina.

Motivo: descoberto na prática (2026-09-04, OpenHands) que no wlroots
0.18 nenhum cliente Wayland conseguia mapear janela — faltava agendar
o configure inicial explicitamente (0.17 fazia isso sozinho); e a API
de xdg-shell mudou de um evento único (`new_surface`) pra dois
(`new_toplevel`/`new_popup`). Sem o guard, corrigir isso pro Mint teria
quebrado o Xubuntu (ou vice-versa).

Consequência: toda chamada nova de API do wlroots em `swlwm.c` precisa
ser checada contra as duas versões antes de assumir que compila nas
duas máquinas. Não remover o guard `SWL_WLR_0_18` enquanto as duas
máquinas de desenvolvimento continuarem em versões diferentes do
wlroots — se um dia as duas forem unificadas numa só versão, aí sim
faz sentido simplificar e remover o guard.

---

## DEC-007 — Colaboração entre IAs: pedir ajuda via comentário na sessão

Status: ACCEPTED

Decisão: qualquer IA trabalhando no projeto pode pedir ajuda de outra
IA (ou de qualquer IA disponível) deixando um comentário no documento
de sessão descrevendo o que precisa — não precisa esperar o usuário
orquestrar isso manualmente. O usuário aprova esse fluxo.

Motivo: aconteceu na prática (2026-09-04) — a OpenHands notou que a
integração do menu iniciar (patch do Claude) tinha chegado incompleta
ao repositório, pediu ajuda pro usuário permitir que ela mesma
terminasse o trabalho, e o usuário autorizou. Funcionou bem: economizou
uma rodada de ida-e-volta e a OpenHands já tinha todo o contexto do
patch original documentado na sessão anterior.

Consequência: toda IA deve continuar documentando decisões/arquitetura
de forma clara o bastante pra outra IA conseguir pegar o trabalho de
onde parou só lendo a sessão (sem precisar perguntar pro usuário) —
isso é o que torna esse fluxo de "pedir ajuda" viável.

---
## DEC-008 — Documentação: sem árvore fixa, sem arquivos obrigatórios; repo mapeado a cada sessão

Status: ACCEPTED

Decisão: o `README.md` (MASTER_PLAN) não mantém mais uma árvore de diretórios "oficial" fixa, nem uma lista obrigatória de arquivos de documentação (`ARCHITECTURE.md`, `TASKS.md`, `INTEGRATION.md`, `CODING_RULES.md`...) que não existem no repositório. Em vez disso:

- **Toda IA deve mapear o repositório e conferir o estado real** (`git status`, `git log`, listagem de arquivos) antes de alterar qualquer coisa — o repositório é a fonte de verdade (seções 20-22 do README).
- **`docs/ai/` é um diretório vivo**: liste o que existe de fato (`ls docs/ai/`, `ls docs/ai/sessions/`) em vez de assumir arquivos por nome.
- **Documente quando for significativo**: decisão, integração, bug não-óbvio, handoff — não por obrigação burocrática..
- **Leveza é regra**: compile só o que for usado, dependência só com uso real, artefatos de build fora do Git, árvores gigantes de terceiros (kernel) via submodule/fetch.

Motivo: a documentação anterior tinha referências a arquivos que não existem (ARCHITECTURE.md, INTEGRATION.md, docs/decisoes.md do "repositório de trabalho anterior", etc.), uma árvore fake que desatualizou (userspace/, libraries/, drivers/, tools/, tests/... que nunca existiram), e regras rígidas (sequência obrigatória de leitura, "nunca dizer terminei", "nunca criar componentes duplicados") que travavam as IAs em vez de ajudar. O resultado era doc que atrapalhava mais do que guiava.



Consequência: novas IAs devem ler o README reescrito(seções 20-22, e seguir o fluxo de mapeamento → estado do Git → leitura do relevante → verificação → alteração → teste → documentação (se significativo) → commit. O `PROJECT_STATE.md` continua sendo o registro do estado real do projeto(com a data da última atualização); o `DECISIONS.md` continua sendo o registro de decisões arquiteturais(ACCEPTED / SUPERSEDED / PROPOSED).

---
## DEC-009 — Sessão só no final sob pedido; trabalho "no off" vive em `docs/ai/EM_ANDAMENTO.md`

Status: ACCEPTED

Decisão sobre o fluxo de documentação das IAs de implementação:

- **Arquivo de sessão (`docs/ai/sessions/`) só é criado no FINAL de um
  pedaço de trabalho, quando o usuário PEDIR** — o usuário pede quando
  acha que aquele pedaço está pronto de "uma certa forma": não precisa
  ser 100% do escopo, mas **passou nos testes sem erro e está
  funcionando**. Não se cria sessão "no meio" nem "no off".
- **Trabalho em progresso / em teste / no off** (coisa que começou, está
  sendo tentada, ainda quebrada, ainda não validada) é registrado em
  `docs/ai/EM_ANDAMENTO.md` **assim que começar** — o registro é barato
  (uma linha/parágrafo) e é atualizado conforme evolui. Ele é uma
  "espécie de sessão", mas descreve o que está sendo desenvolvido e
  testado, não o que já deu certo.
- **Fonte de verdade do repositório**: se não está no repo (código,
  sessão, `EM_ANDAMENTO`, docs), a IA assume que não existe — e registra
  o que for fazer em `EM_ANDAMENTO` na hora que começar. Assim nenhuma IA
  reinventa ou duplica trabalho que já começou (ou já terminou) fora do
  repo e ainda não foi subido.
- **Formato das sessões (padrão do projeto, quando o usuário pedir no
  final)**: a IA explica o **método** que usou, relata o **processo
  completo** do que aconteceu (erros nela / no terminal do usuário,
  ideias que precisaram ser tentadas/descartadas) até chegar na
  conclusão do que deu certo, e relata **tudo o que foi feito e
  implementado** — não só o resultado final.

Motivo: o usuário não consegue subir toda sessão incompleta (e não
devia) — mas o repositório é a fonte de verdade e as IAs leem só ele.
Isso gerou retrabalho real em 2026-09-05: duas IAs (Claude e OpenHands)
viram o mesmo "vazio" (A4 — GUI no boot real) e começaram a mesma coisa
em paralelo, cada uma numa versão diferente do repo; o Claude pegou uma
versão antiga, fez a GUI subir no boot, e agora precisa refazer sobre o
estado atual; a OpenHands parou sozinha no meio sem progresso. O custo
de registrar o que está acontecendo é mínimo comparado a esse tipo de
colisão.

Consequência:
- Toda IA que começar trabalho longo/off DEVE criar/atualizar sua
  entrada em `docs/ai/EM_ANDAMENTO.md` **antes de mergulhar**.
- `docs/ai/sessions/` só recebe sessão quando o usuário pedir, no final
  de um pedaço que passou nos testes.
- Sessões passam a seguir o formato "relato completo do processo"
  (método + erros + ideias tentadas + conclusão + tudo que foi
  implementado).
- O admin continua dono de `docs/orquestracao/` e `docs/revisao/`; o
  `EM_ANDAMENTO.md` fica em `docs/ai/` para que as IAs de implementação
  possam escrever nele sem quebrar essa regra de área.

