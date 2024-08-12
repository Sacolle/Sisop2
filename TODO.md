Arrumar:
    ~arquivo temporário vim/editor de texto


DECISÕES:
 - [x] mecanismo para checar que o servidor parou de funcionar 
 	- Usar as conexões do bully, quando ela desconectar, usar do fato que o recv 
	retorna o valor 0. (CloseConnectionException)
 - [x] decidir o método de votação (bully)
 - [x] decidir como será a comunicação entre servidores durante o funcionamento normal
 	- Os servidores irão se utilizar da conexão do bully para repassar os pacotes, 
	assim como é feito para outros usuários
	- Para isso deve-se colocar nos pacotes quem são as pessoas que os enviaram, 
	para poder colocar na pasta certa
 - [x] decidir como o cliente vai reestabelecer a conexão com o servidor coordenador
	- O cliente vai ter uma porta UDP que ele vai receber uma mensagem do coordenador eleito, com isso
	ele vai fazer o processo normal de mandar mensagem de connect e etc.
	- O UDP já retorna qual o IP do datagrama recebido, então os outros 
	servidores devem ter uma lista dos cliente conectados para poder mandar essa mensagem via UDP
		- Isso pode ser implementado adicionando um campo IP ao pacote connect, que no caso dos servidores de 
		replicação vai ser usado para essa lista de ips, ou usando um pacote novo
 

(1) todos os clientes sempre utilizarão a mesma cópia primária;
(2) após cada operação, o RM primário irá propagar o estado dos arquivos aos RMs de backup;
(3) somente após os backups serem atualizados o primário confirmará a operação ao cliente.

C -> S1
S1 -> S2, S3
S2 e S3 reply-> S1
S1 reply-> C



TODO:
Client:
 - [ ] Mudar a estrutura para o programa não parar quando a conexão acaba
 - [ ] Criar o schema dos pacotes que serão transmitidos no UDP
 - [ ] Criar a socket UDP no cliente que espera o sinal do coordenador novo
 - [ ] Montar o sistema para, na mensagem do coordenador, começar a conexão com o servidor novo

Server:
 - [ ] Criar uma fase de boot no servidor, para poder estabelecer a conexão entre as replicações. 
 Usar um arquivo de configuração, ou passar pelo terminal via prompt, ou como args de entrada 
 com um comando de GO ou um timer pra começar
 - [ ] Modificar o schema dos pacotes para poder reconecer de qual usuário ele é transmitido
 - [ ] Adicionar na estrutura controller o relay dos pacotes para os outros servidores
 - [ ] Adicionar as threads necessárias para enviar e receber os pacotes das diferentes replicações
 - [ ] Adicionar um método para setar as sockets ser não bloqueante
 - [ ] Implementar o algoritmo do bully, usando essas conexões e essas threads já alocadas para o relay dos pacotes
 - [ ] Fazer um relay de um pacote novo -> pacote que faz o relay de quais usuários estão conectados e os seus IPs
 - [ ] Fazer a socket UDP para notificar todos os usuários conectados qual o novo servidor




Partição de threads para o bully:
```cpp
//thread para falar com os outros 
ServerSocket socket;
vector<ClientSocket> other_servers;

vector<Socket> send_servers;
vector<Socket> recv_servers;

//...

//inicializa as conexões
//espera todos os servidores se conectares a ele

cin >> a;
for(auto& s: other_servers){
	s.connect( /* information */);
	send_servers.push_back(s.build());
}

while(send_servers.size() < 3){
	recv_servers.push_back(socket.accept());
}


```
Cada servidor pode gerir em uma única thread todas as conexões, 
mas para isso o recv tem que ser não bloqueante. 
Segundo esse link: https://www.cs.uic.edu/~ygu1/doc/recv.htm#:~:text=In%20non%2Dblocking%20mode%2C%20recv,expires%2C%20zero%20will%20be%20returned.
Usar o modo não bloqueante não faz ele perder a propriedade de retornar 0 quando a conexão cai, 
ele simplesmente retorna n < 0 quando não há pacotes