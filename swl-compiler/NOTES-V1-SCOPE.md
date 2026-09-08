# SWL v1 — escopo e critério de versão completa

Versão completa aqui não é "tudo que existe em C". É o kernel mínimo do
compilador SWL como projeto ativo: parser, semântica e geração de
assembly x86-32 com runtime coesa.

## O que entra na v1 como entrega

- Sintaxe e estrutura: módulo, funções, variáveis locais, tipos de base,
  structs, arrays locais, ponteiros, comentários, literais.
- Controle: if, while, break/return de fn.
- Construção: make/recursos que compilam o frontend e o runtime.
- Teste: pelo menos um programa útil que compila, monta, linka e roda
  com saída esperada e um programa com erros que o compilador rejeita
  de forma controlada.
- Documentação: README, MVP/módulos implementados e decisões relevantes
  atualizadas.

## O que NÃO é requerido para v1

- Backend extra (x86_64, llvm, webassembly etc).
- Frontend extra não utilizado (árvore AST alternativa, parser alternativo
  não integrado ao make principal).
- Biblioteca padrão completa como C; a menos que esteja coberta e ser
  testada pelo pipeline.
- Publicação, deploy ou commit automático. Esse é decisão do mantenedor.

## Critério de v1 concluída

- O compilador roda.
- Os exemplares principais compila/monta/linka/roda.
- A rejeição de casos inválidos é razoável.
- O projeto tem arquivos de rascunho claros sobre o que está pronto.
- Os arquivos-chave (frontend, runtime, makefile) estão coerentes entre si.

## Critério de trabalho adicional

Se o trabalho adicional for representativo e útil (ponteiros, structs,
arrays locais), é razoável conservá-lo como parte da v1. O ponto é que
o trabalho seja integrado ao Makefile e ao pipeline e não left-over
soltos.

## Critério de out-of-scope arquivado

Código que visita sintaxe/semântica/assembly, mas não compila com o make
principal e não está documentado como parte do kernel, pode ser arquivado
ou removido conforme decisão do mantenedor. O trabalho não deve acumular
arquivos fantasmas que confundam o estado do projeto.

## Próximo passo

Revisar o estado atual do swl-compiler, consolidar o que pertence ao
kernel v1, e decidir o que é keep/archival/remove antes de qualquer
extensão.

Próximo passo é comparar a implementação atual com este escopo e reportar
 discrepâncias.
