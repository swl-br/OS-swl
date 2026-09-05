# EM_ANDAMENTO — Trabalho começado, ainda não finalizado (o "no off")

Registro aberto das IAs de implementação para tudo o que **começou mas
ainda não virou sessão/commit final**. Criado por DEC-009 (ver
`docs/ai/DECISIONS.md`).

Regras:

1. **Quem**: qualquer IA de implementação, além do usuário. O admin pode
   editar/limpar entradas, mas o dono natural da entrada é quem está
   trabalhando.
2. **Quando**: REGISTRE ASSIM QUE COMEÇAR — antes de mergulhar em
   trabalho longo ou "no off". Atualize a entrada conforme evolui
   (mudou de %? quebrou? trocou de abordagem?).
3. **Por quê**: o repositório é a fonte de verdade. Se um trabalho não
   está aqui e não está no repo, outra IA assume que "não existe" e o
   refaz do zero — **custo real de colisão** (ex.: A4 foi começado por
   Claude e OpenHands ao mesmo tempo, cada um numa versão do repo).
4. **Quando sai daqui**: quando o usuário pedir a sessão (trabalho
   passou nos testes e está funcionando) → move-se o relato para
   `docs/ai/sessions/` com o formato "relato completo do processo"
   (método + erros + ideias tentadas + conclusão + tudo implementado),
   e remove a entrada daqui.
5. **Se a entrada não existe, o trabalho não existe.** Não comece algo
   que já conste aqui (ou registre a sua abordagem separada se for
   mesmo concorrente — e avise na entrada existente).

Formato da entrada (coluna "Entrada" na tabela):

```
[data] IA: o que estou fazendo / tentando — onde (arquivos/dir) —
progresso (% estimado) — problema/abordagem atual em 1-2 linhas —
método de contato (ex.: minha sessão/contexto).
```

---

## 🔴 Registro

| Data | IA | Entrada (resumo) |
|---|---|---|
| 2026-09-05 | Claude | **A4 — GUI no boot real (DRM/KMS)**. Peguei uma versão ANTIGA do repo (anterior às atualizações recentes) e consegui subir a GUI no boot — funcionou nessa base. Agora **refazendo sobre o estado atual do projeto**. Método está validado, precisa reaplicar. Caminho: integrar swlwm como sessão gráfica principal no initramfs. |
| 2026-09-05 | OpenHands | **A4 — GUI no boot real**. Também comecei sem saber que o Claude estava nisso. **Paro sozinha no meio do processo (bug) — ainda sem progresso/produto.** Sem receita de conclusão. |
| 2026-09-05 | Usuário | **Decisão**: seguir o **método do Claude** (validado na versão antiga) para A4. OpenHands fica de fora de A4 até destravar o bug de parar no meio. |

---

## 📋 Entradas detaçadas

### A4 — GUI no boot real (DRM/KMS) — Claude (2026-09-05)

- **Status**: em andamento — método validado na versão antiga; sendo
  reaplicado no estado atual do repo.
- **O que**: rodar o swlwm como sessão gráfica principal dentro do
  initramfs, sem X11 (integração DRM/KMS).
- **Progresso estimado**: ~depende de reaplicação sobre a base atual.
- **Observação (admin)**: quando validar, pedir a sessão no formato
  completo (método + erros + ideias tentadas até o que deu certo).
  Depois disso, marcar A4 conforme o resultado em `AFAZERES.md`.