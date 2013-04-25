
  #if 0 // this is 
  template<typename Signature>
  class coroutine 
  {
      public:
        coroutine( std::function<Signature> fun, size_t stack_size = fiber::default_stack_size() );

        typedef (Signature.Result.toRvalue) result_type

        /**
         *  return an rvalue 
         */
        template<typename... Args>
        result_type start( Args&& args... )
        {
           // bind a lambda that uses a fiber to execute with the given args... 
           // then start the fiber
           return std::move(_last_result);
        }

        /** 
         *  Return a rvalue reference to the last result...
         */
        result_type resume() 
        {
          return std::move(_last_result);
        }
      
        template<typename T>
        void yield( T&& v )
        {
           _last_result = std::forward<T>(v);
           _fiber->yield();
        }
      private:
        result_of(_func)         _last_result;
        std::function<Signature> _func;
  };
  #endif 
