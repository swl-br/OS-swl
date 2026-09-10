# Sessao — 2026-09-09

IA: Grok
Data: 2026-09-09
Responsavel: corrigir avisos render-linhagem/t4-dirty + T5 build

Base: origin/main (ece5b26)

## 1. t4-dirty / render-linhagem

Sobre o render.c atual (menubar M1 + T3 DejaVu):

- dirty-row (so linhas dirty + cursor)
- T1 cursor barra 2-3px restaurado
- menubar (swl_menubar_draw + translate) preservado
- T3 DejaVu preservado
- highlight T5 cyan em celulas selecionadas

## 2. T5 build

- term.h/term.c: API de selecao
- main.c: forward decls `selection_copy` / `clipboard_paste` ANTES do
  keyboard_key
- mouse: esquerdo arrasta, direito copia, meio cola
- Ctrl+Shift+C/V
- clipboard interno

## Testes

Parser: 19/19 OK
