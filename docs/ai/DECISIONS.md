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
