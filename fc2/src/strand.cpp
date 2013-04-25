#include <fc/thread/strand.hpp>
#include <fc/thread/thread_pool.hpp>
#include <fc/disruptor/cursor.hpp>
#include <fc/disruptor/ring_buffer.hpp>
#include <dequeue>

#include <boost/context/all.hpp>
#include <boost/coroutine/stack_allocator.hpp>

namespace detail
{
  namespace bc  = boost::context;
  namespace bco = boost::coroutines;
  using namespace disruptor;

  class strand_impl
  {
     public:
       strand*                            _self;
       fiber*                             _cur_fiber;
       fiber*                             _run_loop;

       const char*                        _name;
       ring_buffer< functor<104>, 128 >   _post_buffer;
       shared_write_cursor                _write_cursor;
       read_cursor                        _read_cusror;
       std::dequeue<fiber*>               _ready_fibers;

       bco::stack_allocator               _stack_alloc;

       strand_impl( const char* n )
       :_name(n),_write_cursor(128)
       {
          _read_cursor->follows( *_write_cursor );
          _write_cursor->follows( *_read_cursor );
       }

       fiber* current_fiber()
       {
          if( !_cur_fiber ) 
             return _cur_fiber = new fiber();
          return _cur_fiber;
       }

       /**
        *  
        */
       void yield()
       {
           process_unblock_ring(); // move these to front of ready queue
           process_timeout(); // timeouts are even higher priority than unblocked

           if( _ready_fibers.size() )
           {
              auto next = _ready_fibers.front();
              _ready_fibers.pop_front();
              next->resume();
              thread& cur_thread = 
              thread::set_current_fiber( _cur_fiber );
              thread::set_current_strand( _self );
              return;
           }
           else if( read_cursor.begin() < read_cursor.end() ||
                    read_cursor.begin() < read_cursor.check_end() )
           {
              fiber* f = new fiber( &strand_impl::start_fiber, _stack_alloc, this );
              f->start();
              thread::set_current_fiber( _cur_fiber );
              thread::set_current_strand( _self );
              return;
           }
           else
           {
              _run_loop->resume();
           }
       }
  };
}


strand::strand( const char* name = "" )
:my( new detail::strand_impl(name) )
{
}

strand::~strand()
{
   // clean up _post_buffer
   // clean up _ready_fibers
}

/**
 *  Runs tasks until there is nothing left to do.
 */
void strand::run()
{

}

int64_t strand::claim()
{
   return my->_post_cursor->claim(1);
}

functor& strand::slot( int64_t slot )
{
   return my->_post_buffer.at(slot);
}

void strand::publish( int64_t slot )
{
    my->_post_cursor->publish_after( slot, slot - 1 );
}

void strand::yield()
{
   my->yield();
}

void strand::notify()
{
   // if this strand is 'idle' then it needs to be put
   // into the thread pool queue for execution asap
   // if it is active then nothing to do here...
}
