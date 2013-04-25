#pragma once
#include <boost/atomic.hpp>

/**
 *  @brief Locks the 'first access' to a critical section, subsequent accesses are 'free'
 *
 *  The use case for the onetime spin lock is when there is a race to initialize some
 *  state after which there is no longer any conflict.  The typical solution involves
 *  using a lock, often a spinlock.  
 *
 *  A spinlock is too heavy weight though because it is based on CAS which is
 *  an expensive operation with two potential cache hits (read/write).  When
 *  CAS fails the result is a 'loop' on CAS ops which could lead to a branch
 *  misprediction.
 *
 *  The algorithm used by the onetime_spin_lock requires in the best/common case, 
 *  1 atomic operation that cannot fail, fetch_add,  If there is a very close
 *  race then at most 1 atomic add per thread.  After that one 'always successful'
 *  operation, all other operations are simply a polling 'read loop'. 
 */
class onetime_spin_lock
{
   public:
      onetime_spin_lock();
      bool try_lock() 
      { 
         return ready() || 0 == _state.fetch_add(1); // lock + add 0 gives you the lock.
      }

      /**... these methods don't make sense on a spinlock... 
      bool try_lock_for( const microseconds& rel_time )
      {
         return try_lock_until( fc::time_point::now() + rel_time ); 
      }

      bool try_lock_until( const fc::time_point& abs_time )
      {
         while( !try_lock() && abs_time > time_point::now() ){}
         return try_lock();
      }
      */

      void lock() 
      {
         while( !try_lock() ){
            /* Pause instruction to prevent excess processor bus usage */ 
            #define cpu_relax() asm volatile("pause\n": : :"memory")
            cpu_relax();
         }
      }
      void unlock() 
      {
         _state.store( -100000, boost::memory_order_release );
      }
      /** return true after a successful lock/unlock by any thread */
      bool ready()const 
      { 
         return reinterpret_cast<const int&>(_state) < 0  || // attempt non memory order, no cache sync
                _state.load( boost::memory_order_acquire) < 0; // attempt with memory order
      }
   private:
      boost::atomic<int>  _state;
};
