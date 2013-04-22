#include <disruptor/thread.hpp>
#include <boost/thread.hpp>

namespace disruptor
{

struct cursor_handler
{
   int64_t                   pos;
   int64_t                   end;
   int64_t                   max_batch;
   read_cursor_ptr           cur;
   thread::handler           call;


   cursor_handler( read_cursor_ptr c, thread::handler h )
   :pos(c->begin()),
    end(c->end()),
    max_batch(10),cur(c),call(h){}

};

namespace detail {
class thread_impl
{
   public:
      thread*                        _self;
      std::unique_ptr<boost::thread> _thread;
      bool                           _done;
      std::vector<cursor_handler>    _handlers;
      read_cursor_ptr                _read_post_cursor;


      void run()
      {
         uint64_t spin_count = 0;
         while( !_done )
         {
             bool inc_spin = true;
             for( uint32_t i = 0; i < _handlers.size(); ++i )
             {
                 cursor_handler& current = _handlers[i];
                 if( current.pos == current.end )
                 {
                     current.cur->publish( current.pos - 1 );
                     current.end = current.cur->check_end();
                 }
                 if( current.pos < current.end )
                 {
                    auto next = current.call( current.pos, 
                                              std::min(current.end, 
                                                       current.pos+current.max_batch) );
                    if( next > current.pos )
                    {
                       // we made progress
                       current.pos   = next;
                       inc_spin      = false;
                       spin_count    = 0;
                    }
                 }
             }
             spin_count += inc_spin;

             // progress backoff... until a maximum of 10 ms 
             if( spin_count > 1000 ) usleep( std::min<int64_t>( spin_count >> 4,  40*1000) );
         }
      }

};
} // namespace detail

thread::thread()
:my( new detail::thread_impl() )
{
   my->_self = this;
   my->_done = true;

   my->_read_post_cursor = std::make_shared<read_cursor>();
   post_cursor      = std::make_shared<shared_write_cursor>(post_buffer.get_buffer_size());

   post_cursor->follows( my->_read_post_cursor );
   my->_read_post_cursor->follows( post_cursor );

   add_cursor( my->_read_post_cursor, 
      [=]( int64_t begin, int64_t end )  -> int64_t 
      {
         try
         {
            post_buffer.at(begin).call();
            return begin + 1;
         } 
         catch ( ... ) 
         {
            my->_read_post_cursor->set_alert( std::current_exception() );
            return begin;
         }
      });
}

thread::~thread()
{
}

void thread::add_cursor( read_cursor_ptr c, handler h  )
{
   assert( my->_done && "handlers must be added before starting the thread" );
   my->_handlers.push_back( cursor_handler( c, h ) );
}

void thread::start()
{
   assert( my->_done && "thread already running" );
   my->_done = false;
   my->_thread.reset( new boost::thread( [=](){ my->run(); } ) );
}

void thread::stop()
{
   my->_done = true;
}
void thread::join()
{
   assert( my->_thread );
   my->_thread->join();
}


} // namespace disruptor
