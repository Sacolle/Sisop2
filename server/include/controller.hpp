#pragma once

#include <pthread.h>
#include "payload.hpp"
#include "user_server.hpp"
#include "mutex.hpp"
#include <map>
#include <set>
#include <optional>

namespace net{
	class UserSession {
		public:
			bool is_files_synched(int id);
			void set_files_synched(int id);

			bool add_session(int id); 
			bool remove_session(int id);

			UserSession(const std::string& username);

			const std::string username;
		protected:
			std::map<int, std::queue<std::shared_ptr<net::Payload>>> data_packets_map;
			std::map<int, bool> synched_files_at_start;
			std::set<int> session_ids; 

		friend class Controller;
	};

	class Controller {
		public:
			bool add_session(const std::string& username, int id); 
			bool remove_session(const std::string& username, int id);

			bool is_files_synched(const std::string& username, int id);
			void set_files_synched(const std::string& username, int id);

			std::optional<std::shared_ptr<net::Payload>> get_data_packet(const std::string& username, int id);
			void add_data_packet(const std::string& username, std::shared_ptr<net::Payload> payload); 

			Controller(Controller const&) = delete;
			void operator=(Controller const&) = delete;

			static Controller& getInstance(){
				static Controller instance;
				return instance;
			}
		private:
			Controller() : users_sessions(Mutex(std::map<std::string, UserSession>())) {}
			Mutex<std::map<std::string, UserSession>> users_sessions;

	};
}