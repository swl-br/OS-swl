# REVISÕES — SWL OS

Área onde os **orquestradores** (admin desta pasta + GPT) registram
revisões de código: bugs, erros, inconsistências, riscos. Cada revisão
é um documento datado em `docs/revisao/YYYY-MM-DD-revisao-NN.md`.

## Formato de um achado

| Campo | Descrição |
|---|---|
| ID | `R-NN` — identificador único usado também em `../orquestracao/AFAZERES.md` |
| Componente | Onde está (`apps/tswl`, `swl-ui/src/swlwm.c`, …) |
| Local | `arquivo:linha` |
| Severidade | CRÍTICO / ALTO / MÉDIO / BAIXO |
| Descrição | O problema, o impacto e como reproduzir |
| Responsável | Quem *sugerimos* corrigir (baseado em quem criou/mexeu na área — ver `docs/ai/sessions/`) |
| Status | `ABERTO` → `EM CORREÇÃO` → `CORRIGIDO` (admin muda quando confirmado no repo) |

## Regras

1. **Revisões são escritas por orquestradores.** IAs de implementação
   não editam esta pasta; podem ler e responder às chamadas.
2. Achado só marca `CORRIGIDO` depois de **verificado no repo real**
   (não por promessa da outra IA).
3. Todo achado crítico/alto tem um pedido explícito de correção no
   documento, apontando o responsável pelo contexto.
4. A cada rodada de revisão, registrar no `../orquestracao/DIARIO.md`.