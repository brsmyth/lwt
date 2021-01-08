all: libthread.a ttest mtest eptest timertest pipetest

DESTDIR=../export

INCLS=thread.h threadmutex.h threadpipe.h osp.h dqueue.h epoll.h threadtimer.h spinlock.h ospnew.h

CFLAGS=-g -Wall

install: all
	-mkdir $(DESTDIR)/include $(DESTDIR)/lib $(DESTDIR)/bin
	cp -up $(INCLS) $(DESTDIR)/include
	cp -up libthread.a $(DESTDIR)/lib

clean:
	-rm -f ttest mtest eptest timertest pipetest *.o *.a *temp.s
	(cd alternatives; make clean)

getcontext.o: getcontext.s
	cpp getcontext.s >getcontext-temp.s
	as -o getcontext.o getcontext-temp.s
	-rm getcontext-temp.s

setcontext.o: setcontext.s
	cpp setcontext.s >setcontext-temp.s
	as -o setcontext.o setcontext-temp.s
	-rm setcontext-temp.s

osp.o: osp.cc $(INCLS)
	g++ -c $(CFLAGS) osp.cc -pthread

ospnew.o: ospnew.cc $(INCLS)
	g++ -c $(CFLAGS) ospnew.cc -pthread

threadtimer.o: threadtimer.cc $(INCLS)
	g++ -c $(CFLAGS) threadtimer.cc -pthread

threadmutex.o: threadmutex.cc $(INCLS)
	g++ -c $(CFLAGS) threadmutex.cc -pthread

threadpipe.o: threadpipe.cc $(INCLS)
	g++ -c $(CFLAGS) threadpipe.cc -pthread

libthread.a: epoll.o thread.o getcontext.o setcontext.o threadmutex.o threadpipe.o osp.o ospnew.o threadtimer.o
	ar cr libthread.a epoll.o thread.o getcontext.o setcontext.o threadmutex.o threadpipe.o osp.o ospnew.o threadtimer.o
	ranlib libthread.a

thread.o: thread.cc $(INCLS)
	g++ -c $(CFLAGS) -o thread.o thread.cc -pthread

epoll.o: epoll.cc $(INCLS)
	g++ -c $(CFLAGS) -o epoll.o epoll.cc -pthread

ttest.o: ttest.cc $(INCLS)
	g++ -c $(CFLAGS) -o ttest.o ttest.cc -pthread

ttest: ttest.o libthread.a
	g++ -g -o ttest ttest.o libthread.a -pthread

timertest.o: timertest.cc $(INCLS)
	g++ -c $(CFLAGS) -o timertest.o timertest.cc -pthread

pipetest.o: pipetest.cc $(INCLS)
	g++ -c $(CFLAGS) -o pipetest.o pipetest.cc -pthread

mtest.o: mtest.cc $(INCLS)
	g++ -c $(CFLAGS) -o mtest.o mtest.cc -pthread

eptest.o: eptest.cc $(INCLS)
	g++ -c $(CFLAGS) -o eptest.o eptest.cc -pthread

mtest: mtest.o libthread.a
	g++ -g -o mtest mtest.o libthread.a -pthread

eptest: eptest.o libthread.a
	g++ -g -o eptest eptest.o libthread.a -pthread

timertest: timertest.o libthread.a
	g++ -g -o timertest timertest.o libthread.a -pthread

pipetest: pipetest.o libthread.a
	g++ -g -o pipetest pipetest.o libthread.a -pthread
