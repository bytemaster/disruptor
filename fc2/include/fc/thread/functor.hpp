#pragma once
#include <fc/utility.hpp>
#include <assert.h>

namespace fc
{
   namespace detail
   {
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
            static void move_construct( void* self, void* other )
            {
                new ((char*)other) Functor( fc::move( *((Functor*)self) ) );
            }
        };
   }

   /**
    *  A fixed sized functor with signature void(). The
    *  size is fixed so that it can be used with a fixed
    *  size ringbuffer without dynamic memory allocation.
    */
   template<size_t Size=128>
   class functor
   {
      public:
        functor():run(nullptr),destruct(nullptr){}
        
        template<typename Functor>
        functor( Functor&& f )
        {
           static_assert( sizeof(_buffer) >= sizeof(Functor), "insufficient space for functor" );
           new (_buffer) Functor( std::forward<Functor>(f) );
           run            = &detail::functor_invoker<Functor>::run;
           destruct       = &detail::functor_invoker<Functor>::destruct;
           move_construct = &detail::functor_invoker<Functor>::move_construct;
        }
        
        ~functor()
        {
           if( destruct ) destruct(_buffer);
        }

        functor( functor&& f )
        :run(f.run),destruct(f.destruct),move_construct(f.move_construct)
        {
           f.destruct = nullptr;
           if( destruct != nullptr )
           {
              move_construct( f._buffer, _buffer );
           }
        }
        
        template<typename Functor>
        functor& operator=( Functor&& f )
        {
           static_assert( sizeof(_buffer) >= sizeof(Functor), "insufficient space for functor" );
           if( this != &f )
           {
               if( destruct ) destruct(_buffer);
               new (_buffer) Functor( std::forward<Functor>(f) );
               run = &detail::functor_invoker<Functor>::run;
               destruct = &detail::functor_invoker<Functor>::destruct;
               move_construct = &detail::functor_invoker<Functor>::move_construct;
           }
           return *this;
        }
        functor& operator=( const functor& f )
        {
          assert(!"no copy support..");
          return *this;
        }
        functor& operator=( functor&& f )
        {
          if( destruct ) destruct(_buffer);
          if( f.move_construct )
          {
            f.move_construct(f._buffer, _buffer);
            move_construct = f.move_construct;
            destruct       = f.destruct;
            run            = f.run;
            f.destruct     = nullptr;
          }
          else
          {
            destruct = nullptr;
          }
          return *this;
        }

        void call(){ run( _buffer ); }

      private:
        void (*run)( void* self ); 
        void (*destruct)( void* self );
        void (*move_construct)( void* self, void* other );
        char _buffer[Size];
   };

} // namespace fc
