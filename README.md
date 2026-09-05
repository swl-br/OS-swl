# SWL OS — MASTER PROJECT PLAN
## Documento Mestre do Projeto e Guia de Trabalho para IAs

**Projeto:** SWL OS  
**Tipo:** Sistema operacional completo, leve, modular e nativo  
**Arquitetura inicial:** x86 32-bit / i386  
**Ambiente de desenvolvimento:** Xubuntu e Mint xfce Linux x86_64  
**Status:** Desenvolvimento ativo  
**Documento:** Fonte principal de contexto, decisões e regras do projeto

---

# 1. PROPÓSITO DESTE DOCUMENTO

Este é o **Plano Mestre do SWL OS**.

Toda IA, desenvolvedor ou colaborador que trabalhar no projeto deve ler este documento antes de modificar o código.

Este documento existe para:

- explicar o que é o SWL OS;
- registrar as decisões técnicas do projeto;
- definir a arquitetura e os objetivos;
- explicar como os componentes devem trabalhar juntos;
- definir regras para desenvolvimento;
- evitar que diferentes IAs criem soluções incompatíveis;
- preservar decisões importantes ao longo do tempo;
- servir como ponto de entrada para novos colaboradores e IAs;
- definir como o trabalho realizado deve ser documentado.

**O repositório Git é a fonte de verdade do projeto.**

Nenhuma IA deve confiar apenas no contexto da conversa em que está trabalhando. Este documento descreve **princípios e direção** — mas o estado real, a estrutura e o código estão no repositório. Por isso, antes de alterar qualquer coisa: **mapeie o repositório e confira o estado atual dele** (`git status`, `git log`, listagem de arquivos, código). Nunca confie cegamente nem neste documento nem na própria memória — o repositório sempre sabe mais.

---

# 2. VISÃO DO SWL OS

O SWL OS é um sistema operacional próprio construído sobre o **Linux Kernel**, com uma camada de usuários, ambiente gráfico, bibliotecas, serviços e aplicativos desenvolvidos especificamente para o projeto.

O objetivo não é simplesmente criar uma distribuição Linux personalizada.

O objetivo é construir uma experiência de sistema operacional própria:

- identidade visual própria;
- ambiente gráfico próprio;
- aplicativos nativos;
- ferramentas próprias;
- shell próprio;
- linguagem de programação própria;
- integração profunda entre sistema e aplicativos;
- arquitetura modular;
- baixo overhead;
- boa utilização de memória;
- desempenho consistente;
- ferramentas para criação e desenvolvimento;
- capacidade de executar software externo através de uma camada de compatibilidade/integracão apropriada.

O SWL OS deve ser um sistema utilizável de verdade, e não apenas uma demonstração, mockup ou interface estática.

---

# 3. PRINCÍPIO FUNDAMENTAL

## Leve não significa limitado.

O SWL OS deve ser leve por possuir uma arquitetura eficiente, e não por remover arbitrariamente funcionalidades importantes.

O projeto deve buscar:

- baixo consumo de memória;
- baixo consumo de CPU quando ocioso;
- inicialização rápida;
- poucos processos desnecessários;
- componentes modulares;
- carregamento sob demanda;
- serviços independentes quando apropriado;
- código eficiente;
- ausência de bloatware;
- gerenciamento correto de recursos.

Ao mesmo tempo, o sistema deve ser capaz de realizar tarefas reais de:

- programação;
- desenvolvimento;
- edição;
- internet;
- multimídia;
- criação de jogos;
- eletrônica;
- modelagem 3D;
- administração do sistema;
- automação;
- uso cotidiano.

---

# 4. BASE TECNOLÓGICA

## 4.1 Kernel

O SWL OS utiliza o **Linux Kernel** como base de baixo nível.

O projeto não pretende reimplementar um kernel completo do zero apenas por estética.

O Linux fornece uma base madura para:

- processos;
- memória;
- drivers;
- dispositivos;
- rede;
- armazenamento;
- segurança;
- hardware;
- escalonamento;
- sistemas de arquivos;
- infraestrutura de baixo nível.

O SWL OS constrói sua própria experiência e arquitetura de usuários sobre essa base.

---

# 5. ARQUITETURA DE CPU

## Target inicial

**x86 32-bit / i386**

O sistema deve ser compilado e projetado inicialmente para computadores x86 de 32 bits.

O computador de desenvolvimento pode ser x86_64.

Portanto:

**Host:**
- Ubuntu Linux x86_64

**Target:**
- SWL OS x86 32-bit

Todo código novo deve respeitar essa realidade.

Quando necessário, utilizar:

- cross-compilation;
- `-m32`;
- assembler apropriado;
- linker apropriado;
- bibliotecas compatíveis;
- toolchain compatível com i386.

Não assumir que um programa x86_64 funcionará no target.

---

# 6. LINGUAGENS

## 6.1 C

C é uma das linguagens principais do sistema.

Deve ser utilizada para:

- componentes complexos;
- bibliotecas;
- GUI;
- gerenciamento de dados;
- parsers;
- aplicativos;
- serviços;
- infraestrutura;
- código que exige manutenção e clareza.

---

## 6.2 Assembly

Assembly x86 deve ser utilizado quando fizer sentido.

Exemplos:

- boot;
- entrada/saída extremamente baixa;
- rotinas específicas da arquitetura;
- otimizações críticas;
- operações diretamente relacionadas ao processador;
- pequenas rotinas onde Assembly realmente ofereça vantagem.

Não existe a regra de que todo o sistema deve ser 100% Assembly.

**Decisão oficial: C + Assembly conforme a necessidade.**

Forçar Assembly onde C é melhor aumenta complexidade, bugs e custo de manutenção sem benefício real.

---

## 6.3 SWL

SWL é a linguagem própria planejada para o ecossistema do projeto.

A linguagem deverá evoluir progressivamente.

Ela não deve bloquear o desenvolvimento do sistema.

Enquanto o compilador ainda estiver evoluindo, componentes podem ser desenvolvidos em C e Assembly.

---

# 7. COMPILADOR SWL

O projeto possui o conceito de um compilador chamado `swlc`.

A direção do compilador é:

```text
Código SWL
   ↓
Lexer
   ↓
Parser
   ↓
AST
   ↓
Análise semântica
   ↓
Sistema de tipos
   ↓
IR / otimizações
   ↓
Backend
   ↓
Assembly x86
   ↓
Assembler
   ↓
Linker
   ↓
ELF x86 32-bit
```

O compilador deve evoluir para suportar progressivamente:

- tipos fortes;
- inferência;
- funções;
- estruturas;
- enums;
- arrays;
- coleções;
- módulos;
- imports;
- generics;
- traits/interfaces;
- Option;
- Result;
- tratamento de erros;
- memória;
- ownership/segurança de memória;
- `unsafe` controlado;
- threads;
- processos;
- IPC;
- filesystem;
- rede;
- GUI;
- gráficos;
- FFI com C;
- integração com Assembly;
- otimizações;
- debugging;
- testes;
- formatter;
- linter;
- documentação.

Objetivo futuro:

**self-hosting**, quando o ecossistema estiver maduro o suficiente.

---

# 8. FILOSOFIA DE DESENVOLVIMENTO

O projeto deve priorizar software real.

Não criar apenas:

- telas falsas;
- botões sem função;
- protótipos que fingem ser aplicativos;
- funcionalidades descritas mas não implementadas;
- APIs vazias sem necessidade;
- código descartável quando já for possível criar a implementação real.

Um componente pode começar simples, mas deve possuir uma base funcional real e evolutiva.

Se uma funcionalidade ainda não estiver implementada, documentar claramente como:

`TODO`, `PLANNED`, `STUB`, `NOT IMPLEMENTED` ou equivalente.

Nunca apresentar uma função inexistente como concluída.

---

# 9. GUI DO SWL OS

O SWL OS terá uma **interface gráfica própria**.

Ela não deve simplesmente ser um desktop Linux tradicional com outro tema.

A identidade visual é inspirada em tecnologia dos anos 2000, interfaces hacker/retro-tech e estética industrial/digital.

Características desejadas:

- visual escuro;
- cores relativamente sóbrias;
- detalhes em verde, ciano, azul, roxo e magenta quando apropriado;
- aparência tecnológica;
- elementos compactos;
- tipografia quadrada/pixel/terminal quando apropriado;
- barras de título finas;
- controles pequenos;
- ícones próprios;
- janelas eficientes;
- pouca decoração inútil;
- informação visual clara;
- estética consistente.

A interface de referência fornecida para o projeto deve ser tratada como referência visual, não como uma especificação rígida de implementação.

O objetivo é transformar a linguagem visual em uma interface funcional própria.

---

# 10. SISTEMA DE ÍCONES

Os ícones fazem parte da identidade do SWL OS.

O sistema deve possuir um mecanismo centralizado para:

- armazenar ícones;
- identificar ícones por nome/ID;
- fornecer ícones aos aplicativos;
- permitir diferentes tamanhos;
- cache;
- fallback;
- ícones de arquivos;
- ícones de pastas;
- ícones de aplicativos;
- ícones de dispositivos;
- ícones de ações da interface.

Aplicativos não devem duplicar desnecessariamente todos os recursos gráficos do sistema.

A arquitetura deve permitir que os aplicativos solicitem recursos ao sistema/tema de ícones.

---

# 11. ARQUITETURA MODULAR

O SWL OS deve ser modular.

Um aplicativo ou subsistema pesado não deve precisar carregar todos os outros componentes.

Exemplo:

Um usuário trabalhando apenas com arquivos não deveria precisar carregar:

- emulador de CPU;
- simulador eletrônico;
- engine 3D;
- ferramentas de FPGA;
- ferramentas de desenvolvimento de jogos.

Quando apropriado, utilizar:

- módulos;
- plugins;
- bibliotecas compartilhadas;
- carregamento sob demanda;
- processos separados;
- serviços independentes.

---

# 12. APLICATIVOS PRINCIPAIS

A lista abaixo representa o ecossistema planejado. A ordem pode mudar conforme as necessidades técnicas.

## 12.1 SWLPad

Editor de texto e código nativo.

Deve possuir:

- edição real de texto;
- cursor;
- seleção;
- copiar/colar;
- undo/redo;
- teclado;
- atalhos;
- múltiplas abas;
- arquivos grandes;
- line numbers;
- syntax highlighting;
- busca;
- substituir;
- ir para linha;
- UTF-8;
- LF/CRLF;
- autosave/recovery;
- configurações;
- suporte a SWL;
- suporte a C;
- Assembly;
- Shell;
- HTML;
- CSS;
- JavaScript;
- JSON;
- Markdown;
- integração futura com `swlc`;
- diagnósticos;
- autocomplete progressivo.

Não deve ser apenas uma interface de editor.

---

## 12.2 Gerenciador de Arquivos

Aplicativo oficial de gerenciamento de arquivos.

Deve suportar:

- arquivos;
- diretórios;
- navegação;
- copiar;
- mover;
- renomear;
- excluir;
- lixeira;
- restauração;
- busca;
- arquivos ocultos;
- propriedades;
- permissões;
- dispositivos;
- volumes;
- previews;
- thumbnails;
- cache;
- abas;
- múltiplas janelas;
- integração com terminal;
- associação de arquivos;
- arquivos grandes;
- operações com progresso;
- pausa/cancelamento quando possível;
- arquivos compactados;
- integração com sistema de ícones.

---

## 12.3 TSWL

Shell/terminal próprio do SWL OS.

Objetivos:

- comandos nativos;
- scripts;
- gerenciamento do sistema;
- execução de programas;
- automação;
- integração com SWL;
- pipes;
- redirecionamento;
- variáveis;
- processos;
- filesystem;
- rede;
- ferramentas de desenvolvimento.

---

## 12.4 Configurações

Aplicativo central para configuração do sistema.

Deve eventualmente controlar:

- aparência;
- tema;
- ícones;
- display;
- teclado;
- mouse;
- rede;
- áudio;
- dispositivos;
- usuários;
- armazenamento;
- aplicativos;
- serviços;
- energia;
- segurança;
- opções avançadas.

---

## 12.5 SEBRE

Sistema de documentação/manual do SWL OS.

Deve permitir acesso a:

- documentação;
- manuais;
- comandos;
- APIs;
- linguagem SWL;
- desenvolvimento;
- configuração;
- solução de problemas.

---

## 12.6 Media Player

Reprodutor multimídia nativo.

---

## 12.7 Images

Visualizador e posteriormente editor de imagens.

---

## 12.8 Compactador

Gerenciamento de arquivos compactados.

A implementação pode utilizar bibliotecas maduras quando apropriado.

---

## 12.9 Browser

Navegador web.

Não é necessário reinventar toda a infraestrutura web se componentes maduros puderem ser integrados corretamente.

O objetivo é oferecer uma experiência integrada ao SWL OS.

---

## 12.10 Game Studio

Ambiente para criação de jogos.

Possíveis componentes:

- editor;
- cenas;
- sprites;
- scripts;
- áudio;
- física;
- assets;
- debugging;
- exportação.

---

## 12.11 Pixel Studio

Ferramenta de pixel art e desenho 2D.

---

## 12.12 3D Studio

Ferramenta de modelagem e manipulação 3D.

---

# 13. ELECTRONICS / HARDWARE STUDIO

Este é um dos principais aplicativos planejados do SWL OS.

Não deve ser um simples editor de esquemas.

A visão é uma estação de trabalho de engenharia eletrônica.

Fluxo desejado:

```text
Ideia
 ↓
Esquemático
 ↓
Simulação
 ↓
PCB
 ↓
3D
 ↓
Firmware
 ↓
Emulação
 ↓
Validação
 ↓
Fabricação
```

---

## 13.1 Esquemático

Deve suportar componentes como:

- resistores;
- capacitores;
- indutores;
- LEDs;
- diodos;
- transistores;
- conectores;
- fontes;
- sensores;
- ICs;
- microcontroladores;
- componentes personalizados.

---

## 13.2 PCB

Deve permitir:

- formato da placa;
- dimensões;
- camadas;
- trilhas;
- pads;
- vias;
- furos;
- posicionamento;
- componentes;
- regras;
- DRC;
- exportação.

---

## 13.3 Medidas físicas

A ferramenta deve trabalhar com unidades físicas reais.

Exemplos:

- mm;
- cm;
- µm;
- graus;
- coordenadas X/Y/Z;
- largura;
- comprimento;
- altura;
- área;
- perímetro;
- distância;
- ângulos.

Pixels são unidades de interface, não unidades físicas da placa.

---

## 13.4 Simulação

Deve evoluir para permitir:

- tensão;
- corrente;
- resistência;
- potência;
- circuitos analógicos;
- circuitos digitais;
- frequência;
- temporização;
- filtros;
- amplificadores;
- sinais;
- medição virtual.

Instrumentos virtuais planejados:

- multímetro;
- osciloscópio;
- gerador de sinais;
- analisador lógico;
- fonte de bancada.

---

## 13.5 Microcontroladores

Suporte planejado para famílias como:

- ESP32;
- STM32;
- AVR;
- PIC;
- RP2040;
- outras arquiteturas.

Possibilidades:

- configuração;
- programação;
- compilação;
- upload;
- comunicação serial;
- USB;
- debugging.

---

## 13.6 Emulação digital

O usuário deve poder construir sistemas digitais por código e componentes.

Possíveis elementos:

- portas lógicas;
- flip-flops;
- registradores;
- ALUs;
- memórias;
- barramentos;
- periféricos;
- CPUs;
- microcontroladores;
- chips personalizados.

---

## 13.7 Laboratório de CPU

O sistema deve futuramente permitir estudar e construir CPUs virtuais:

```text
Portas lógicas
 ↓
Flip-flops
 ↓
Registradores
 ↓
ALU
 ↓
Controle
 ↓
CPU
 ↓
Memória
 ↓
Barramento
 ↓
Sistema completo
```

Também deve ser possível experimentar ISAs personalizadas.

---

## 13.8 FPGA / HDL

Integração futura com:

- Verilog;
- SystemVerilog;
- VHDL;
- ferramentas de síntese;
- bitstream;
- programação de FPGA.

Sempre que ferramentas open source maduras existirem, considerar integração em vez de reimplementar tudo.

---

## 13.9 Fabricação

Exportações planejadas:

- Gerber;
- drill files;
- BOM;
- Pick & Place;
- documentação;
- arquivos de fabricação.

DRC deve ocorrer antes de exportações críticas.

---

## 13.10 3D

O ambiente deve utilizar dimensões físicas reais para:

- componentes;
- PCB;
- caixas;
- conectores;
- colisões;
- alturas;
- encaixes;
- distâncias;
- integração mecânica;
- impressão 3D.

---

## 13.11 Biblioteca de componentes

Cada componente pode conter:

- símbolo;
- footprint;
- modelo 3D;
- propriedades elétricas;
- pinos;
- dimensões;
- parâmetros de simulação;
- datasheet;
- metadados.

Também deve ser possível criar componentes personalizados.

---

# 14. SOFTWARE EXTERNO

O SWL OS não deve ficar preso exclusivamente a programas escritos em SWL.

Deve existir uma camada/infraestrutura para permitir integração com software externo.

Exemplos possíveis:

- C;
- Python;
- ferramentas Linux;
- Node;
- Git;
- outros runtimes;
- formatos de aplicativos externos.

A solução exata ainda pode evoluir.

Princípio:

**o sistema deve integrar software externo quando isso for tecnicamente útil, sem obrigar o projeto a reimplementar tudo do zero.**

---

# 15. REUTILIZAÇÃO DE SOFTWARE EXISTENTE

Não reinventar componentes maduros sem motivo.

Quando uma tecnologia open source adequada existir, avaliar:

- licença;
- compatibilidade;
- tamanho;
- dependências;
- desempenho;
- segurança;
- manutenção;
- possibilidade de integração;
- impacto na arquitetura.

Reutilizar uma biblioteca madura é aceitável e muitas vezes preferível a escrever uma implementação inferior apenas para dizer que é "100% própria".

O que define o SWL OS é a arquitetura e integração do sistema, não a quantidade de código reinventado.

---

# 16. BUILD SYSTEM

O projeto deve possuir um sistema de build reproduzível.

Ele deve eventualmente permitir:

- configurar o target;
- compilar kernel;
- compilar userspace;
- compilar bibliotecas;
- compilar aplicativos;
- gerar filesystem;
- gerar imagem;
- executar testes;
- executar em emulador;
- gerar artefatos.

O processo deve funcionar de forma previsível em Ubuntu x86_64.

---

# 17. TESTES

Código novo importante deve possuir testes quando tecnicamente aplicável.

Prioridades:

- compilação;
- unit tests;
- integração;
- testes de filesystem;
- testes de GUI;
- testes do compilador;
- testes de parser;
- testes de APIs;
- testes de aplicativos;
- testes de arquitetura x86 32-bit;
- regressão.

Uma alteração não deve quebrar silenciosamente funcionalidades existentes.

---

# 18. SEGURANÇA

O projeto deve tratar segurança como requisito técnico.

Considerar:

- memory safety onde aplicável;
- validação de entrada;
- permissões;
- isolamento de processos;
- execução de arquivos;
- parsing seguro;
- arquivos malformados;
- corrupção de memória;
- overflow;
- underflow;
- race conditions;
- sandboxing quando necessário;
- privilégios mínimos.

Não introduzir mecanismos inseguros apenas para simplificar uma implementação.

---

# 19. PERFORMANCE

Performance deve ser medida, não presumida.

Evitar:

- loops desnecessários;
- cópias desnecessárias;
- alocações excessivas;
- carregamento antecipado de componentes pesados;
- dependências gigantes sem justificativa;
- processos inúteis;
- polling agressivo;
- consumo excessivo de memória.

Quando uma otimização for importante, considerar benchmark.

Não sacrificar segurança, estabilidade ou manutenção por micro-otimizações sem benefício mensurável.

---

# 20. LEVEZA E OTIMIZAÇÃO DE BUILD

Manter o projeto leve é uma exigência prática, não estética. Dicas objetivas:



- **Compile só o que for usado.** Se um build liga 60 bibliotecas/lib mas o código usa 10, as outras 50 não devem ser compiladas nem linkadas. Não existe "vai que um dia precisa" — adicione quando houver uso real.
- **Dependência só entra com uso real no mesmo trabalho.** Se a dependência não é referenciada pelo código que está sendo entregue, ela não entra. Isso vale para bibliotecas, ferramentas e serviços..
- **Nada de "por precaução".** Não carregar framework, toolkit, driver ou app "para o caso de". Carregamento sob demanda e build enxuto são parte da arquitetura, não exceção..
- **Artefatos de build não vão pro Git.** Binários, imagens de disco, logs, diretórios de build gerados—tudo isso é regenerável e só incha o repositório. Use `.gitignore` e/ou scripts..
- **Árvores gigantes de terceiros (kernel etc.) devem ser tratadas com cuidado:** avaliar submodule, script de fetch, ou manter só o que o projeto realmente usa/modifica, em vez de commitar a árvore inteira no repositório principal. O repositório deve ser leve o bastante para clonar e trabalhar sem arrastar centenas de MiB de código que não é do projeto..
- **Meça antes de otimizar.** Perfil, instrumente, e só então decida onde gastar esforço. Leveza não é micro-otimização cega—é não carregar o que não é necessário..
- **Revise o que entra no commit:** código morto, arquivos de build, cópias de assets não usados e dependências não referenciadas não devem ser commitados. Se um arquivo não é necessário pro build ou pro runtime, ele não pertence ao repositório principal.



---

# 21. O REPOSITÓRIO É A FONTE DE VERDADE

**Não existe uma árvore de diretórios "oficial" fixa neste documento — e isso é de propósito.** A estrutura do repositório evolui com o projeto, e uma árvore documentada vira mentira no dia seguinte (e faz a IA confiar no papel em vez do código).

O procedimento correto para qualquer IA, sempre, é:

1. **Mapear o repositório** — listar a estrutura real (`find`, `ls`, explorar os diretórios) antes de qualquer suposição.
2. **Conferir o estado real** — `git status`, `git log`, ver o que foi mudado recentemente, ver se há trabalho novo de outra IA.

3. **Ler o que for relevante para a tarefa** — `README.md`, este documento, `docs/ai/` (ver abaixo) e o código do componente que vai ser alterado.



4. **Verificar se outra implementação já existe** antes de criar algo — nunca assumir que algo está ausente sem procurar no repositório.



5. **Alterar, testar, documentar se significativo, commitar.**



Mudanças estruturais importantes devem ser documentadas em `docs/ai/DECISIONS.md`.

---

# 22. DOCUMENTAÇÃO ENTRE IAs E FLUXO DE TRABALHO

O projeto será desenvolvido por múltiplas IAs. Documentação de trabalho existe para **transferir contexto** — não para virar burocracia. A regra prática:


1. **Documente quando for significativo:** mudança de arquitetura, decisão relevante, integração entre componentes, bug não-óbvio, algo que outra IA precisará saber. Trabalho trivial não exige documento.



2. **`docs/ai/` é um diretório vivo — não um template fixo.** Liste o que existe lá de verdade (`ls docs/ai/`, `ls docs/ai/sessions/`) em vez de assumir arquivos por nome. O que costuma existir:


   - `PROJECT_STATE.md` — estado atual real do projeto (o que funciona, o que não, próximos passos). Atualize quando o estado mudar significativamente.


   - `DECISIONS.md` — decisões arquiteturais importantes, com status (ACCEPTED / SUPERSEDED / PROPOSED). Decisões antigas não são apagadas—quando mudam, marcar a anterior como SUPERSEDED e criar a nova explicando o motivo.


   - `sessions/` — relatos de trabalho de cada IA/sessão que tenha algo a transmitir. Use o formato que fizer sentido para o trabalho—o importante é outra IA conseguir continuar dali: o que foi feito, o que falta, o que ela precisa saber.



3. **Não duplique contexto.** Se a informação já está no código, nos commits ou em outro doc, não repita—aponte.



4. **Fluxo de trabalho padrão** — em vez de uma sequência rígida e obrigatória de leitura:

   1. **Mapeie o repositório** — veja a estrutura real, não a imaginada.
   2. **Confira o estado do Git** — `git status`, `git log`, branches, trabalho em andamento.
   3. **Leia o que for relevante** — `README.md`, `docs/ai/`, e o código do componente que vai alterar.
   4. **Verifique o que já existe** — não duplicar implementação, biblioteca, API ou serviço.
   5. **Altere** — mudanças focadas e claras.


   6. **Teste** — compile, rode, valide o comportamento.
   7. **Documente se significativo** — decisão, integração, estado, handoff.
   8. **Commit** — descritivo, focado, sem lixo.


Nunca assumir que algo está ausente sem procurar no repositório. E lembre: **o repositório sabe mais sobre o projeto do que a memória da IA** — se a memória e o repositório discordarem, confie no repositório( e atualize a documentação se ela estiver errada).

# 23. NÃO QUEBRAR O TRABALHO DE OUTRAS IAS

Antes de modificar uma área:

- entender sua arquitetura;
- preservar APIs existentes quando possível;
- evitar mudanças incompatíveis sem necessidade;
- procurar dependências;
- executar testes relevantes;
- documentar breaking changes.

Se uma alteração de arquitetura for necessária, registrar em `DECISIONS.md`.

---

# 24. NÃO CRIAR COMPONENTES DUPLICADOS

Antes de criar:

- biblioteca;
- API;
- serviço;
- parser;
- sistema de configuração;
- gerenciador;
- widget;
- módulo;

verificar se já existe algo equivalente.

Se existir, reutilizar, melhorar ou integrar.

Não criar duas implementações diferentes da mesma função sem motivo arquitetural.

---

# 25. DECISÕES ARQUITETURAIS

Decisões importantes devem ser registradas em:

```text
docs/ai/DECISIONS.md
```

Formato recomendado:

```text
## DEC-001 — C + Assembly

Status: ACCEPTED

Decisão:
Utilizar C e Assembly de acordo com a necessidade.

Motivo:
Evitar complexidade artificial e preservar desempenho e manutenção.

Consequência:
Assembly será utilizado principalmente em partes de baixo nível e otimizações críticas.
```

Decisões antigas não devem ser silenciosamente apagadas.

Quando uma decisão mudar:

- marcar a anterior como SUPERSEDED;
- criar nova decisão;
- explicar o motivo da mudança.

---

# 26. ESTADO ATUAL DO PROJETO

O arquivo:

```text
docs/ai/PROJECT_STATE.md
```

deve manter o estado atual real do projeto.

Ele deve registrar:

- kernel;
- boot;
- userspace;
- GUI;
- libraries;
- toolchain;
- compilador SWL;
- shell;
- aplicativos;
- testes;
- funcionalidades prontas;
- funcionalidades em desenvolvimento;
- funcionalidades planejadas;
- bugs conhecidos;
- arquitetura atual;
- próximos objetivos.

Esse arquivo deve ser atualizado quando o estado do projeto mudar significativamente.

---

# 27. INTEGRAÇÃO

A integração entre subsistemas deve ser **documentada onde faz sentido** — não num arquivo fixo e obrigatório. Onde registrar depende do caso:

- **Decisões de integração** (ex.: trocar a base da GUI, mudar como o kernel e o userspace se conectam) → `docs/ai/DECISIONS.md` COM status.
- **Estado atual das integrações** (o que está ligado no que, o que falta) → `docs/ai/PROJECT_STATE.md`.

- **Detalhe fino de uma integração específica** → no doc de sessão relevante, ou num comentário no próprio código quando for não-óbvio.



Exemplos de integrações que importam documentar:

```text
Kernel
  ↓
System Services
  ↓
GUI / Window System
  ↓
Libraries
  ↓
Native Apps
```

E:

```text
SWLPad
  ↓
SWL compiler / swlc
  ↓
Toolchain
  ↓
System APIs
```

E:

```text
File Manager
  ↓
Filesystem APIs
  ↓
Storage
```

Toda integração importante deve ser documentada. E lembre: confira sempre `docs/ai/` (o que existe de fato) antes de citar um arquivo por nome.

---

# 28. CODING RULES

Código deve priorizar:

- clareza;
- segurança;
- manutenção;
- eficiência;
- modularidade;
- APIs pequenas;
- dependências justificadas;
- tratamento de erros;
- portabilidade dentro do target;
- documentação de interfaces importantes.

Evitar:

- código morto;
- hacks sem comentário;
- duplicação;
- abstrações inúteis;
- dependências gigantes;
- otimizações sem justificativa;
- funções gigantes quando podem ser divididas logicamente.

---

# 29. GIT

Git é o mecanismo de integração do projeto.

Regras:

- commits devem ser descritivos;
- evitar commits gigantes contendo mudanças não relacionadas;
- não apagar trabalho de outra pessoa/IA;
- verificar `git diff` antes do commit;
- verificar testes antes do commit;
- documentar alterações significativas;
- branches podem ser utilizadas para trabalhos independentes;
- merge deve preservar histórico e contexto quando possível.

Exemplos de commits:

```text
feat(swlpad): add text buffer implementation
fix(gui): correct window event dispatch
feat(fs): add directory enumeration API
docs(ai): update project state
refactor(kernel): isolate scheduler interface
test(swlc): add parser regression tests
```

---

# 30. FLUXO DE TRABALHO MULTI-IA

O projeto funciona como uma equipe.

## Líder do projeto

O usuário é responsável pelas decisões finais do projeto.

---

## Arquiteto / Integrador

Uma IA pode atuar como responsável por:

- arquitetura;
- integração;
- coerência;
- revisão;
- documentação;
- roadmap;
- conflitos entre componentes.

---

## Pesquisa e Desenvolvimento

Outra IA pode atuar em:

- decisões técnicas;
- pesquisa;
- análise de alternativas;
- arquitetura de subsistemas;
- performance;
- especificações;
- prompts;
- planejamento.

---

## Implementação

Outra IA pode atuar diretamente no repositório para:

- escrever código;
- implementar funcionalidades;
- refatorar;
- executar testes;
- corrigir bugs;
- integrar componentes.

---

## QA / Auditoria

Outra IA pode revisar:

- bugs;
- regressões;
- segurança;
- performance;
- compatibilidade i386;
- build;
- testes;
- integração;
- código incompleto.

As funções podem ser exercidas por IAs diferentes ou pela mesma IA em momentos diferentes.

O importante é que o repositório documente o resultado.

---

# 31. REGRA DE OURO PARA IAs

**O repositório sabe mais sobre o projeto do que a memória da IA.**

Uma IA nunca deve dizer:

> "Eu lembro que o projeto era assim."

Ela deve **verificar**. O fluxo já foi descrito na seção 22 — o resumo é:

```text
Mapear o repositório
 ↓
Conferir o estado do Git (status, log, trabalho novo)
 ↓
Ler o que for relevante (README, docs/ai/, código do componente)
 ↓
Verificar o que já existe
 ↓
Alterar
 ↓
Testar
 ↓
Documentar se significativo
 ↓
Commit
```

Nunca confie na memória quando o repositório pode responder. Se os dois discordarem, confie no repositório — e se a documentação estiver errada, corrija-a.

---

# 32. TRABALHO EM GRANDE ESCALA

O projeto não deve ser desenvolvido artificialmente em pequenos pedaços apenas para aumentar o número de etapas.

Quando uma IA receber uma tarefa ampla, ela deve:

1. analisar o escopo;
2. dividir internamente o trabalho;
3. implementar o máximo de funcionalidades coerentes possível;
4. testar;
5. integrar;
6. documentar;
7. entregar um estado funcional.

Não significa implementar coisas impossíveis ou fingir que tudo está pronto.

Significa evitar protótipos mínimos quando uma implementação real já é viável.

---

# 33. REGRA CONTRA ESCOPO FALSO

Uma IA não deve marcar uma funcionalidade como concluída apenas porque:

- existe uma tela;
- existe um botão;
- existe uma função vazia;
- existe um arquivo;
- existe uma interface;
- existe um comentário dizendo que funciona.

"Implementado" significa que existe código funcional suficiente para cumprir o comportamento especificado e que isso foi validado por testes ou uso real apropriado.

---

# 34. ROADMAP GERAL

O roadmap não é rígido.

A prioridade pode mudar conforme bloqueios técnicos.

Uma progressão possível:

```text
FASE 0
Fundação do repositório
↓
FASE 1
Boot + Kernel + toolchain
↓
FASE 2
Userspace básico
↓
FASE 3
System services
↓
FASE 4
GUI própria
↓
FASE 5
Bibliotecas e APIs
↓
FASE 6
TSWL + SWL
↓
FASE 7
Aplicativos essenciais
↓
FASE 8
Ferramentas de desenvolvimento
↓
FASE 9
Electronics Studio
↓
FASE 10
Game / Pixel / 3D Studio
↓
FASE 11
Browser + integração externa
↓
FASE 12
Ecossistema / package manager / app distribution
↓
FASE 13
Otimização e maturidade
↓
FASE 14
Self-hosting progressivo
```

As fases podem ocorrer em paralelo.

---

# 35. O QUE O SWL OS NÃO DEVE SER

O projeto não deve virar:

- apenas um tema Linux;
- apenas um desktop customizado;
- apenas um shell;
- apenas uma coleção de mockups;
- apenas um experimento visual;
- uma distribuição cheia de software desnecessário;
- um projeto que reinventa tudo sem necessidade;
- um sistema artificialmente limitado para parecer leve.

---

# 36. O QUE O SWL OS DEVE SER

O SWL OS deve ser:

- funcional;
- nativo;
- modular;
- eficiente;
- extensível;
- tecnicamente consistente;
- visualmente reconhecível;
- adequado para desenvolvimento;
- adequado para criação;
- adequado para uso cotidiano;
- capaz de evoluir;
- documentado;
- testável;
- colaborativo;
- construído com engenharia real.

---

# 37. REGRA FINAL

Qualquer IA que entrar no projeto deve considerar este documento como a especificação geral do projeto.

Porém:

**Código existente, decisões mais recentes e documentação específica podem atualizar partes deste documento.**

Quando houver conflito:

1. verificar `DECISIONS.md`;
2. verificar `PROJECT_STATE.md`;
3. verificar documentação específica;
4. verificar código e testes;
5. registrar a decisão atualizada.

Nenhuma mudança importante deve permanecer apenas na memória de uma IA.

Se uma informação for importante para o futuro do projeto, ela deve ser colocada no repositório.

---

# 38. CHECKLIST ANTES DE ENTREGAR UMA TAREFA

Antes de considerar uma tarefa concluída:

- [ ] Código compilando
- [ ] Target i386 respeitado
- [ ] Testes executados
- [ ] Funcionalidade realmente implementada
- [ ] Sem funcionalidades falsas
- [ ] Sem duplicação desnecessária
- [ ] Integração verificada
- [ ] Dependências verificadas
- [ ] Performance considerada
- [ ] Segurança considerada
- [ ] Git diff revisado
- [ ] `docs/ai/PROJECT_STATE.md` atualizado se necessário
- [ ] `docs/ai/DECISIONS.md` atualizado se necessário
- [ ] Handoff da IA criado
- [ ] Próximos passos documentados

---

# 39. PRINCÍPIO DE CONTINUIDADE

O SWL OS é um projeto contínuo.

Cada IA deve trabalhar como parte de uma cadeia:

```text
IA A
 ↓
implementa
 ↓
documenta
 ↓
commit
 ↓
IA B
 ↓
entende
 ↓
integra
 ↓
melhora
 ↓
documenta
 ↓
commit
 ↓
IA C
 ↓
...
```

O objetivo não é descobrir quem escreveu mais código.

O objetivo é fazer o projeto avançar sem perder contexto.

---

**SWL OS — Build the system, not just the interface.**
