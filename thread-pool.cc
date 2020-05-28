/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
#include "semaphore.h"
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>

using namespace std;
using develop::ThreadPool;

ThreadPool::ThreadPool(size_t numThreads) : wts(numThreads), executePermit(0) , dispatcherPermit(0), workerAvailable(numThreads), numToExecute(0), kNumThreads(numThreads), allDone(false), numAvailable(numThreads) {

  for (size_t workerID = 0; workerID < numThreads; workerID++) {
    wts[workerID].myThread = thread([this](size_t workerID) {
	worker(workerID);
      }, workerID);
  }
}

void ThreadPool::schedule(const std::function<void(void)>& thunk) {
  queueLock.lock();
  queue.push(thunk);
  queueLock.unlock();
  executeLock.lock();
  numToExecute++;
  executeLock.unlock();
  dispatcherPermit.signal();
}

//void ThreadPool::dispatcher() {
// while(true) {
//   dispatcherPermit.wait();
//   workerAvailable.wait();
//   queueLock.lock();
//   if (queue.empty()) {
//    queueLock.unlock();
//    break;
//   }
//   std::function<void(void)> nextFunction = queue.front();
//   queue.pop();
//   queue2.push(nextFunction);
//   queueLock.unlock();
//   executePermit.signal();
//  }
//}

void ThreadPool::worker(std::size_t workerID) {
  while(true) {
    dispatcherPermit.wait();
    wts[workerID].executePermit.wait();
    if (allDone) {
      break;
    }
    executeLock.lock();
    numAvailable--;
    executeLock.unlock();
    queueLock.lock();
    std::function<void(void)> func = queue.front();
    queue.pop();
    queueLock.unlock();
    func();
    wts[workerID].executePermit.signal();
    executeLock.lock();
    numToExecute--;
    numAvailable++;
    executeLock.unlock();
    cv.notify_all();
  }
}

void ThreadPool::wait() {
  unique_lock<mutex> ul(executeLock);
  cv.wait(ul, [this]{return numToExecute == 0 && numAvailable == kNumThreads; });
}

ThreadPool::~ThreadPool() {
  wait();
  allDone = true;
  for (myStruct& t : wts) {
    dispatcherPermit.signal();
    t.executePermit.signal();
  }
  for (myStruct& t : wts) {
    t.myThread.join();
  }
}
