#include <disruptor/thread.hpp>


auto thread_a = new disruptor::thread(); 
auto thread_b = new disruptor::thread(); 

void count( int64_t c, disruptor::thread* source, disruptor::thread* reply )
{
    if( c > (1LL << 24) ) exit(1);
    reply->post( [=](){  count( c+1, reply, source ); } );
}


int main( int argc, char** argv )
{
    thread_a->start();
    thread_b->start();
    usleep( 1000*1000*30 );
    thread_a->post( [=]() { count( 1, thread_a, thread_b ); } );
    thread_a->join();
    thread_b->join();
    return 0;
}
