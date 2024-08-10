#ifndef MUTEX_HEADER
#define MUTEX_HEADER

#include <pthread.h>

namespace net{

    template <typename T>
    class Lock {
        public:
            inline void unlock(){
				pthread_mutex_unlock(mutex);
			}
            ~Lock(){
				unlock();
			}
            
			Lock(Lock const&) = delete;
			void operator=(Lock const&) = delete;

            inline T* operator->(){ return ptr; }
            inline T& get(){ return *ptr; }

        protected:
            Lock(T* ptr, pthread_mutex_t *mutex): ptr(ptr), mutex(mutex){
				pthread_mutex_lock(mutex);
			}

        private:
			T* ptr = nullptr;
			pthread_mutex_t *mutex;

        template <typename B> friend class Mutex;   
    };


    template <typename T>
    class Mutex {
        public:
            Mutex(T val): val(val), mutex(PTHREAD_MUTEX_INITIALIZER){} 
			inline Lock<T> lock(){
				return Lock(&val, &mutex);
			}

        private:
            T val;
			pthread_mutex_t mutex;
    };
}

#endif
