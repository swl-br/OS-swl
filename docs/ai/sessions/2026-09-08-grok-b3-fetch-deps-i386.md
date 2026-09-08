# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: B3 — `fetch-deps.sh` deve gerar bash/busybox **i386** estáticos

## Escopo

Apenas `userland/fetch-deps.sh`.

Não mexeu em: build-rootfs.sh (já aponta `/bin/sh` → busybox e documenta
o problema), kernel, GUI, apps.

## Problema

Num host x86_64, o script antigo compilava bash/busybox **nativos**
(ELF 64-bit). O kernel do SWL OS é i386 → `execve` de `/bin/sh` ou
scripts falhava com ENOEXEC e o boot ficava calado. A sessão
2026-09-07 já tinha corrigido um rootfs pontual (`sh → busybox` i386),
mas um `fetch-deps.sh` limpo reintroduzia o binário errado.

## Solução

1. **CFLAGS/LDFLAGS i386**: em host não-i386, usa `-m32 -static -Os`.
2. **`require_i386_cc`**: testa `cc -m32 -static` no início; se falhar,
   mensagem clara pedindo `gcc-multilib` / `libc6-dev-i386`.
3. **`assert_elf32`**: valida o binário com `readelf -h` ou `file`
   (Class ELF32 / "ELF 32-bit"); falha alto se for 64-bit.
4. **`ensure_elf32_or_rebuild`**: se `build/tools/{bash,busybox}` já
   existir mas não for ELF32, apaga e reconstrói (evita reutilizar
   lixo do host).
5. **bash**: `./configure --host=i386-pc-linux-gnu` + CFLAGS/LDFLAGS
   i386.
6. **busybox**: `CONFIG_STATIC=y` + `EXTRA_CFLAGS`/`LDFLAGS` i386.
7. Validação final logo antes de chamar `build-rootfs.sh`.

## Arquivo alterado

- `userland/fetch-deps.sh`

## Verificação

- `sh -n userland/fetch-deps.sh` → syntax ok.
- Não rodei o download/build completo neste sandbox (rede/deps
  multilib limitadas); a lógica de detecção e as flags estão no script.

Sugestão no host de desenvolvimento:

```bash
# se ainda tiver binários 64-bit:
rm -f build/tools/bash build/tools/busybox
sudo apt-get install gcc-multilib libc6-dev-i386   # se precisar
./userland/fetch-deps.sh
file build/tools/bash build/tools/busybox
# deve mostrar ELF 32-bit LSB executable, statically linked
```

## Próximo passo sugerido

- Orquestrador: marcar B3 como concluída após teste num clone limpo
  x86_64.
- Candidatos seguintes: A5 (maximizada no resize de output), A6
  (fullscreen real), A7 (leveza de build).
