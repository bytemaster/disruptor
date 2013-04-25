#include <fc/thread/onetime_spin_lock.hpp>
#include <fc/thread/future.hpp>
#include <fc/thread/strand.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <exception>
#include <fc/fwd_impl.hpp>

#include <assert.h>

namespace fc {

namespace detail {
    class promise_impl
    {
      public:
      onetime_spin_lock    _lock;
      std::exception_ptr   _except;
      const char*          _what;
      context              _blocked_context;
    };
}
basic_promise::basic_promise( const char* w )
{ my->_what = w; } 
basic_promise::~basic_promise(){}

void basic_promise::_notify()
{
  { synchronized( my->_lock )
    if( my->_blocked_context._fiber != nullptr )
    {
         my->_blocked_context._strand->unblock( my->_blocked_context );
    }
  }
}

const char* basic_promise::what()const { return my->_what; }

void basic_promise::_wait()
{
   if( my->_lock.ready() ) return;

   // only one strand can block on a promise
   // to allow more than one strand would increase locking requirements and
   // greatly slow down the most common case.... to implement multiple
   // waiters, pick one strand to wait and then 'notify' everyone else.
   assert( my->_blocked_context._strand == nullptr ); 

   my->_blocked_context  = context::current();

   { synchronized( my->_lock )
       if( my->_lock.ready() ) return;
       my->_blocked_context._strand->block( my->_blocked_context, what() );  
   }
}

void basic_promise::set_exception( std::exception_ptr e )
{
   my->_except = std::move(e);
   _notify();
}

} // namespace fc
