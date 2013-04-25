#include <boost/context/all.hpp>
#include <boost/coroutine/stack_allocator.hpp>
#include <fc/thread/fiber.hpp>
#include <fc/fwd_impl.hpp>

namespace fc
{
  namespace bc  = boost::context;
  namespace bco = boost::coroutines;
  namespace detail
  {
     class fiber_impl
     {
       public:
        fiber_impl()
        :_context(nullptr),
         _caller(nullptr)
        {
        }

        bc::fcontext_t*    _context;
        fiber*             _caller;
        functor<>          _functor;
        std::exception_ptr _exit_except;
        bool               _done;
      
        static void start( intptr_t s )
        {
           fiber_impl* self = reinterpret_cast<fiber_impl*>(s);
           try {
             self->_functor.call();
           } 
           catch ( ... )
           {
             self->_exit_except = std::current_exception();
           }
           self->_done  = true;
           assert( self->_caller );
           reinterpret_cast<fiber*>(self)->yield_to(*self->_caller);
        }
     };
  }

  bool fiber::done()const { return my->_done; }
  context& context::current()
  {
      #ifdef _MSC_VER
         static __declspec(context) context* t = NULL;
      #else
         static __thread context* t = NULL;
      #endif
      if( !t ) t = new context();
      return *t;
  }

  bco::stack_allocator& get_stack_alloc()
  {
    static bco::stack_allocator self;
    return self;
  }

  fiber::fiber()
  {
      my->_context = new bc::fcontext_t();
  }

  fiber::~fiber()
  {
      get_stack_alloc().deallocate( my->_context->fc_stack.sp, my->_context->fc_stack.size );
      // TODO:... if I allocated _context... delete it...
  }

  fiber::fiber( functor<> start, size_t stack_size  )
  {
      my->_functor = std::move(start);
      my->_context = bc::make_fcontext(get_stack_alloc().allocate(stack_size), stack_size, &detail::fiber_impl::start);
  }

  /** return true if the fiber is 'complete' or throw an exception
   *  if the fiber threw an exception.
   */
  bool fiber::start()
  {
    assert( !done() );
    context& cur = context::current();
    if( !cur._fiber ) 
      cur._fiber = new fiber();

    my->_caller = cur._fiber;
    cur._fiber = this;
    auto r = bc::jump_fcontext( my->_caller->my->_context, my->_context, (intptr_t)&my );
    context* c = reinterpret_cast<context*>(r);
    // I may have woken up in a new thread...
    assert( c == &context::current() );
    if( my->_exit_except ) std::rethrow_exception( my->_exit_except );
    return my->_done;
  }

  /**
   *  return true if the fiber is complete or throw an exception
   *  if the fiber threw an exception.
   */
  bool fiber::resume()
  {
    assert( !done() );
    context& cur = context::current();
    if( !cur._fiber ) cur._fiber = new fiber();
    my->_caller = &fiber::current();
    cur._fiber = this;
    auto r = bc::jump_fcontext( my->_caller->my->_context, my->_context, (intptr_t)&cur );
    context* c = reinterpret_cast<context*>(r);
    // I may have woken up in a new thread...
    assert( c == &context::current() );

    if( my->_exit_except ) std::rethrow_exception( my->_exit_except );
    return my->_done;
  }

  /**
   *  Returns the fiber that last called start(), resume() or yield_to()
   */
  fiber* fiber::caller()const
  {
     return my->_caller;
  }

  /**
   *  Jumps to another fiber.  This can only be called 
   *  from the current fiber.
   */
  bool fiber::yield_to( fiber& f  )
  {
    assert( &f != this );
    context& cur = context::current();
    assert( cur._fiber == this );

    // don't set caller for yield to... because someone who called
    // this fiber via 'start' or 'resume' depend upon having control
    // returned to them when the fiber exits.
    // my->caller = cur._fiber; 
    auto from = this;
    cur._fiber = &f; // we are jumping to this... 
    auto r = bc::jump_fcontext( from->my->_context, f.my->_context, (intptr_t)&cur );
    context* c = reinterpret_cast<context*>(r);
    assert( c == &context::current() );

    if( my->_exit_except ) std::rethrow_exception( my->_exit_except );
    return my->_done;
  }

  /** 
   * Shorthand for yield_to( *caller() ) 
   */
  void fiber::yield()
  {
     yield_to( *my->_caller );
  }

  /**
   *   Accesses the 'thread local'
   */
  fiber& fiber::current()
  {
    auto cc = context::current();
    if( !cc._fiber ) cc._fiber = new fiber();
    return *cc._fiber;
  }

  size_t fiber::default_stack_size()
  {
     return bco::stack_allocator::default_stacksize();
  }

} // namespace fc
