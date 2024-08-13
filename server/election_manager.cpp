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

	void ElectionManager::remove_recv_socket(std::shared_ptr<Socket> socket){
		for(auto i = 0; i < recv_sockets.size(); i++){
			if(recv_sockets.at(i) == socket){
				recv_sockets.at(i) = std::nullopt;
			}
		}
	}

	void ElectionManager::remove_send_socket(std::shared_ptr<Socket> socket){
		for(auto i = 0; i != send_sockets.size(); i++){
			if(send_sockets.at(i) == socket){
				send_sockets.at(i) = std::nullopt;
			}
		}
	}

	void ElectionManager::add_clients_adress(const std::string& ip, const std::string& port){
		clients_adress.emplace_back(std::string(ip), std::string(port));
	}
}