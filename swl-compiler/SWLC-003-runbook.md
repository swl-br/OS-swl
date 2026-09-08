## SWLC-003 — runbook local mínimo

**Entrada**  
Compilador presente em `swl-compiler/build/swlc`.  
Runtime presente em `swl-compiler/build/swlrt.o`.  
Exemplos em `swl-compiler/examples/`.  
Rejeições em `swl-compiler/tests/fail/`.

**Early failure**  
Antes de qualquer ação nova, o runbook deve emitir um estado simples:
- compilador presente;
- runtime presente;
- pelo menos um exemplo de cada módulo em estado coerente.

Se qualquer um desses itens falhar, o runbook para e não avança.

**Estado**  
Estado é emitido pelo próprio pipeline local e pelo marcador de módulo no
`swl-compiler/`. Não há estado válido sem pipeline.

**Build**  
Build é:
- reconstruir o compilador se necessário;
- reconstruir o runtime se necessário;
- compilar cada exemplo de módulo em assembly e montar.

**Test**  
Test é:
- para cada exemplo, verificar que o código de saída corresponde ao `.exit`;
- para cada entrada inválida, verificar que o compilador rejeita de forma
  controlada.

**Entrega local**  
Entrega local é decidida pelo estado do marcador e pelo resultado do test
local. Não há entrega sem marcador e sem teste local.

**CI mínimo**  
Se houver automação, o CI deve executar os passos 1–7 do pipeline e reportar
sucesso ou falha por módulo.

**Próximo passo**  
Próximo passo é manter a verificação local antes de qualquer extensão e só
avançar quando o pipeline confirmar.

**Laudo de sessão**  
Ao final de cada sessão, o runbook pede um laudo mínimo:
- compilador presente ou ausente;
- runtime presente ou ausente;
- exemplos verificados;
- módulos em estado atualizado ou pendente.

Esse laudo é complementar ao `SWLC-004-status.md` e serve como entrada para
decisões de entrega.
