# redes-computadores
Repositório geral para os projetos da disciplina de Redes de computadores da pós-graduação em Ciências da computação - CIN-UFPE

### Projeto Dumbbell
Utilizando o simulador de redes NS-3 foi solicitado o seguinte cenário:
  1)    Criar um cenário com um transmissor e um receptor, largura de banda dos links 10 Mbps, topologia Dumbbell;
  2)    Configurar uma transmissão (background) usando UDP que dura a duração inteira da simulação e que transmite usando UDP numa taxa de 8 Mpbs;
  3)    Configurar que a fonte de tráfego S1 envia com TCP Reno em 5 Mbps concorrendo com o UDP;
  4)    Plote a taxa de transferência de um arquivo grande no tempo;
  5)    Plote a evolução da janela do transmissor S1;
  6)    Refazer o mesmo estudo para a versão TCP Cubic.

Para isso foi criado o arquivo main.cc que contém a simulação e a geração dos arquivos necessários para a análise da simulação.
    
