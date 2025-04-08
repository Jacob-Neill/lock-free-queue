# lock-free-queue
Michael &amp; Scott queue implementation

This Michael &amp; Scott queue currently uses an internal raw atomic pointer, an internal lock-free bag. This bag stores nodes popped from the queue and saves them for deletion later. This is a memory-inefficient solution to the ABA problem.
