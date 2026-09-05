# ORQUESTRAÇÃO — SWL OS

## O que é esta pasta

Área de trabalho dos **administradores/orquestradores** do projeto (nesta
fase: a IA desta sessão e GPT, com supervisão do usuário dono do projeto).

O papel do orquestrador é:

- **pensar e planejar** — ideias, ordem de execução, prioridades;
- **organizar** — lista de afazeres, estados, donos;
- **revisar** — apontar bugs e pendências (ver `../revisao/`);
- **chamar a atenção dos responsáveis** — indicar quem deve corrigir o quê;
- **manter a documentação** — `../ai/PROJECT_STATE.md`,
  `../ai/DECISIONS.md` e os documentos desta pasta.

O orquestrador **não mexe em código por padrão**. Só em caso de
necessidade real e acordada com o usuário. A regra do projeto continua:
quem implementa é a IA que pega a tarefa (via sessão documentada).

## Arquivos desta pasta

| Arquivo | Conteúdo |
|---|---|
| `README.md` | Este arquivo — propósito e regras |
| `AFAZERES.md` | Lista-mestra de trabalho: desbloqueado / planejado / em curso |
| `IDEIAS.md` | Ideias e propostas ainda não comprometidas |
| `DIARIO.md` | Atualizações diárias do que mudou (entre sessões) |

## Regras de edição

1. **Só orquestradores (eu e GPT) editam esta pasta e `../revisao/`.**
   IAs de implementação podem *ler* e *sugerir*, mas não editar.
2. **Não mexo em `../ai/sessions/`** — aquilo é propriedade das IAs de
   implementação. Eu leio para saber quem fez o quê, mas não edito.
3. `../ai/PROJECT_STATE.md` e `../ai/DECISIONS.md` **são nossos** de
   editar (com base no que o repo realmente mostra).
4. Toda revisão de código vira documento em `../revisao/` com:
   achado, arquivo:linha, severidade, **responsável** e status.

## Fluxo de trabalho

```
1. Mapear o repositório (git status, git log, estrutura real)
2. Conferir o que mudou (DIARIO.md aponta mudanças; mas sempre conferir no repo)
3. Atualizar AFAZERES.md (mover blocos, abrir prioridades)
4. Revisar o que merece (behavior/segurança/build) → ../revisao/
5. Escrever docs/revisao apontando responsáveis e pedindo correção
6. Atualizar PROJECT_STATE.md quando o estado mudar de verdade
7. Registrar no DIARIO.md o que foi feito nesta rodada
```