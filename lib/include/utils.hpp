#ifndef UTILS_HEADER
#define UTILS_HEADER

#include <string>
#include <algorithm> 
#include <random>
#include <cctype>
#include <locale>

#include "packet_generated.h"

namespace utils {
	//from https://stackoverflow.com/a/217605
	// trim from start (in place)
	inline void ltrim(std::string &s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
			return !std::isspace(ch);
		}));
	}

	// trim from end (in place)
	inline void rtrim(std::string &s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return !std::isspace(ch);
		}).base(), s.end());
	}

	inline void trim(std::string &s) {
		rtrim(s);
		ltrim(s);
	}
	inline std::string filename_without_path(const std::string &s){
		const size_t pos_of_slash = s.find_last_of('/');
		const size_t pos = pos_of_slash == std::string::npos ? 0 : (pos_of_slash + 1);
		return s.substr(pos, s.size() - pos);
	}
	inline std::string get_file_extension(const std::string &s){
		const size_t pos_of_dot = s.find_first_of('.');
		const size_t pos = pos_of_dot == std::string::npos ? 0 : pos_of_dot;
		return s.substr(pos, s.size() - pos);
	}
	inline bool is_tmp_file(const std::string &s){
		const size_t pos = s.find_last_of('.');
		//não tem extensão, não retorna
		if(pos == std::string::npos){
			return true;
		}

		const auto ext = s.substr(pos, s.size() - pos);

		/* Após análise das funções de POSIX referentes a nomeação de arquivos temporários (tmpnam, tmpfile, mkstemp e tempnam), */
		/* tmpnam -> gera arquivo com prefixo 'file', seguido de sequência alfanumérica */
		/* tmpfile, mkstemp, tempnam -> gera arquivo com prefixo definido pelo usuário, seguido de sequência alfanumérica */
		/* Portanto, tratar prefixação de arquivos temporários é o método ideal. */

		if (s.size() > 0) {
			const char firstChar = s.at(0);
			/* Se arquivo começa com um '.', '#', "~", é considerado arquivo temporário */
			if ((firstChar == '.') || (firstChar == '#') || (firstChar == '~')) {
				return true;
			}
		}

		/* Se a extensão do arquivo tem 4+ ou 0 caracteres, é considerado arquivo temporário */
		if ((ext.size() > 4) || (ext.size() == 0)) {
			return true;
		}

		static const std::vector<std::string> TEMPORARY_FILE_EXTENSIONS{".tmp", ".TMP", ".swp", ".swx"}; 

		for(const auto& tmp: TEMPORARY_FILE_EXTENSIONS){
			if(ext == tmp) return true;
		}
		return false;
	}


	std::string generate_random_alphanumeric_string(std::size_t len);
	const char* pckt_type_to_name(Net::Operation op);

	std::string get_sync_dir_path(const std::string& username);

	int random_number();

	void test_and_set_folder(const std::string& foldername); 
}

#endif