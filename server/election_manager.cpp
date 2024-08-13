#include "election_manager.hpp"
#include <iostream> 

		
namespace net{
	void ElectionManager::add_recv_socket(std::shared_ptr<net::Socket> socket){
		recv_sockets.push_back(socket);
	}
	void ElectionManager::add_send_socket(std::shared_ptr<net::Socket> socket){
		send_sockets.push_back(socket);
	}

	void ElectionManager::stage_send_socket_to_remove(std::shared_ptr<Socket> recv_socket){
		auto weight = recv_socket->election_weight;
		for(auto s: send_sockets){
			if(s.has_value()){
				if(s.value()->election_weight == weight){
					send_socket_to_remove = s;
					break;
				}
			}
		}
	}

	void ElectionManager::remove_staged_send_socket(){
		if(send_socket_to_remove.has_value()){
			remove_send_socket(send_socket_to_remove.value());
		}
		send_socket_to_remove = std::nullopt;
	}

	void ElectionManager::add_client_info(std::shared_ptr<net::ClientInfo> client){
		remove_client_info(client); // remove duplicated instances ( probably we should use a set ) 
		clients_info.push_back(client);
	}


	void ElectionManager::remove_client_info(std::shared_ptr<net::ClientInfo> client){
		std::list<std::shared_ptr<net::ClientInfo>> clients_to_remove; 
		for (auto& client_candidate : clients_info){
			if (client_candidate->ip == client->ip && client_candidate->port == client->port){
				clients_to_remove.push_back(client_candidate); 
			}
		}
		for (auto& client_to_remove : clients_to_remove) {
			clients_info.remove(client_to_remove); 
		}
	}

	void ElectionManager::remove_recv_socket(std::shared_ptr<Socket> socket){
		for(auto i = 0; i < recv_sockets.size(); i++){
			if(recv_sockets.at(i) == socket){
				std::cout << "Removing recv_socket [" <<  socket->get_their_ip() << ":" << socket->get_their_port() << "]" << std::endl; 
				recv_sockets.at(i) = std::nullopt;
			}
		}
	}

	void ElectionManager::remove_send_socket(std::shared_ptr<Socket> socket){
		for(auto i = 0; i != send_sockets.size(); i++){
			if(send_sockets.at(i) == socket){
				std::cout << "Removing send_socket [" <<  socket->get_their_ip() << ":" << socket->get_their_port() << "]" << std::endl; 
				send_sockets.at(i) = std::nullopt;
			}
		}
	}
}