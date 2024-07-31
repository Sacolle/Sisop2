#include "controller.hpp"

namespace net{

	UserSession::UserSession(const std::string& username): 
		username(username), 
		synched_files_at_start(Mutex(new std::map<int, bool>())),
		session_ids(Mutex(new std::set<int>())){}


	int UserSession::get_session_connections_num(){
		auto ids = session_ids.lock();
		return ids->size();
	}
	bool UserSession::has_session(int id){
		auto ids = session_ids.lock();
		return ids->count(id) == 1;
	}
}