/*

Copyright 2016-2020 Cazamar Systems

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "thread.h"
#include "threadmutex.h"
#include "threadtimer.h"

class TimerTest;

long main_maxCount;

void delayRand(long maxSpins) {
    long i;
    long spins;
    static long total = 0;

    spins = random() % maxSpins;
    for(i=0;i<spins;i++) 
        total++;
};

class CancelTest : public Thread {
    static ThreadMutex _mutex;
    ThreadTimer *_timerp;
    static const uint32_t sleepMs = 100;

public:
    static void timerFired(ThreadTimer *timerp, void *contextp) {
        CancelTest *p = (CancelTest *) contextp;
        CancelTest::_mutex.take();
        if (timerp->isCanceled()) {
            CancelTest::_mutex.release();
            return;
        }

        printf("Timer fired\n");
        p->_timerp = NULL; /* because it fired */
        p->_timerp = new ThreadTimer(sleepMs, &CancelTest::timerFired, p);
        p->_timerp->start();

        _mutex.release();
    }

    void *start() {
        uint32_t counter = 0;

        printf("Cancel test thread=%p\n", this);
        _timerp = new ThreadTimer(sleepMs, &CancelTest::timerFired, this);
        _timerp->start();
        while(1) {
            ThreadTimer::sleep(499);
            printf("Stopping timer\n");
            _mutex.take();
            _timerp->cancel();
            _timerp = NULL;
            _mutex.release();
            ThreadTimer::sleep(100);

            printf("Restarting timer\n");
            _mutex.take();
            _timerp = new ThreadTimer(50, &CancelTest::timerFired, this);
            _timerp->start();
            _mutex.release();

            if(++counter > main_maxCount) {
                _timerp->cancel();
                _timerp = NULL;
                printf("Cancel test done\n");
                return NULL;
            }
            else {
                printf("Cancel test iteration %d\n", counter);
            }   
        }
    }
};

ThreadMutex CancelTest::_mutex;

class SleepTest : public Thread {
public:
    void *start() {
        uint32_t counter = 0;
        while(1) {
            printf("Sleep test ran\n");
            ThreadTimer::sleep(150);
            if (++counter > main_maxCount)
                break;
            else {
                printf("Sleep test iteration %d\n", counter);
            }   
        }
        printf("Sleep test done\n");
        return NULL;
    }
};

class TimerTest : public Thread {
public:
    ThreadTimer *_timerp;
    static ThreadMutex _lock;
    ThreadCond _cv;

    static void timerFired(ThreadTimer *timerp, void *contextp) {
        TimerTest *testp;
        _lock.take();
        if (timerp->isCanceled()) {
            _lock.release();
            return;
        }
        testp = (TimerTest *) contextp;
        testp->_cv.broadcast();
        _lock.release();
    }

    void *start() {
        uint32_t counter=0;
        _lock.take();
        while(1) {
            _timerp = new ThreadTimer(100, timerFired, this);
            _timerp->start();
            _cv.wait(&_lock);
            printf("Timer fired and woke us up\n");
            if (++counter > main_maxCount) {
                printf("Timer test done\n");
                break;
            }
            else {
                printf("Timer test iteration %d\n", counter);
            }   
        }
        _lock.release();
    }
};

/* statics */
ThreadMutex TimerTest::_lock;

int
main(int argc, char **argv)
{
    long long us;
    int code;
    TimerTest *testp;
    SleepTest *sleepTestp;
    CancelTest *cancelTestp;
    void *junk;
    
    if (argc<2) {
        printf("usage: timertest <count>\n");
        return -1;
    }
    
    main_maxCount = atoi(argv[1]);

    /* start the dispatcher */
    ThreadDispatcher::setup(/* # of pthreads */ 2);

    ThreadTimer::init();

    testp = new TimerTest();
    testp->setJoinable();
    testp->queue();

    sleepTestp = new SleepTest();
    sleepTestp->setJoinable();
    sleepTestp->queue();

    testp->join(&junk);
    sleepTestp->join(&junk);

    cancelTestp = new CancelTest();
    cancelTestp->setJoinable();
    cancelTestp->queue();

    cancelTestp->join(&junk);

    _exit(0);

    while(1) {
        sleep(100);
    }
    return 0;
}

