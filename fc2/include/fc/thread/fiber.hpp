#pragma once
#include <fc/fwd.hpp>
#include <fc/thread/functor.hpp>

namespace fc
{
  namespace detail
  {
    class fiber_impl;
  }

  class thread;
  class strand;
  class fiber;

  /** thread local state */
  class context
  {
    public:
     context():_thread(nullptr),_strand(nullptr),_fiber(nullptr),_block_desc(""),_id(-1){}
     thread*      _thread;
     strand*      _strand;
     fiber*       _fiber;
     const char*  _block_desc;  // what is blocking this context
     int          _id;

     static context& current();
  };



  /**
   *  Provides a basic 'coroutine' that takes no arguments and
   *  generates no results 'directly'.  
   */
  class fiber
  {
    public:
      fiber();
      ~fiber();

      fiber( functor<> start, size_t stack_size = fiber::default_stack_size() );

      /** return true if the fiber is 'complete' or throw an exception
       *  if the fiber threw an exception.
       */
      bool start();

      /**
       *  return true if the fiber is complete or throw an exception
       *  if the fiber threw an exception.
       */
      bool resume();

      /**
       *  Returns the fiber that last called start() or resume() and
       *  to whom control will be returned when the fiber exits.
       */
      fiber* caller()const; 

      /**
       *  Jumps to another fiber.  This can only be called 
       *  from the current fiber.
       */
      bool yield_to( fiber& f  );

      /** 
       * Shorthand for yield_to( *caller() ) 
       */
      void yield();

      /**
       *   Accesses the 'thread local'
       */
      static fiber& current();
      static size_t default_stack_size();

      bool done()const;

    private:
      fc::fwd<detail::fiber_impl,184> my;
  };
} // namespace fc


