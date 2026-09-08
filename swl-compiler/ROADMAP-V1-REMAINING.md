# Roadmap v1 — perspectiva de linguagem em evolução

Este documento é uma reflexão sobre o que pode ainda restar para a primeira
versão completa do compilador SWL, visto como processo em evolução.

## Linguagem como processo

A linguagem não é um arquivo único. Ela é um conjunto de decisões,
restrições, exemplos e rejeições coerentes entre si. Por isso, v1 pode ser
considerada chegando mesmo antes de “tudo” estar escrito, desde que o
conjunto verificado seja consistente.

## Pontos possíveis ainda não verificados

- Documentação end-to-end: README coeso, spec atualizada, critério de
  fechamento claro.
- Pipeline de teste automatizado: `make test` do `swl-compiler/` confirma
  exemplos e rejeições.
- Coerência dos arquivos principais: frontend, runtime e Makefile alinhados.
- Ausência de código fantasma que confunda o estado.

## O que já pode estar coberto

- Parser e semântica para os módulos implementados.
- Geração de assembly x86-32 coerente com o runtime.
- Exemplos e rejeições que dão visibilidade ao comportamento.

## Como testar cada ponto

- Leitura humana dos arquivos chave.
- Execução do `make test`.
- Verificação de que os exemplos principais compilam, montam, linkam e rodam.
- Verificação de que programas inválidos são rejeitados.

## Critério de avanço

Se um ponto real for identificado como faltante, eu sinalizo que a v1 ainda
não está completa e continuo trabalhando nos arquivos da linguagem.

Se nada real for identificado como faltante, eu sinalizo que a v1 pode ser
considerada chegando e deixo a decisão final como observável, não hipótese.

## Próximo passo

Comparar o estado atual com este roadmap antes de qualquer aviso de
prontidão.
