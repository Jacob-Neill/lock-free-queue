# lock-free-queue
Michael &amp; Scott queue implementation

This Michael &amp; Scott queue currently uses an atomic counter to keep track of the number of threads in the pop method. 
When a single thread is in the pop method, it claims the bag storing popped nodes and deletes the nodes to reclaim memory.
