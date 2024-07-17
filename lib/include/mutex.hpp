#ifndef MUTEX_HEADER
#define MUTEX_HEADER

namespace sync {
    
    template <typename T>
    class Lock {
        public:
            unlock();
            ~Lock();

            T operator->();
            T get();

        protected:
            Lock();

        private:


        friend class Mutex<T>;   
    };



    template <typename T>
    class Mutex {
        public:
            Mutex(T item);
            ~Mutex();



        private:
            T item;
    
        
    };




}

#endif
