#include "controller.hpp"
#include "exceptions.hpp"
#include <iostream>
#include <exceptions.hpp>

namespace net{

	UserSession::UserSession(const std::string& username): username(username){}

	void UserSession::set_files_synched(int id){
		if (session_ids.count(id) == 0) {
			throw std::runtime_error("Session " +  std::to_string(id) +  " not logged for user " +  username); 
		}
		synched_files_at_start[id] = true; 
	}

	bool UserSession::is_files_synched(int id){
		if (synched_files_at_start.count(id) == 0) {
			throw std::runtime_error("Session " +  std::to_string(id) +  " not logged for user " +  username); 
		}
		return synched_files_at_start[id];
	}
	/*
	void UserSession::add_data_packet(int id, Serializer& serde, std::shared_ptr<Socket> socket){
		auto data_packets_map_lock = data_packets_map.lock(); 
		if (data_packets_map_lock->count(id) == 0){
			throw std::runtime_error("Session " +  std::to_string(id) +  " not logged for user " +  username); 
		}
		auto& queue = data_packets_map_lock.get()[id];
		if (queue.empty()) {
			return; 
		}
		auto& payload = queue.front();
		queue.pop();
		if (payload->get_type() == Net::Operation_FileMeta) {
			dynamic_cast<net::Upload*>(payload.get())->is_server = true; 
		}
		payload->send(serde, socket);
		payload->await_response(serde, socket);
	}*/

	bool UserSession::remove_session(int id){
		if (session_ids.count(id) == 0) {
			return false; 
		}
		session_ids.erase(id);
		synched_files_at_start.erase(id);
		data_packets_map.erase(id);
		return true;
	}

	bool UserSession::add_session(int id){
		// Trying to add session already logged
		if (session_ids.count(id) != 0){
			std::cout << "Trying to add session already logged: " << username << std::endl;
			return false; 
		}
		// limit of sessions breached, one user must have at maximum 2 connections
		//NOTE: não deveria ser 2?
		if (session_ids.size() > 1) {
			std::cout << "Limit of sessions breached, one user must have at maximum 2 connections: " << username << std::endl;
			return false; 
		}

		session_ids.insert(id); 
		data_packets_map[id] = std::queue<std::shared_ptr<net::Payload>>(); 
		synched_files_at_start[id] = false; 

		std::cout << "ids: [";
		for(auto i: session_ids){
			std::cout << i << " ";
		}
		std::cout << "]" << std::endl;

		return true;
	}

	bool Controller::add_session(const std::string& username, int id){
		auto user_sessions = users_sessions.lock(); 
		if (user_sessions->count(username) == 0){
			user_sessions->try_emplace(username, username);
		}
		UserSession& user_session = user_sessions->at(username); 
		// Already logged, return true
		if (user_session.session_ids.count(id) != 0) {
			std::cout << "already logged in" << std::endl;
			return true;
		}
		return user_session.add_session(id); 
	}
	
	bool Controller::remove_session(const std::string& username, int id){
		auto user_sessions = users_sessions.lock();
		if (user_sessions->count(username) == 0){
			return false; 
		}
		auto& user_session = user_sessions->at(username); 
		bool ret = user_session.remove_session(id); 
		if (user_session.session_ids.size() == 0){
			user_sessions->erase(username); 
		}
		return ret; 
	}

	bool Controller::is_files_synched(const std::string& username, int id){
		auto user_sessions = users_sessions.lock();
		return user_sessions->at(username).is_files_synched(id);
	}

	void Controller::set_files_synched(const std::string& username, int id){
		auto user_sessions = users_sessions.lock();
		user_sessions->at(username).set_files_synched(id);
	}

	//throws:
	// std::out_of_range
	// std::runtime_error
	std::optional<std::shared_ptr<net::Payload>> Controller::get_data_packet(const std::string& username, int id){
		auto user_sessions = users_sessions.lock();
		if(user_sessions->count(username) == 0){
			throw net::CloseSessionException(); 
		}

		auto& session = user_sessions->at(username);
		if (session.session_ids.count(id) == 0){
			//"get_data_packet: Session " +  std::to_string(id) +  " not logged for user " +  username
			throw net::CloseSessionException(); 
		}
		auto& queue = session.data_packets_map[id];
		if (queue.empty()) {
			return std::nullopt; 
		}
		auto payload = queue.front();
		queue.pop();

		return std::make_optional(payload);
	}

	void Controller::add_data_packet(const std::string& username, std::shared_ptr<net::Payload> payload){
		auto user_sessions = users_sessions.lock();
		//adiciona a todos os ids que existem, pois um pacote tem q ser devolvido
		for(auto& v: user_sessions->at(username).data_packets_map){
			v.second.push(payload);
		}
	}
}