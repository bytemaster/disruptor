


template<size_t Size, size_t Num=1024*256>
class hash_malloc
{
  public:
    struct segment
    {
      segment():_reserved(0){}
      uint32_t                _data[Size];
      boost::atomic<uint32_t> _reserved;
    };

    hash_malloc()
    {
      _segments = (segment*)malloc( sizeof(segment)*Num);
      memset( _segments, 0, sizeof(segment)*Num);
    }

    char* alloc()
    {
       // get hash
      int32_t hash;
      int32_t index;
       do
       {
           hash = 0; // get random... hash... 
           index = hash & (Num-1);
           while( segments[index]._reserved != 0 )
           {
              index = (index+1) & (Num-1);
           }

       } while( segments[ndex]._reserved.fetch_add(1) ) ;

       return _segments[index]._data;
    }

    bool  free( char* s )
    {
    }

    segment* _segments; 
};

hash_malloc& hash_malloc_60()
{
  static hash_malloc<60> hm;
  return hm;
}
