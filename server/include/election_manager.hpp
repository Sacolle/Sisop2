#pragma once

#include "sock.hpp"
#include "mutex.hpp"

#include <list>
#include <memory>

namespace net{

	class ElectionManager {
		public:

			ElectionManager(ElectionManager const&) = delete;
			void operator=(ElectionManager const&) = delete;
			static ElectionManager& getInstance(int valor = 0){
				static ElectionManager instance(valor);
				return instance;
			}

			void add_recv_socket(std::shared_ptr<Socket> socket);
			void add_send_socket(std::shared_ptr<Socket> socket);

			//seta qual socket de send deve ser removida no fim da eleição
			//isso serve de sincronização entre as threads, para n remover uma socket
			//durante a eleição
			//recebe a recv socket e encontra a send socket correspondente
			void stage_send_socket_to_remove(std::shared_ptr<Socket> recv_socket);
			//remove a socket q foi staged
			void remove_staged_send_socket();

			void remove_recv_socket(std::shared_ptr<Socket> socket);
			void remove_send_socket(std::shared_ptr<Socket> socket);

			bool in_election(){
				auto lock = _in_election.lock();
				return lock.get();
			}
			void set_in_election(bool e){
				auto lock = _in_election.lock();
				*lock.raw() = e;
			}

			bool is_coordinator(){
				auto lock = _is_coordinator.lock();
				return lock.get();
			}
			void set_is_coordinator(bool e){
				auto lock = _is_coordinator.lock();
				*lock.raw() = e;
			}
			inline std::shared_ptr<Socket> get_coordinator_socket(){ return coordinator_socket; }
			inline void set_coordinator_socket(std::shared_ptr<Socket> s){ coordinator_socket = s; }

			std::vector<std::optional<std::shared_ptr<Socket>>>& get_recv_sockets(){ return recv_sockets; }
			std::vector<std::optional<std::shared_ptr<Socket>>>& get_send_sockets(){ return send_sockets; }

			const int valor;

		private:
			ElectionManager(int v): valor(v), 
				_in_election(Mutex(false)), _is_coordinator(Mutex(false)){}


			Mutex<bool> _in_election;
			Mutex<bool> _is_coordinator;
			std::shared_ptr<Socket> coordinator_socket;

			std::vector<std::optional<std::shared_ptr<Socket>>> recv_sockets;
			std::vector<std::optional<std::shared_ptr<Socket>>> send_sockets;

			std::optional<std::shared_ptr<Socket>> send_socket_to_remove = std::nullopt;
	};
}

