#pragma once

namespace fc {
   template<typename T>
   class scoped_lock 
   {
      public:
         scoped_lock( T& l ):_lock(l) { _lock.lock();   }
         ~scoped_lock()               { _lock.unlock(); }
      private:
         T& _lock;
   };
}

#define synchronized(X) fc::scoped_lock<decltype((X))> _lock((X));
