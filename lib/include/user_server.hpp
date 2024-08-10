#pragma once

#include <pthread.h>
#include <payload.hpp>
#include <queue>
#include <map>

#define MAX_SESSION_CONECTIONS 2

class UserServer {
    public:
        std::string get_username();
        void set_username(std::string s); 
        int get_session_connections_num(); 
        void add_session(int id);
        void remove_session(int id);
        void add_data_packet(std::shared_ptr<net::Payload> payload, int id);
        std::shared_ptr<net::Payload> get_data_packet(int id);
        bool is_logged(int id); 
        void unlock_packet();
        bool is_ready(int id);
        void set_ready(int id);
    private:
        pthread_mutex_t mutex_session_connection_num = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_t mutex_username = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_t mutex_data_packets = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_t mutex_synched_files_at_start = PTHREAD_MUTEX_INITIALIZER;
        std::map<int, std::queue<std::shared_ptr<net::Payload>>> data_packets_map;
        std::map<int, bool> synched_files_at_start;
        int session_connections = 0; 
        std::string username;
        std::vector<int> session_ids; //change to set
};