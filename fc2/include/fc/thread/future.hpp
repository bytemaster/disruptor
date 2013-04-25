#pragma once
#include <exception>
#include <memory>
#include <fc/fwd.hpp>
#include <fc/optional.hpp>

namespace fc {

namespace detail
{
  class promise_impl;
}

class basic_promise
{
  public:
    /**
     *  what must be statically allocated because
     *  it will not be freed.
     */
    basic_promise( const char* _what = "" );
    ~basic_promise();

    void set_exception( std::exception_ptr e );
    const char* what()const;

  protected:
    void _notify();
    void _wait();

  private:
    fwd<detail::promise_impl,64> my;
};

template<typename T>
class promise : public basic_promise
{
  public:
    typedef std::shared_ptr< promise<T> > ptr;
    promise( const char* _what = "" )
    :basic_promise(_what){}
  
    T& wait()
    { 
      _wait();
      return std::move(*_result);
    }

    template<typename V>
    void set_value(  V&& v )
    {
      _result = std::forward<V>(v);
      _notify();
    }


  private:
    optional<T> _result;
};

template<>
class promise<void> : public basic_promise
{
  public:
    typedef std::shared_ptr< promise<void> > ptr;

    promise( const char* _what = "" )
    :basic_promise(_what){}

    void wait()
    { 
      _wait();
    }

    void set_value()
    {
      _notify();
    }
};


template<typename T>
class future
{
  public:
    future(){}
    ~future(){}

    future( future&& f )
    :_prom( std::move(f._prom) ){}

    future( const future& f )
    :_prom( f._prom ){}

    future& operator=( const future& f )
    { _prom = f._prom; return *this; }

    future& operator=( future&& f )
    { _prom = std::move(f._prom); return *this; }


    future( typename promise<T>::ptr p )
    :_prom( std::move(p) ){}

    const char* what()const { return _prom->what(); }

    T& wait()const
    {
      return _prom->wait();
    }
  private:
    typename promise<T>::ptr _prom;
};

template<>
class future<void>
{
  public:
    future(){}
    ~future(){}

    future( future&& f )
    :_prom( std::move(f._prom) ){}

    future( const future& f )
    :_prom( f._prom ){}

    future& operator=( const future& f )
    { _prom = f._prom; return *this; }

    future& operator=( future&& f )
    { _prom = std::move(f._prom); return *this; }


    future( promise<void>::ptr p )
    :_prom( std::move(p) ){}

    const char* what()const { return _prom->what(); }

    void wait()const
    {
       _prom->wait();
    }
  private:
    typename promise<void>::ptr _prom;
};

} // namespace fc
