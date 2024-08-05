#include "utils.hpp"
#include <iostream> 

#include <string>
#include <filesystem>
#include <pthread.h>

#define TOSTR(x) # x
#define ENUM_TO_STR_CASE(val) case val: return TOSTR(val) 
namespace utils {
	const char* pckt_type_to_name(Net::Operation op){
		switch (op){
			ENUM_TO_STR_CASE(Net::Operation_Connect);
			ENUM_TO_STR_CASE(Net::Operation_FileMeta);
			ENUM_TO_STR_CASE(Net::Operation_SendFileRequest);
			ENUM_TO_STR_CASE(Net::Operation_FileData);
			ENUM_TO_STR_CASE(Net::Operation_ListFiles);
			ENUM_TO_STR_CASE(Net::Operation_Exit);
			ENUM_TO_STR_CASE(Net::Operation_Download);
			ENUM_TO_STR_CASE(Net::Operation_Delete);
			ENUM_TO_STR_CASE(Net::Operation_Response);
			ENUM_TO_STR_CASE(Net::Operation_Ping);
			default: return "Unkown packet";
		}
	}
	//https://stackoverflow.com/a/444614
	template <typename T = std::mt19937> 
	auto random_generator() -> T {
		auto constexpr seed_bytes = sizeof(typename T::result_type) * T::state_size;
		auto constexpr seed_len = seed_bytes / sizeof(std::seed_seq::result_type);
		auto seed = std::array<std::seed_seq::result_type, seed_len>();
		auto dev = std::random_device();
		std::generate_n(begin(seed), seed_len, std::ref(dev));
		auto seed_seq = std::seed_seq(begin(seed), end(seed));
		return T{seed_seq};
	}
	std::string generate_random_alphanumeric_string(std::size_t len) {
		static constexpr auto chars =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
		thread_local auto rng = random_generator<>();
		auto dist = std::uniform_int_distribution{{}, std::strlen(chars) - 1};
		auto result = std::string(len, '\0');
		std::generate_n(begin(result), len, [&]() { return chars[dist(rng)]; });
		return result;
	}

	std::string get_sync_dir_path(std::string username){
		std::string s("sync_dir_");
		s.append(username);
		return s;
	}

	int random_number() {
		srand(time(NULL));
		return rand();
	}

	void test_and_set_folder(const std::string& username){
		std::string foldername = get_sync_dir_path(username); 
        //NOTE: creio q o inicializador estatico deve dar certo,
        // e n dar problema com o inicilizador do mutex
        static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

        pthread_mutex_lock(&mutex);
        // Create directory if it doesn't exist
        if (!std::filesystem::exists(foldername)) {
            std::filesystem::create_directory(foldername);
        }
        pthread_mutex_unlock(&mutex);
    }
}
