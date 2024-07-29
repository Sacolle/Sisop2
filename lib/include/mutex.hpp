#ifndef MUTEX_HEADER
#define MUTEX_HEADER

#include <pthread.h>

namespace sync {
    
    template <typename T>
    class Lock {
        public:
            inline unlock(){
				pthread_mutex_unlock(&mutex);
			}
            ~Lock(){
				unlock();
			}

            inline T* operator->(){ return ptr; }
            inline T* get(){ return ptr; }

        protected:
            Lock(T* ptr, pthread_mutex_t mutex): ptr(ptr), mutex(mutex){
				pthread_mutex_lock(&mutex);
			}

        private:
			T* ptr = nullptr;
			pthread_mutex_t mutex;

        friend class Mutex<T>;   
    };



    template <typename T>
    class Mutex {
        public:
            Mutex(T* ptr): ptr(ptr), mutex(PTHREAD_MUTEX_INITIALIZER){} 
            ~Mutex(){
				delete ptr;
			}

			inline Lock<T> lock(){
				return Lock(ptr);
			}

        private:
            T* ptr = nullptr;
			pthread_mutex_t mutex;
    };
}

#endif
