#ifndef EXCEPTIONS_MODULE
#define EXCEPTIONS_MODULE

#include <exception>
#include <stdexcept>
#include <string>

namespace net {
	class NetworkException : public std::runtime_error{
		public:
			NetworkException(const std::string& what) : std::runtime_error(what){}
	};
	class ReceptionException : public std::runtime_error {
		public:
			ReceptionException(const std::string& what) : std::runtime_error(what){}
	};
	class TransmissionException : public std::exception {};
	class CloseConnectionException : public std::exception {};
}

#endif 