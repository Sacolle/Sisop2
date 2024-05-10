#include "utils.hpp"

#define TOSTR(x) # x
#define ENUM_TO_STR_CASE(val) case val: return TOSTR(val) 
namespace utils {
	const char* pckt_type_to_name(Net::Operation op){
		switch (op){
			ENUM_TO_STR_CASE(Net::Operation_Connect);
			ENUM_TO_STR_CASE(Net::Operation_FileMeta);
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
}
