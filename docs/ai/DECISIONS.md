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
