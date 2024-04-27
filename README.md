# Trabalho de Sistemas Operacionais 2

Este trabalho é para a cadeira de sistemas operacionais 2 da UFRGS.
Nele faremos um clone do dropbox usando as bibliotecas nativas do linux como socket e posix threads para a sincronização.
Além disso é usado como biblioteca de marshaling o [flatbuffers](https://flatbuffers.dev/).

## Buildar o projeto
Esse projeto é feito para linux, então se estiver no windows é requerido o uso de WSL.
Para a build se usa `cmake`, versão mínima sendo 3.20.
Usando o VSCODE, basta usar as extensões `CMake` e `CMake Tools`. Se não, basta rodar:

```bash
# precisa rodar esse comando uma vez
cmake -S . -B build 
#roda toda vez que quiser compilar o programa
cmake --build build
```

Os executáveis estarão dentro de build nas pastas client e server.

Lembrando que se você está no windows, faça por WSL e instale as extensões do VSCODE no WSL.

Caso ainda tenha erros no VSCODE, tem essa [resposta no StackOverflow que resolve](https://stackoverflow.com/a/71115284)