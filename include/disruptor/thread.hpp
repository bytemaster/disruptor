#pragma once
#include "disruptor.hpp"
namespace disruptor
{
   namespace detail
   {
        struct functor
        {
            functor():callback(nullptr),destruct(nullptr){}
            void call(){ callback( _buffer ); }

            void (*callback)( void* self ); 
            void (*destruct)( void* self );
            char _buffer[256];
        };
        template<typename Functor>
        struct functor_invoker
        {
            static void run( void* self )
            {
                (*((Functor*)self))();
            }
            static void destruct( void* self )
            {
                ((Functor*)self)->~Functor();
            }
        };
        class thread_impl;
   }


   /**
    *   When there is shared state you require locks or atomics, the
    *   to get the highest possible performance it is best to assign
    *   shared resources to a single thread.
    *
    *
    *   A thread takes a set of read cursors and will 
    *   alternate calling event handlers for those cursors when
    *   they have data available.  
    *
    *   The effect is to cooperatively share a thread.  
    */
   class thread
   {
      public:
         thread();
         ~thread();
         
         /**
          *  Is passed the range [begin,end) to process and
          *  returns the first unprocessed element.  If all
          *  elements are processed then it should return end.
          *
          *  If no elements are processed then it should return
          *  begin.
          *
          *  If N elements are processed then it should return
          *  begin + N
          */
         typedef std::function< int64_t( int64_t, int64_t)> handler;

         /**
          *  Performs thread-safe posting of events to the thread.
          */
         template<typename Functor>
         void atomic_post( Functor&& f )
         {
            static_assert( sizeof(f) < 256, "Functor's must be smaller than 256 bytes" );
            int64_t          slot = post_cursor->claim(1);
            detail::functor& func = post_buffer.at(slot);

            // construct the functor inplace
            new (func._buffer) Functor( std::forward<Functor>(f) );
            func.callback = &detail::functor_invoker<Functor>::run;
            func.destruct = &detail::functor_invoker<Functor>::destruct;

            post_cursor->publish_after( slot, slot - 1 );
         }

         /** 
          * For single producer this is safe to call, if more than one
          * thread may be posting events at the same time then all threads
          * must use atomic_post() instead.
          */
         template<typename Functor>
         void post( Functor&& f )
         {
            static_assert( sizeof(f) < 256, "Functor's must be smaller than 256 bytes" );

            int64_t slot          = post_cursor->wait_next();
            detail::functor& func = post_buffer.at(slot);

            // construct the functor inplace
            new (func._buffer) Functor( std::forward<Functor>(f) );
            func.callback = &detail::functor_invoker<Functor>::run;
            func.destruct = &detail::functor_invoker<Functor>::destruct;
            
            post_cursor->publish(slot);
         }


         void add_cursor( read_cursor_ptr c, handler h  );

         void start();
         void stop();
         void join();

      private:
         shared_write_cursor_ptr                post_cursor;
         ring_buffer<detail::functor,256>       post_buffer;
         std::unique_ptr<detail::thread_impl>   my;
   };
}
