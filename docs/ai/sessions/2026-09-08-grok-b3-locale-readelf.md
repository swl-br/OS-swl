# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: B3 ressalva — locale do `readelf` em `fetch-deps.sh`

## Escopo

Apenas `userland/fetch-deps.sh`.

Não toca: `swl-compiler` (Buffy), resize Claude (C1).

## Problema

Em locale pt_BR, `readelf -h` imprime `Classe:` em vez de `Class:`.
O `grep` antigo só aceitava `Class:` → `ensure_elf32_or_rebuild`
achava que o binário ELF32 não era válido e forçava rebuild
redundante (sem deixar 64-bit passar — só desperdício).

## Solução

- `LC_ALL=C readelf -h` nos dois caminhos (`assert_elf32` e
  `ensure_elf32_or_rebuild`)
- grep `Class(e)?:` como rede de segurança se o locale vazar

## Arquivo

- `userland/fetch-deps.sh`
