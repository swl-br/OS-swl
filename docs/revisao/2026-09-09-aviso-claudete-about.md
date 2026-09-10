# AVISO à Claudete — port cirúrgico do About (2026-09-09, orquestrador)

## Veredito: about.c/h APROVADOS; resto BLOQUEADO por base velha

`about.c`/`about.h` compilam limpos contra os headers atuais. Mas
`panel.c`, `meson.build` e `swlwm.c` entregues são pré R-07/R-10/C1/R-13
— copiar apaga tudo isso. Portar SÓ:

1. `swl-ui/src/about.c` + `swl-ui/include/about.h` — como estão.
2. `panel.h`: campos `ajuda_x`/`ajuda_w` + decl
   `swl_panel_hit_test_ajuda` (igual ao entregue).
3. `panel.c`: bloco de medição do item AJUDA + função
   `swl_panel_hit_test_ajuda` (só isso; R-07 intacto).
4. `meson.build`: só a linha `'src/about.c'` (nada do resto).
5. `swlwm.c`: ganchos — include, campo `about`, create/resize/destroy
   junto aos widgets, toggle no clique AJUDA, fechar no X/fora
   (prioridade dos popups). Sem tirar C1/R-13/A5/A6.

## Assets (decisão do usuário)

- `neo.png` é byte idêntico ao `cat.png` já no repo — NÃO duplicar
  1,9MB; apontar o resolve pra `cat.png` (ou justificar).
- `wallpaper.png` do sistema FICA como está (usuário gosta) — não
  trocar.
- `neo-skyline.png` não é referenciado em nada — dizer pra que serve
  ou remover da entrega.

Status: PENDENTE.
