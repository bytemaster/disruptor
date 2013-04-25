#pragma once
#include <fc/thread/fiber.hpp>
#include <fc/thread/strand.hpp>
#include <fc/time.hpp>
#include <memory>

namespace fc
{
   namespace detail 
   { 
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
   class thread : public strand
   {
      public:
         thread( const char* name = "" );
         ~thread();

         /**
          *   Start a new OS level thread that will
          *   process tasks until quit() is called.
          *
          *   Returns immediately.
          *
          *   @note may not be called again until after join()
          *   has been called.
          */
         void exec();

         /**
          *  Causes exec() to exit after
          *  canceling all tasks.
          */
         void quit();

         /**
          * Waits for the thread to exit. 
          * This will hang until quit() has been called.
          */
         void join();

         const char*      name()const;
         static thread&   current()const;
         fiber&           current_fiber()const;
         strand&          current_strand()const;

      private:
         thread( detail::thread_impl* m );
         std::unique_ptr<detail::thread_impl> my;
   };

   void usleep( const microseconds& t );
   void sleep_until( const time_point&  t );

} // namespace fc




