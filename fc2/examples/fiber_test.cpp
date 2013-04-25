#include <iostream>
#include <fc/thread/fiber.hpp>
#include <stdexcept>
#include <memory>


void hello()
{
   int count = 0;
   while( count < 10 )
   {
     std::cerr<<"hello: " <<count<<"   "<<fc::context::current()._fiber<<"\n";
     fc::fiber::current().yield();
     if( count == 5 )
       throw std::runtime_error( "test error" );
     ++count;
   }
   std::cerr<<"done...\n";
}

int main( int argc, char** argv )
{
  try {
     std::unique_ptr<fc::fiber> f( new fc::fiber( [=](){ hello(); } ));
     std::cerr<<"main\n";
     f->start();
     std::cerr<<"main\n";
     int c = 0;
     while( !f->done()  )
     {
        std::cerr<<"main: " <<c<<"   "<<fc::context::current()._fiber<<"\n";
        f->resume();
        ++c;
      }
   } catch ( std::exception& e )
   {
    std::cerr<<e.what()<<"\n";
   }
  return 0;
}
