[SWL_OS_MASTER_PLAN.md](https://github.com/user-attachments/files/31809672/SWL_OS_MASTER_PLAN.md)
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

O repositório Git é a **fonte de verdade do projeto**.

Nenhuma IA deve confiar apenas no contexto da conversa em que está trabalhando. Antes de alterar algo, deve consultar o estado atual do repositório e os documentos de projeto.

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

# 20. REPOSITÓRIO

Estrutura inicial recomendada:

```text
swl-os/
├── boot/
├── kernel/
├── userspace/
├── apps/
│   ├── swlpad/
│   ├── filemanager/
│   ├── terminal/
│   ├── settings/
│   ├── sebre/
│   ├── media/
│   ├── images/
│   ├── compressor/
│   ├── browser/
│   ├── game-studio/
│   ├── pixel-studio/
│   ├── 3d-studio/
│   └── electronics-studio/
├── libraries/
├── drivers/
├── tools/
├── tests/
├── docs/
│   ├── architecture/
│   ├── specifications/
│   ├── decisions/
│   ├── api/
│   └── ai/
├── assets/
│   ├── icons/
│   ├── fonts/
│   ├── themes/
│   └── wallpapers/
├── scripts/
├── build/
└── README.md
```

A estrutura pode mudar conforme o projeto evolui. Mudanças estruturais importantes devem ser documentadas.

---

# 21. DOCUMENTAÇÃO ENTRE IAs

O projeto será desenvolvido por múltiplas IAs.

Por isso, documentação de trabalho entre IAs é obrigatória.

Cada IA deve registrar o trabalho realizado.

Diretório:

```text
docs/ai/
```

Arquivos gerais recomendados:

```text
docs/ai/
├── PROJECT_STATE.md
├── ARCHITECTURE.md
├── DECISIONS.md
├── TASKS.md
├── INTEGRATION.md
├── CODING_RULES.md
└── AI_HANDOFF.md
```

---

# 22. DOCUMENTO DE CADA SESSÃO / IA

Quando uma IA realizar uma tarefa significativa, ela deve deixar um documento de handoff.

Exemplo:

```text
docs/ai/
├── sessions/
│   ├── 2026-09-01-gpt-swlpad.md
│   ├── 2026-09-01-claude-architecture.md
│   ├── 2026-09-02-gemini-build-system.md
│   └── ...
```

Cada documento deve informar:

```text
# Sessão

IA:
Data:
Responsável:
Branch:
Commit:

## Objetivo

O que foi feito.

## Alterações

Arquivos criados:
Arquivos modificados:
Arquivos removidos:

## Implementação

Descrição técnica das mudanças.

## Decisões

Decisões tomadas durante o trabalho.

## Testes

Testes executados e resultados.

## Problemas conhecidos

Bugs, limitações ou partes incompletas.

## TODO

Próximos passos.

## Integração

O que outras IAs precisam saber antes de modificar esta parte.

## Observações

Informações relevantes para continuidade.
```

A IA não deve simplesmente dizer "terminei".

Ela deve deixar o projeto compreensível para a próxima IA.

---

# 23. REGRAS PARA TODAS AS IAs

Antes de trabalhar:

1. Ler `README.md`.
2. Ler este `MASTER_PLAN.md`.
3. Ler `docs/ai/PROJECT_STATE.md`.
4. Ler `docs/ai/ARCHITECTURE.md`.
5. Ler `docs/ai/DECISIONS.md`.
6. Ler documentos relacionados ao componente que será alterado.
7. Inspecionar o código atual.
8. Verificar o estado do Git.
9. Verificar se outra implementação já existe.

Nunca assumir que algo está ausente sem procurar no repositório.

---

# 24. NÃO QUEBRAR O TRABALHO DE OUTRAS IAS

Antes de modificar uma área:

- entender sua arquitetura;
- preservar APIs existentes quando possível;
- evitar mudanças incompatíveis sem necessidade;
- procurar dependências;
- executar testes relevantes;
- documentar breaking changes.

Se uma alteração de arquitetura for necessária, registrar em `DECISIONS.md`.

---

# 25. NÃO CRIAR COMPONENTES DUPLICADOS

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

# 26. DECISÕES ARQUITETURAIS

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

# 27. ESTADO ATUAL DO PROJETO

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

# 28. INTEGRAÇÃO

O arquivo:

```text
docs/ai/INTEGRATION.md
```

deve explicar como os subsistemas se conectam.

Exemplos:

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

Toda integração importante deve ser documentada.

---

# 29. CODING RULES

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

# 30. GIT

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

# 31. FLUXO DE TRABALHO MULTI-IA

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

# 32. REGRA DE OURO PARA IAs

**O repositório sabe mais sobre o projeto do que a memória da IA.**

Uma IA nunca deve dizer:

> "Eu lembro que o projeto era assim."

Ela deve verificar.

A sequência correta é:

```text
README
  ↓
MASTER_PLAN
  ↓
PROJECT_STATE
  ↓
ARCHITECTURE
  ↓
DECISIONS
  ↓
Código
  ↓
Testes
  ↓
Alteração
  ↓
Documentação
  ↓
Commit
```

---

# 33. TRABALHO EM GRANDE ESCALA

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

# 34. REGRA CONTRA ESCOPO FALSO

Uma IA não deve marcar uma funcionalidade como concluída apenas porque:

- existe uma tela;
- existe um botão;
- existe uma função vazia;
- existe um arquivo;
- existe uma interface;
- existe um comentário dizendo que funciona.

"Implementado" significa que existe código funcional suficiente para cumprir o comportamento especificado e que isso foi validado por testes ou uso real apropriado.

---

# 35. ROADMAP GERAL

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

# 36. O QUE O SWL OS NÃO DEVE SER

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

# 37. O QUE O SWL OS DEVE SER

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

# 38. REGRA FINAL

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

# 39. CHECKLIST ANTES DE ENTREGAR UMA TAREFA

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
- [ ] PROJECT_STATE atualizado se necessário
- [ ] DECISIONS atualizado se necessário
- [ ] Handoff da IA criado
- [ ] Próximos passos documentados

---

# 40. PRINCÍPIO DE CONTINUIDADE

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
