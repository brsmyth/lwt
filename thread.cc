#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include <assert.h>

#include "thread.h"

extern "C" {
extern int xgetcontext(ucontext_t *ctxp);
extern int xsetcontext(ucontext_t *ctxp);
};

pthread_key_t ThreadDispatcher::_dispatcherKey;
pthread_once_t ThreadDispatcher::_once;
ThreadDispatcher *ThreadDispatcher::_allDispatchers[ThreadDispatcher::_maxDispatchers];
uint16_t ThreadDispatcher::_dispatcherCount;

SpinLock Thread::_globalThreadLock;
dqueue<ThreadEntry> Thread::_allThreads;

/* internal function doing some of the initialization of a thread */
void
Thread::init()
{
    static const long memSize = 1024*1024;
    xgetcontext(&_ctx);
    _ctx.uc_link = NULL;
    _ctx.uc_stack.ss_sp = malloc(memSize);
    _ctx.uc_stack.ss_size = memSize;
    _ctx.uc_stack.ss_flags = 0;
#if THREAD_PTR_FITS_IN_INT
    makecontext(&_ctx, (void (*)()) &ctxStart, 
                2,
                (int) ((long) this),
                0);
#else
    makecontext(&_ctx, (void (*)()) &ctxStart, 
                2,
                (int) ((long) this) & 0xFFFFFFFF,
                (int)((long)this)>>32);
#endif
}

/* called to start a light weight thread */
/* static */ void
Thread::ctxStart(unsigned int p1, unsigned int p2)
{
    /* reconstruct thread pointer */
#if THREAD_PTR_FITS_IN_INT
    unsigned long threadInt = (unsigned long) p1;
#else
    unsigned long threadInt = (((unsigned long) p2)<<32) + (unsigned long)p1;
#endif
    Thread *threadp = (Thread *)threadInt;

    threadp->start();
    printf("Thread %p returned from start\n", threadp);
}

/* called to resume a thread, or start it if it has never been run before */
void
Thread::resume()
{
    xsetcontext(&_ctx);
}

/* find a suitable dispatcher and queue the thread for it */
void
Thread::queue()
{
    unsigned int ix;
    
    ix = (unsigned int) this;
    ix = (ix % 127) % ThreadDispatcher::_dispatcherCount;
    ThreadDispatcher::_allDispatchers[ix]->queueThread(this);
}

void
Thread::sleep(SpinLock *lockp)
{
    _currentDispatcherp->sleep(this, lockp);
}

/* idle thread whose context can be resumed */
void
ThreadIdle::start()
{
    SpinLock *lockp;
    
    while(1) {
        xgetcontext(&_ctx);
        lockp = getLockAndClear();
        if (lockp)
            lockp->release();
        _disp->dispatch();
    }
}

/* should we switch to an idle thread before sleeping?  Probably, since
 * otherwise we might be asleep when another CPU resumes the thread
 * above us on the stack.  Actually, this can happen even if we don't
 * go to sleep.  We really have to be off this stack before we call
 * drop the 
 */
void
ThreadDispatcher::dispatch()
{
    Thread *newThreadp;
    while(1) {
        _runQueueLock.take();
        newThreadp = _runQueue.pop();
        if (!newThreadp) {
            _sleeping = 1;
            _runQueueLock.release();
            pthread_mutex_lock(&_runMutex);
            while(_sleeping) {
                pthread_cond_wait(&_runCV, &_runMutex);
            }
            pthread_mutex_unlock(&_runMutex);
        }
        else{
            _runQueueLock.release();
            _currentThreadp = newThreadp;
            newThreadp->_currentDispatcherp = this;
            newThreadp->resume();   /* doesn't return */
        }
    }
}

/* When a thread needs to block for some condition, the paradigm is that
 * it will have some SpinLock held holding invariant some condition,
 * such as the state of a mutex.  As soon as that spin lock is
 * released, another processor might see this thread in a lock queue,
 * and queue it to a dispatcher.  We need to ensure that the thread's
 * state is properly stored before allowing a wakeup operation
 * (queueThread) to begin.
 *
 * In the context of this function, this means we must finish the
 * getcontext call and the clearing of _goingToSleep before allowing
 * the thread to get queued again, so that when the thread is resumed
 * after the getcontext call, it will simply return to the caller.
 *
 * Note that this means that the unlock will get performed by the
 * same dispatcher as obtained the spin lock, but when sleep returns,
 * it may be running on a different dispatcher.
 */
void
ThreadDispatcher::sleep(Thread *threadp, SpinLock *lockp)
{
    assert(threadp == _currentThreadp);
    _currentThreadp = NULL;
    threadp->_goingToSleep = 1;
    xgetcontext(&threadp->_ctx);
    if (threadp->_goingToSleep) {
        threadp->_goingToSleep = 0;

        /* prepare to get off this stack, so if this thread gets resumed
         * after we drop the user's spinlock, we're not using this
         * stack any longer.
         *
         * The idle context will resume at ThreadIdle::start, either at
         * the start or in the while loop, and will then dispatch the
         * next thread from the run queue.
         */
        _idle._userLockToReleasep = lockp;
        xsetcontext(&_idle._ctx);

        printf("!Error: somehow back from sleep's setcontext disp=%p\n", this);
    }
    else {
        /* this thread is being woken up */
        return;
    }
}

void
ThreadDispatcher::queueThread(Thread *threadp)
{
    _runQueueLock.take();
    _runQueue.append(threadp);
    if (_sleeping) {
        _runQueueLock.release();
        pthread_mutex_lock(&_runMutex);
        _sleeping = 0;
        pthread_mutex_unlock(&_runMutex);
        pthread_cond_broadcast(&_runCV);
    }
    else {
        _runQueueLock.release();
    }
}

/* static */ void *
ThreadDispatcher::dispatcherTop(void *ctx)
{
    ThreadDispatcher *disp = (ThreadDispatcher *)ctx;
    pthread_setspecific(_dispatcherKey, disp);
    disp->_idle.resume(); /* idle thread switches to new stack and then calls the dispatcher */
    printf("Error: dispatcher %p top level return!!\n", disp);
}

/* the setup function creates a set of dispatchers */
/* static */ void
ThreadDispatcher::setup(uint16_t ndispatchers)
{
    pthread_t junk;
    uint32_t i;

    for(i=0;i<ndispatchers;i++) {
        new ThreadDispatcher();
    }

    /* call each dispatcher's dispatch function on a separate pthread;
     * note that the ThreadDispatcher constructor filled in _allDispatchers
     * array.
     */
    for(i=0;i<ndispatchers;i++) {
        pthread_create(&junk, NULL, dispatcherTop, _allDispatchers[i]);
    }
}

ThreadDispatcher::ThreadDispatcher() {
    Thread::_globalThreadLock.take();
    _allDispatchers[_dispatcherCount++] = this;
    Thread::_globalThreadLock.release();

    _sleeping = 0;
    _currentThreadp = NULL;
    _idle._disp = this;
    pthread_mutex_init(&_runMutex, NULL);
    pthread_cond_init(&_runCV, NULL);
    pthread_once(&_once, &ThreadDispatcher::globalInit);
}
