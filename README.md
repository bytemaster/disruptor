Disruptor
=========

My take on the LMAX Disruptor, with a different API.  Existing
C++ implementations of the LMAX disruptor relied on virtual
interfaces and dispatch methods that copied from Java.  This
implementation attempts to do things in a more C++ way and
provide what I hope to be a simpler interface.

Design
=========

There are 3 primary types that users of the API must be familiar
with:

   * *ring_buffer<T,Size>*  is a circular buffer with Power of 2 Size
   * *write_cursor*         tracks a position in the buffer and can follow
                            other read cursors to ensure things don't wrap.
   * *read_cursor*          tracks the read position and can follow / block
                            on other cursors (read or write).

The concept of the cursors are separated from the data storage.  Every cursor
should read from one or more sources and write to its own outbut buffer.  

Features
==========
  * Progressive-backoff blocking.  When one cursor needs to wait on another it starts
out with a busy wait, followed by a yield, and ultimately falls back to sleeping
wait if the queue is stalled.  

  * Batch writing / Reading with 'iterator like' interface.  Producers and consumers
  always work with a 'range' of valid positions.   The ring buffer provides the
  ability to detect 'wraping' and therefore it should be possible to use this as
  a data queue for a socket that can read/write many slots all at once.  Slots
  could be single bytes and the result would be a very effecient stream-processing
  library.  This manner of operation is not possible with LMAX's API. 

Performance
===========
Simple benchmarks indicate that performance of this implementation is better than
other known C++ implementations of this pattern.  


Todo
===========
Multi-Producer write cursor / pattern.  Generally I think it is best to structure 
your code to avoid this use case all together.  If multiple producers need to
send data to a single consumer, the consumer should give them each their own queue
and then procses the results 'round-robin'.  Assuming the number of producers is
known in advance this would represent a 'fixed cost' one-time setup and avoid the
need to use atomic operations all together.



