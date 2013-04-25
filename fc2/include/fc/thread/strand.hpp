#pragma once
#include <fc/thread/future.hpp>
#include <fc/thread/fiber.hpp>
#include <fc/thread/functor.hpp>

namespace fc {
namespace detail
{
  class strand_impl;
}
class thread;

thread& get_current_thread();

/**
 *  @brief a group of operations that may not execute at the same time.
 *
 *  Functors posted to a strand may be executed by any thread, but only one
 *  thread at a time will be executing the functors.  Functors may yield and
 *  cooperatively block, therefore each strand has a set of fibers that it
 *  manages.
 *
 *  When a strand runs out of work and needs to 'block' until a timeout then
 *  it places itself into the shared 'timer queue'.  Which the real threads
 *  check on a regular basis and when that timeout expires the strand is
 *  picked up by the first available real thread to continue work.
 *
 *  When all of a strands fibers are 'blocked' there are no tasks or timeouts,
 *  then the strand simply 'returns control' to the thread which picks up
 *  the next available strand and runs with it.  
 *
 *  When a strand is in this 'blocked state' and a new task is posted or
 *  a fiber is 'unblocked' then the strand re-inserts itself into the global
 *  shared strand queue to be executed by the next available thread.
 *
 *  The global ready strand queue is a ring buffer of fixed size.  An atomic
 *  counter is used for threads to 'claim their slot', a thread cannot start
 *  executing the slot it has claimed until the 'published writer' sequence
 *  number marks the slot as ready. 
 *
 *  If claimed strands > published strands then a real thread is blocked waiting
 *  for work and needs to be woken up when a new strand is available.
 *
 *
 *  Where are the atomic operations:
 *    1) onetime lock on future publishing  add_fetch
 *    2) publish claim counter, hit every time a strand is published add_fetch
 *    3) execute claim counter, hit every time a strand yields. add_fetch
 *    4) sleep thread claim publishing add_fetch
 *
 *  Where are the locks:
 *    1) when there are no ready strands, a thread will lock a mutex
 *        and increment a 'wait counter', then check the published
 *        index one last time before sleeping.
 *
 *        On waking, wait counter will be decremented.
 *
 *    2) On post, save wait counter,
 *       update publish pos
 *       if saved wait counter > 0, 
 *         ??? grab mutex ??? ... if this can be avoided it would help producers a lot..
 *       - notify all, 
 *
 *
 *  A publisher cannot publish to a slot
 *  until the executor has 'claimed it'. In practice all we need to do is
 *  update the sequence producer sequence number and the executor will wait
 *  until it is ready.... producers would then track how many total have
 *  been consumed to catch 'wrapping'... but wrapping should never be a problem, 
 *  assuming we have an upper limit of total 'strands' as each 'strand' has
 *  its own 'fixed size' ring buffer they are not cheap.
 */
class strand
{
  public:
     strand( const char* name = "" );
     ~strand();
     template<typename Functor>
     auto async( Functor&& f, const char* desc = "" ) -> future<decltype(f())>
     {
         typedef decltype(f()) Result;
         typename promise<Result>::ptr prom = std::make_shared< promise<Result> >(desc);
         post( [=](){ 
            try
            {
                prom->set_value( f() );
            }
            catch(...)
            {
                prom->set_exception( std::current_exception() );
            }
         });
         return future<Result>(prom);
     }

     template<typename Functor>
     auto await( Functor&& f, const char* desc = "" ) -> decltype(f());

     template<typename Functor>
     void post( Functor&& f );

     /**
      * Processes tasks on this strand until everything
      * is 'blocked'.
      *
      * @return true if *something* was run, false if there was nothing to do
      */
     bool run();

     /**
      *  Cancels all operations on the strand and
      *  prevents new operations from being posted.
      */
     void cancel();

     /**
      *   Waits for all fibers to exit and for the
      *   task queue to be empty.
      */
     void wait();

  private:
     friend class basic_promise;
     void block( context& f, const char* desc = "");
     void unblock( context& f );
     void notify();
     void yield();

     int64_t                claim();
     functor<104>&          get_slot( int64_t );
     void                   publish( int64_t p );

     std::unique_ptr<detail::strand_impl>   my;
};



/**
 *  Performs thread-safe posting of events to the thread.
 */
template<typename Functor>
void strand::post( Functor&& f )
{
   static_assert( sizeof(f) < 104, "Functor's must be smaller than 104 bytes" );
   int64_t  next_slot = claim();
   get_slot(next_slot) = std::forward<Functor>(f);

   publish(next_slot);
   notify();
}

/**
 *  Starts a new async fiber and returns immediately.
 */
//template<typename Functor>
//auto strand::async( Functor&& f, const char* desc ) -> future<decltype(f())>;

/**
 *  Blocks the calling fiber until *this thread 
 *  produces the desired result. This method
 *  is much more effecient than async() because
 *  the functor does not need to be copied nor
 *  are there any dynamic memory allocations.
 */
template<typename Functor>
auto strand::await( Functor&& f, const char* desc  ) -> decltype(f())
{
    typedef decltype(f()) Result;
    optional<Result>      _result;
    std::exception_ptr    _except;
    context&              _current_ctx = context::current();

    block( _current_ctx, desc );
     
    // capture by reference
    post( [&](){
       try 
       {
          _result = f();
       } 
       catch( ... )
       {
          _except = std::current_exception();
       }
       _current_ctx._strand->unblock( _current_ctx );
    });

    yield();

    if( _except != std::exception_ptr() ) 
       std::rethrow_exception(_except);
    return *_result;
}



} // namespace fc
