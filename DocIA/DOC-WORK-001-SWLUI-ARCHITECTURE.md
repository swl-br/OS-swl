### Documento de Trabalho: Estruturação do Compositor Gráfico Nativo (swl-ui)

**Status:** 🧪 EM DESENVOLVIMENTO E TESTES
**Responsável Atual:** AI Integration Engine (Arquiteto)
**IAs Relacionadas:** Nenhuma (Foco exclusivo na infraestrutura base) 

### 1. Escopo do Trabalho

Estruturar o código base do swl-ui utilizando a biblioteca wlroots. O objetivo nesta fase inicial é garantir que o compositor inicialize corretamente, gerencie os backends de vídeo de forma limpa e abra o socket Wayland para comunicação. 

### 2. O que está sendo feito / Planejamento

* [x] Correção de arquitetura: Alinhamento total sobre o uso de wlroots.
* [x] Implementar a inicialização do display Wayland (wl_display_create).
* [x] Configurar os subcomponentes do wlroots (Backend, Renderer e Allocator nativos).
* [x] Criar o loop de eventos principal do servidor para mantê-lo rodando.
* [ ] Configurar os gerenciadores de saídas de vídeo (Outputs) e superfícies de janelas (XDG Shell).

### 3. Registro de Alterações (Histórico)

* **03/09/2026:** Escrita e validação do esqueleto principal de inicialização em C no arquivo swl-ui/src/main.c. O código gerencia displays, alocadores e cria o socket automaticamente de forma limpa.

### 4. Bugs, Bloqueios e Desafios Conhecidos

* **Observação de Teste:** Ao rodar o binário no Xubuntu, certifique-se de ter a variável de ambiente XDG_RUNTIME_DIR definida no seu terminal para que o wl_display_add_socket_auto consiga criar o arquivo de socket com sucesso.

### 5. Integração e Ajuda Necessária

* Nenhuma no momento. Esta é a fundação que permitirá tudo funcionar.

### 6. Conclusão

* [x] Inicialização limpa do wl_display sem erros de segmentação.
* [x] Criação do socket Wayland confirmada na estrutura lógica.
