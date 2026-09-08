# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: TSWL — alt-screen real (CSI ? 1049)

## Escopo

- `apps/tswl/src/term.c`
- `apps/tswl/tests/test_term_parser.c`

## Problema

`?1049h/l` só apagava a tela e restaurava o cursor — o conteúdo
principal (prompt/histórico visível) se perdia ao sair do vim/htop.

## Solução

- Buffer `main_save` (mesmo tamanho do grid)
- `?1049h`: copia grid → main_save, limpa grid, cursor home, `alt_screen=1`
- `?1049l`: restaura main_save → grid, cursor salvo
- Em alt, `scroll_up` **não** alimenta o scrollback
- Resize migra `main_save` junto com o grid

## Testes

```
summary: 19 passed, 0 failed
```
(inclui restauração HELLO após alt)
