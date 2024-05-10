#include "sock.hpp"
#include "payload.hpp"
#include "serializer.hpp"
#include "utils.hpp"

#include "packet_generated.h"

#include <iostream>

#define PORT "20001"
#define BACKLOG 10
// Arquivo Histórico .fbs == .facebook 
// Arquivo histórico: "lacall"

//read an operation:
// supoem-se que já conectou -> sabe-se o nome do usuário e validou o nº de conexões
/*
	## REQ
	- listfiles -> sends the names of the files in the sync_dir
	- dowload -> checks if has the file, then sends the file to the client
	- delete -> deletes the file and sends a response 
	- connect -> saves the user name and sends an ok
		|> all receives a simple text and respondes with simple text
	
	- get_sync_dir -> sends the files to the client
		|> receives simple text, sends a lot of files
		|> needs a finish message -> finished sending or 
			an inicial message, telling how many files will be sent

	- upload -> receives the file from the client and saves it in it's sync_dir
		|> receives a file and sends a simple ok

	## RES
	- OK + data -> delete, listfiles, upload
	- OK + file -> dowload, get_sync_dir
	- ERR
*/


//faz o inicio da conexão, 
//checando o número de pessoas conectadas (max 2)
//e setando o nome do user dessa socket
void initial_handshake(){
	bool isConnected = false; // solução temporária -> Supõe-se que não vai funcionar pra múltiplos clientes/threads
	/*
	if(!isConnected && data->operation_type != Net::Operation_Connect){
		std::cerr << "Servidor: erro no teste, primeira operação enviada não é um Connect" << std::endl;
		exit(2);
	} else if (!isConnected) {
		isConnected = true;
		std::cout << "Cliente connectado: " << data->payload.text << std::endl;
		std::string res("Conectado corretamente!");
		socket->send_response(Net::Status_Ok, res);
	}
	std::cout << (int)data->operation_type << std::endl; 
	if ( data->operation_type == Net::Operation_FileMeta ){
		socket->receive_file(data->payload.filemeta.name, data->payload.filemeta.size); 
	}*/
}

std::unique_ptr<net::Payload> parse_payload_from_buff(uint8_t* buff){
	auto msg = Net::GetSizePrefixedPacket(buff);

	switch (msg->op_type()){
	case Net::Operation_Connect: {
		auto payload = msg->op_as_Connect();
		return std::make_unique<net::Connect>(
			payload->username()->c_str(),
			payload->type(),
			payload->id()
		);
	} break;
	case Net::Operation_ListFiles: {
		return std::make_unique<net::ListFiles>();
	} break;
	case Net::Operation_Ping: {
		return std::make_unique<net::Ping>();
	} break;
	case Net::Operation_Exit: {
		return std::make_unique<net::Exit>();
	} break;
	case Net::Operation_Download: {
		auto payload = msg->op_as_Download();
		return std::make_unique<net::Download>(
			payload->filename()->c_str(),
			false //it's not a clean file, cuz when receiving a dowload req, it has the file to send (is server)
		);
	} break;
	case Net::Operation_Delete: {
		auto payload = msg->op_as_Delete();
		return std::make_unique<net::Delete>(
			payload->filename()->c_str()
		);
	} break;
	case Net::Operation_FileMeta: {
		auto payload = msg->op_as_FileMeta();
		return std::make_unique<net::Upload>(
			payload->name()->c_str(),
			payload->size()
		);
	} break;
	case Net::Operation_FileData:
	case Net::Operation_Response:
		throw net::ReceptionException("Unexpected packet at Payload::parse_from_buffer");
		break;
	default:
		//didn't match any operation known
		throw net::ReceptionException("didn't match any operation known");
	}
	//NOTE: could make a trycatch which cathes and sends the error after
}



void server_loop(std::shared_ptr<net::Socket> socket){
	net::Serializer serde;
	try{
		initial_handshake();
	}catch(...){
		//deal if fails to connect -> response
	}
	
	while(1){
		try{
			auto buff = socket->read_full_pckt();
			auto payload = parse_payload_from_buff(buff);
			std::cout << "Recebido pacote: " << utils::pckt_type_to_name(payload->get_type()) << std::endl;
			payload->reply(serde, socket);
		}catch(const net::CloseConnectionException& e){
			std::cerr << "Cliente desconectado" << std::endl;
			exit(1);
		}catch(const net::ReceptionException& e){
			// TODO: Consertar erro ao enviar um arquivo com 0 bytes
			std::cerr << "Falha ao ler o pacote: " << e.what() << std::endl;
		}
	}

}


int main() {
	net::ServerSocket socket;

	try{
		socket.open(PORT, BACKLOG);
	}
	catch(const net::NetworkException& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}
	while(1){
		try {
			auto s = socket.accept();
			s->print_their_info();
			server_loop(s);
		}catch(const net::NetworkException& e){
			std::cerr << e.what() << '\n';
		}
	}
    return 0;
}

