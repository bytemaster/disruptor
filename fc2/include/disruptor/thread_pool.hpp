#pragma once
#include <fc/thread/future.hpp>

namespace fc 
{

/**
 *  Manages a set of threads to process strands and
 *  free operations.
 */
class thread_pool
{
   public:
     static void start( uint32_t num_threads = 8 );

     // post operations that are free from any strand/thread
     template<typename Functor>
     static auto async( Functor f, const char* desc = "" ) -> future<decltype(f())>;

     // post operations that are free from any strand/thread
     template<typename Functor>
     static auto await( Functor f, const char* desc = "" ) -> decltype(f());

     // post operations that are free from any strand/thread
     template<typename Functor>
     static void post( Functor&& f );

     static thread_pool& instance();
   private: 
     std::unique_ptr<thread_pool_impl> my;
};


} // namespace fc
