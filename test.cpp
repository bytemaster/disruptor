#include "disruptor.hpp"
#include <thread>
#include <stdexcept>
#include <iostream>
#include <sys/time.h>

using namespace disruptor;

struct event
{
  uint64_t source;
  uint64_t square;
  uint64_t cube;
  uint64_t diff;
};

class cursor_error : public std::exception
{
    public:
        cursor_error( const char* e )
        :_e(e){}

       virtual const char* what()const noexcept { return _e; }
    private:
       const char* _e;
};

int main( int argc, char** argv )
{
   #define SIZE 1024
   uint64_t iterations = 1000L * 1000L * 300;
   // data source / publisher
   auto source     = std::make_shared<ring_buffer<int64_t,SIZE>>();
   auto square     = std::make_shared<ring_buffer<int64_t,SIZE>>();
   auto cube       = std::make_shared<ring_buffer<int64_t,SIZE>>();
   auto diff       = std::make_shared<ring_buffer<int64_t,SIZE>>();

   // verify cache conflict...
   // auto combined   = std::make_shared<ring_buffer<event,1024*1024>>();

   auto a = std::make_shared<read_cursor>("a");
   auto b = std::make_shared<read_cursor>("b");
   auto c = std::make_shared<read_cursor>("c");
   auto p = std::make_shared<write_cursor>("write",SIZE);

   a->follows(p);
 //  p->follows(a);
   b->follows(p);
   c->follows(a);
   c->follows(b);
   p->follows(c);

   // thread publisher
   auto pub_thread = [=](){
      try
      {
        auto pos = p->begin();
        auto end = p->end();
        for( uint64_t i = 0; i < iterations; ++i ) //1000*1000ll*1000ll; ++i )
        {
           if( pos >= end )
           {  
              end = p->wait_for(end);
           }
           source->at( pos ) = i;
           // note, publishing in 'batches' is 2x as fast, hitting this
           // memory barrior really slows things down in this trival example.
           p->publish(pos);
           ++pos;
        }
        p->set_eof();
      } 
      catch ( std::exception& e )
      {
        std::cerr<<"publisher caught: "<<e.what()<<" at pos "<<p->pos().aquire()<<"\n";
      }
   };

   // thread a
   auto thread_a = [=](){
      try 
      {
         auto pos = a->begin();
         auto end = a->end();
         while( pos < int64_t(iterations) ) 
         {
            if( pos == end )
            {
                a->publish(pos-1);
                end = a->wait_for(end);
            }
            source->at(pos) = pos; // set published data here...
            ++pos;
         }
      } 
      catch ( std::exception& e )
      {
        std::cerr<<"a caught: "<<e.what()<<"\n";
      }
   };

   // thread b
   auto thread_b = [=](){
      try {
         auto pos = b->begin();
         auto end = b->end();
         while( true )
         {
            if( pos == end )
            {
                b->publish(pos-1);
                end = b->wait_for(end);
            }
            if( pos % 10000000ll == 9934000 )
            {
                std::cerr<<"THROWING! "<<pos<<"\n";
                c->set_alert(std::make_exception_ptr(cursor_error("threadb")));
            }
            else
            {
              cube->at(pos) = source->at(pos) * source->at(pos) * source->at(pos);
            }

            ++pos;
         }
      } catch ( std::exception& e )
      {
        std::cerr<<"b caught: "<<e.what()<<"\n";
      }
   };
   
   // thread c
   auto thread_c = [=](){
      try {
           auto pos = c->begin();
           auto end = c->end();
           while( true )
           {
              if( pos == end )
              {
                  c->publish(pos-1);
                  end = c->wait_for(end);
              }
              diff->at(pos) = cube->at(pos) - square->at(pos);
              ++pos;
           }
      } 
      catch ( std::exception& e )
      {
        std::cerr<<"c caught: "<<e.what()<<"\n";
      }
   };


   std::thread pt( pub_thread );
   std::thread at( thread_a );
   std::thread bt( thread_b );
   std::thread ct( thread_c );
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

     pt.join();
     at.join();
     bt.join();
     ct.join();

    gettimeofday(&end_time, NULL);

    double start, end;
    start = start_time.tv_sec + ((double) start_time.tv_usec / 1000000);
    end = end_time.tv_sec + ((double) end_time.tv_usec / 1000000);

    std::cout.precision(15);
    std::cout << "1P-1EP-UNICAST performance: ";
    std::cout << (iterations * 1.0) / (end - start)
              << " ops/secs" << std::endl;
    std::cout << "Source: "<< source->at(0) <<"  2x: "<<square->at(0)<<"\n";
    std::cout << "Source: "<< source->at(1) <<"  2x: "<<square->at(1)<<"\n";
    std::cout << "Source: "<< source->at(2) <<"  2x: "<<square->at(2)<<"\n";
   /*
   pt.join();
   at.join();
   */

   std::cerr<<"Exiting\n";

   return 0;
}
