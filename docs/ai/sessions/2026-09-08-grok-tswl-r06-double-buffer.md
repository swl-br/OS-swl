# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: R-06 — double-buffer / `wl_buffer.release` no TSWL (resto da A2)

## Escopo

Apenas `apps/tswl/src/main.c`.

Não mexeu em: swl-ui, term.c, meson, outros apps.

Base: `origin/main` pós A1 (`344d861`) e A2 parcial (`baae84b`).

## Problema (R-06)

Buffer shm **único** era reescrito a cada `redraw` sem esperar
`wl_buffer.release`. No resize, `wl_buffer_destroy` podia derrubar um
buffer ainda em uso pelo compositor → race / tearing / crash possível.

O comentário no código antigo dizia que esperava release; o listener
era no-op.

## Solução

Double-buffer explícito (`TSWL_BUF_COUNT = 2`):

| Peça | Comportamento |
|------|----------------|
| `struct tswl_shm_buf` | `wl`, `data`, `size`, `busy`, `stale`, `app` |
| `buffer_release` | `busy = false`; se `stale`, libera e recria no tamanho atual |
| `redraw` | pega slot livre (`!busy && !stale`); pinta; `attach` + `busy = true` |
| sem slot livre | mantém `need_redraw` e tenta de novo no próximo ciclo |
| `recreate_buffers` (resize) | slots livres: destroy + create; slots busy: `stale = true` (troca no release) |
| cleanup | libera todos os slots |

Nunca se destrói um `wl_buffer` ainda marcado `busy`.

## Arquivo alterado

- `apps/tswl/src/main.c`

## Verificação

Sandbox sem `wayland-client.h` — sem build gráfico aqui. Leitura
estática confirma:

- zero referências ao antigo `a->buffer` / `buffer_data` único
- `busy` setado no attach; liberado só no release
- resize não sobrescreve slot busy

Sugestão de teste no host:

```bash
cd apps/tswl && meson setup build && ninja -C build
./build/tswl
# redimensionar a janela várias vezes rápido
# digitar + receber saída do shell em paralelo
```

## Próximo passo sugerido

- Orquestrador: marcar R-06 como CORRIGIDO; A2 completa.
- Candidatos seguintes: B3 (`fetch-deps` i386), A5/A6, ou B2
  (revalidar se o reset do cursor ainda falta no `main`).
