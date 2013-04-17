#include "disruptor.hpp"
#include <assert.h>
#include <unistd.h>
#include <iostream>

namespace disruptor {

void barrier::follows( std::shared_ptr<const event_cursor> e )
{
    _limit_seq.push_back( std::move(e) );
}

int64_t barrier::wait_for( int64_t pos )const
{
   if( _last_min > pos ) 
      return _last_min;

   int64_t min_pos = 0x7fffffffffffffff;
   for( auto itr = _limit_seq.begin(); itr != _limit_seq.end(); ++itr )
   {
      int64_t itr_pos = 0;
      itr_pos = (*itr)->pos().aquire();
      // spin for a bit 
      for( int i = 0; itr_pos < pos && i < 1000; ++i  )
      {
         itr_pos = (*itr)->pos().aquire();
         if( (*itr)->pos().alert() )
         {
            if( itr_pos > pos ) 
               return itr_pos -1;// process everything up to itr_pos
            (*itr)->check_alert();
            throw eof();
         }
      }
      // yield for a while, queue slowing down
      for( int y = 0; itr_pos < pos && y < 1000; ++y )
      {
         usleep(0);
         itr_pos = (*itr)->pos().aquire();
         if( (*itr)->pos().alert() )
         {
            if( itr_pos > pos ) 
               return itr_pos -1;// process everything up to itr_pos
            (*itr)->check_alert();
            throw eof();
         }
      }

      // queue stalled, don't peg the CPU but don't wait
      // too long either...
      while( itr_pos < pos )
      {
         usleep( 10*1000 );
         itr_pos = (*itr)->pos().aquire();
         if( (*itr)->pos().alert() )
         {
            if( itr_pos > pos ) 
               return itr_pos -1; // process everything up to itr_pos
            (*itr)->check_alert();
            throw eof();
         }
      }
      if( itr_pos < min_pos ) 
          min_pos = itr_pos; 
   }
   assert( min_pos != 0x7fffffffffffffff );
   return _last_min = min_pos;
}

void event_cursor::check_alert()const
{
    if( _alert != std::exception_ptr() ) std::rethrow_exception( _alert );
}

}
