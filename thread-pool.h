/**
 * File: thread-pool.h
 * -------------------
 * Exports a ThreadPool abstraction, which manages a finite pool
 * of worker threads that collaboratively work through a sequence of tasks.
 * As each task is scheduled, the ThreadPool waits for at least
 * one worker thread to be free and then assigns that task to that worker.  
 * Threads are scheduled and served in a FIFO manner, and tasks need to
 * take the form of thunks, which are zero-argument thread routines.
 */

#ifndef _thread_pool_
#define _thread_pool_

#include <cstdlib>
#include <functional>
// place additional #include statements here
#include <thread>      // for thread
#include <vector>      // for vector
#include "semaphore.h"
#include <mutex>
#include <queue>
#include <condition_variable>



using namespace std;
namespace develop {

class ThreadPool {
 public:

/**
 * Constructs a ThreadPool configured to spawn up to the specified
 * number of threads.
 */
  ThreadPool(size_t numThreads);

/**
 * Destroys the ThreadPool class
 */
  ~ThreadPool();

/**
 * Schedules the provided thunk (which is something that can
 * be invoked as a zero-argument function without a return value)
 * to be executed by one of the ThreadPool's threads as soon as
 * all previously scheduled thunks have been handled.
 */
  void schedule(const std::function<void(void)>& thunk);

/**
 * Blocks and waits until all previously scheduled thunks
 * have been executed in full.
 */
  void wait();

 private:

  struct myStruct {
  myStruct(): executePermit(1) {}
    semaphore executePermit;
    std::thread myThread;
  };

  //  std::thread dt;                // dispatcher thread handle
  std::vector<struct myStruct> wts;  // worker thread handles
  semaphore executePermit;
  semaphore dispatcherPermit;
  semaphore workerAvailable;
  std::size_t numToExecute;
  std::size_t kNumThreads;
  std::size_t allDone;
  std::size_t numAvailable;
  semaphore execute;

  std::mutex executeLock;
  std::queue<std::function <void(void)>> queue;

  std::condition_variable_any cv;
  std::mutex queueLock;
  std::mutex workerLock;

  /**
   * dispatcher()
   * -----------------
   * private helper method
   */
  void dispatcher();


  /**
   * worker(size_t workerID)
   * ------------------------
   *  Private helper method
   */
  void worker(std::size_t workerID);
  
  ThreadPool(const ThreadPool& original) = delete;
  ThreadPool& operator=(const ThreadPool& rhs) = delete;
};

#endif

}
