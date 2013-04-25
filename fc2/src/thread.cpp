#include <disruptor/thread.hpp>
#include <boost/thread.hpp>
#include <boost/context/all.hpp>
#include <boost/coroutine/stack_allocator.hpp>

namespace disruptor
{

namespace detail {

namespace bc  = boost::context;
namespace bco = boost::coroutines;

struct sleep_timer
{
    time_point  _expires;
    fiber*      _fiber;
};
bool operator < ( const sleep_timer& a, const sleep_timer& b )
{
   return a._expires < b._expires;
}

class fiber
{
   public:

     fiber()
     {
     }

     fiber( void (*sf)(intptr_t), bco::stack_allocator& alloc, fc::thread* t )
     :stack_alloc(&alloc)
     {
         size_t stack_size =  bco::stack_allocator::default_stacksize();
         my_context = bc::make_fcontext(alloc.allocate(stack_size), stack_size, sf);
     }

     ~fiber()
     {
         if( stack_alloc != nullptr )
            stack_alloc->deallocate( my_context->fc_stack.sp, bco::stack_allocator::default_stacksize() );
         else
          delete my_context;
     }



     bco::stack_allocator*        stack_alloc;
     bc::fcontext_t*              my_context;
};


class thread_impl
{
   public:
      thread*                         _self;
      fiber*                          _current_fiber;

      bco::stack_allocator            _stack_alloc;

      shared_write_cursor_ptr         _unblock_write_cursor;
      read_cursor_ptr                 _unblock_read_cursor;

      // a buffer for receving unblock notifications from other threads.
      ring_buffer<fiber*,1024>        _unblock_buffer;

      std::vector<fiber*>             _free_list; // fibers that have exited.
      std::vector<fiber*>             _blocked_fibers;
      std::dequeue<fiber*>            _ready_fibers;
      std::vector<sleep_timer>        _sleep_heap;

      std::unique_ptr<boost::thread>  _thread;
      bool                            _done;
      read_cursor_ptr                 _read_post_cursor;

      boost::mutex                    _task_ready_mutex;
      boost::condition_variable       _task_ready;


     static void start_fiber( intptr_t my ) 
     {
        thread_impl* self = (thread_impl*)my;
        try {
           self->run();
        } catch ( ... ) {
           assert( !"fiber exited with uncaught exception" );
        }
        self->_free_list.push_back( my->_current_fiber );
        start_next_fiber();
     }


      void sleep_until( const time_point& tp )
      {
         _sleep_heap.push_back( sleep_timer( &self->current_fiber(), tp ) );
         std::push_heap( _sleep_heap.begin(), _sleep_heap.end() );
         start_next_task();
      }
      
      /** moves unblocked fibers from the blocked list 
       * into the ready list
       */
      void process_unblock_ring()
      {
         int64_t pos = _unblock_read_cursor.begin();
         int64_t end = _unblock_read_cursor.end();
         while( pos < end )
         {
            self->unblock( *_unblock_buffer.at(pos) );
            ++pos;
         }
         _unblock_read_cursor->publish(pos - 1);
      }

      bool switch_to( fiber* f ) 
      {
          if( f != _current_fiber ) 
            return false;
          auto prev = _current_fiber;
          _current_fiber = f;
          bc::jump_fcontext( prev->my_context, _current_fiber->my_context, 0 );
          return true;
          // I am now in context of f... I should check exceptions... 
      }


      bool start_ready_fiber()
      {
           process_unblock_ring();      // these tasks get priority
           process_timeout();           // timeouts are even higher priority than unblocked
           if( _ready_fibers.size() )   // if any fibers are ready, switch to one of them.
           {
              auto next = _ready_fibers.front();
              _ready_fibers.pop_front();
              return switch_to( next );
           }
           return false;
      }

      /** called when the current thread must yield, returns next time this
       * fiber is scheduled / unblocked... 
       *
       *   @pre - caller has blocked and cannot continue (but has placed itself on a list someplace)
       *   @post - a new context that is ready to execute takes over.
       **/
      bool start_next_fiber()
      {
           if( start_ready_fiber() )
           {
              return true;
           }
           else if( _idle_fibers.size() ) // fibers that are ready to run the next posted task
           {
              auto next = _idle_fibers.front();
              _idle_fibers.pop_front();
              return switch_to( next );
           }
           else 
           {
              // start a new fiber to call run and process said event... 
              fiber* f = new fiber( &thread_impl::start_fiber, _stack_alloc, this );
              return switch_to( f );
           }
      }


      /**
       *  Main loop... 
       */
      void run()
      {
          int spin_count = 0;
          while( !_done )
          {
              // save our context just in case we switch to a
              // ready fiber... 
              _idle_fibers.push_front( _current_fiber );
              if( !start_ready_fiber() )
              {
                 _idle_fibers.pop_front(); // we didn't switch so nothing changed, remove ourself from ready.
                 auto pos = _read_post_cursor.begin();
                 if( pos != _read_post_cursorpost_buffer.end() )
                 {
                    self->_post_buffer.at(pos)();
                    self->_post_buffer.at(pos).destruct();
                    _read_post_cursor->publish(pos);
                    spin_count = 0;
                 }
                 else // no ready fibers, no posted jobs... keep checking.
                 {
                    ++spin_count;
                    if( spin_count > 1000 )
                    {
                      { // lock scope
                         boost::unique_lock<boost::mutex> lock(_task_ready_mutex);
                         _posted_messages.store( 0, boost::memory_order_release );
                        
                         if( timeout_time == time_point::max() ) 
                         {
                           task_ready.wait( lock );
                         } 
                         else if( timeout_time != time_point::min() ) 
                         {
                           task_ready.wait_until( lock, boost::chrono::system_clock::time_point() + 
                                                        boost::chrono::microseconds(timeout_time.time_since_epoch().count()) );
                         }
                      }
                      // TODO: limit sleep time to next timeout interval...
                      // Switch sleeping to waiting on a mutex.
                      // ::usleep( std::min<int64_t>( spin_count >> 4,  40*1000) );
                    }
                    continue;
                 }
              }
              spin_count = 0;
          }
      }
}; // class thread_impl

} // namespace detail


/**
 *  This thread may be 'blocked' waiting on events in which case we need to
 *  signal for it to 'wake up'.  We do not want every 'poster' to have
 *  to block checking to see whether or not the thread needs to be notified.
 *
 *  When the thread wishes to 'yield' it will grab the lock, set the
 *  _posted_messages count to 0, check one last time for posted messages,
 *  then wait to be notified.
 *
 *  When a thread posts an event it can know that if _posted_messages is not
 *  0 then the thread will see its event.  If it is 0 then that means
 *  the thread has been blocked and it needs to be 'notified'.  It is
 *  possible for multiple producers to both post 'at the same time' in
 *  which case we do not want them to both block attempting to aquire
 *  the _task_ready_mutex so both threads attempt an atomic increment and
 *  the one which increments from 0 to 1 gets the privilage of notifying
 *  the thread that there are new messages available.
 *
 *  For this to work the following must be true:
 *  - the posting thread sees 0 posted messages before this thread checks 
 *    for the last time.  
 */
void thread::notify()
{
    /** TODO: can memory_order_aquire be relaxed? */
    if( this != &current() &&  my->_posted_messages.load( boost::memory_order_aquire ) == 0 )
    {
         if( 0 == my->_posted_messages.fetch_add(1) )
         {
            boost::unique_lock<boost::mutex> lock(my->_task_ready_mutex);
            my->_task_ready.notify_one();
         }
    }
}

void thread::block( fiber& f )
{
   assert( this == &current() );
   _blocked_fibers.push_back(&f);
}

void thread::unblock( fiber& f )
{
  if( this == &current() )
  {
      auto itr = std::find( my->_blocked_fibers.begin(); my->_blocked_fibers.end(), &f );
      assert( itr != my->_blocked_fibers.end() );
      my->_blocked_fibers.erase(itr);
      my->_ready_fibers.push_front( &f );
  }
  else
  {
      int64_t   slot = my->_unblock_write_cursor->claim(1);
      my->_unblock_buffer.at(slot) = &f;
      my->_unblock_write_cursor->publish_after( slot, slot - 1 );
  }
}



thread::thread( const char* name )
:my( new detail::thread_impl() )
{
   my->_self = this;
   my->_name = name;
   my->_done = false;
   my->_current_fiber = nullptr;

   my->_read_post_cursor = std::make_shared<read_cursor>();
   post_cursor           = std::make_shared<shared_write_cursor>(post_buffer.get_buffer_size());
   my->_unblock_read_cursor  = std::make_shared<read_cursor>();
   my->_unblock_write_cursor = std::make_shared<shared_write_cursor>(my->_unblock_buffer.get_buffer_size());

   post_cursor->follows( my->_read_post_cursor );
   my->_read_post_cursor->follows( post_cursor );
   my->_unblock_write_cursor->follows( my->_unblock_read_cursor );
   my->_unblock_read_cursor->follows( my->_unblock_write_cursor );

   my->_thread.reset( new boost::thread( [=](){ my->run(); } ) );
}

thread::~thread()
{
   join();
}


thread*& current_thread() 
{
   #ifdef _MSC_VER
      static __declspec(thread) thread* t = NULL;
   #else
      static __thread thread* t = NULL;
   #endif
   return t;
}

thread& thread::current() 
{
  if( !current_thread() ) current_thread() = new thread();
  return *current_thread();
}

detail::fiber& thread::current_fiber() 
{
  if( !my->_current_fiber ) my->_current_fiber = new detail::fiber();
  return *(my->_current_fiber);
}

void thread::join()
{
   if( !my->_done )
   {
      assert( my->_thread );
      my->_done = true;
      my->_thread->join();
      my->_thread.reset();
   }
}

void yield()
{
   sleep_until( time_point::now() + fc::microseconds(100) );
}
void usleep( const microseconds& usec )
{
   sleep_until( time_point::now() + usec );
}
void sleep_until( const time_point& t )
{
   thread::current().my->sleep_until(t);
}

/**
 *  Prevents all future tasks from being posted, cancels
 *  any blocked tasks, and exits the thread. 
 */
void thread::join()
{
  my->_done = true;
  my->_thread->join();
}

} // namespace disruptor
