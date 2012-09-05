//-------------------------------------------------------------------------
// Test newly introduced locks with multiple-threads; The program
// below "may" core, if built without locks, and must terminate
// without error if WITH_LOCK is defined. To test all the various
// underlying implementations, such as select, poll, epoll, etc. 
// libevent has to be compiled with config.h modified. The test below
// tries to maximize contention between addition and deletion of timers
// while they are being serviced in the dispatch loop, and is
// shown to core on vam-build-001 in all attempts thus far. There is
// heavy contention for the lock, because of the sheer number of
// timers and their minimum popup period, which will result in variable
// running time.
//
// The timer-deletion thread ensures that all timers get deleted, and
// the event dispatch loop exits successfully.
//
// Built with 
// gcc -I.. -I../compat -D__WITH_LOCK -g -O2 -Wall -o test-multithread 
//     test-multithread.o ../.libs/libevent.a -lpthread 
//
// For descriptors, NEVENTS has to be set small enough, and the RLIMIT
// expression removed from poll, select.c. otherwise we can't realistically
// test the reallocation of the array. The only way to do this as of now
// is to change the code, and then test - ugly but other ways to do this
// will require unit-test infrastructure port into libevent files.
//-------------------------------------------------------------------------

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "sys/queue.h"
#include <assert.h>
#include <event.h>

#define PERIODIC_TM_DEF_TICK    10
#define MAX_TIMERS              300000
#define MAX_ADD_DEL_ITER        1000000
#define MAX_ADD_DEL_TIMERS      1
#define MAX_FDS                 256
#define NEVENT                  16 
#define SOMEPRIME_MOD500        27
#define MAX_FD_ITER             2
#define MAX_FD_ADD_DEL_ITER     1000000
#define FDS_CLOSED_ARRAY_SIZE   16384

static int32_t num_timers, done_add_del, num_streams, random_rw_done;
static int32_t fds_closed_array[FDS_CLOSED_ARRAY_SIZE];

struct periodic_tm
{
    LIST_ENTRY(periodic_tm)     lh;
    struct event                ev;
    struct timeval              tick_tm;
    struct timeval              tick_last;
    int32_t                     tick_cnt;
    int32_t                     tick_max_jitter;
    int32_t                     tm_id;
};

struct stream_rw
{
    LIST_ENTRY(stream_rw)       lh;
    struct event                ev;
    int32_t                     pair[2];
    int32_t                     rw;
    int32_t                     cnt;
    int32_t                     id;
    int32_t                     refcnt;
};

const static struct timespec wakeup = {0, 1000000};

LIST_HEAD(, periodic_tm) ll_periodic_tm;
LIST_HEAD(, stream_rw)   ll_stream_rw;

static pthread_mutex_t llk = PTHREAD_MUTEX_INITIALIZER;

#ifdef __WITH_LOCK
static pthread_mutex_t evm = PTHREAD_MUTEX_INITIALIZER;

static int g_lock_get (void)
{
    return (pthread_mutex_lock(&evm));
}
static int g_lock_put (void)
{
    return (pthread_mutex_unlock(&evm));
}
#endif // __WITH_LOCK

static void periodic_tm_cb (int fd, short evt, void *arg)
{

#ifdef __WITH_LOCK
    assert(pthread_mutex_trylock(&evm));
#endif // __WITH_LOCK

    struct periodic_tm *tm = (struct periodic_tm *)arg;
    tm->tick_cnt++;
    gettimeofday(&tm->tick_last, NULL);
    evtimer_add(&tm->ev, &tm->tick_tm);
}

static void periodic_stream_cb (int fd, short evt, void *arg)
{
    struct stream_rw *s;
    int32_t len;
    char buf[256];

#ifdef __WITH_LOCK
    assert(pthread_mutex_trylock(&evm));
#endif // __WITH_LOCK

    s = (struct stream_rw *)arg;
    len = read(fd, buf, sizeof(buf));
    if (len <= 0) {
        assert(s->pair[0] < FDS_CLOSED_ARRAY_SIZE);
        assert(s->pair[1] < FDS_CLOSED_ARRAY_SIZE);

        assert(fds_closed_array[s->pair[1]]);
        if (0) {
            printf("Desc-could-be-closed! %d:%d:%d:%d:%d\n", 
                   s->id, evt, len, s->pair[1], fds_closed_array[s->pair[1]]);
        }
        fds_closed_array[s->pair[1]] = 0;
    } else if (len != (strlen("test running")+1)) {
        assert(0);
    } else {
        s->cnt++;
    }
}

static struct periodic_tm *create_periodic_tm (void)
{
    struct periodic_tm *tm = calloc(1, sizeof(struct periodic_tm));
    assert(tm);        
    tm->tick_tm.tv_usec = PERIODIC_TM_DEF_TICK;
    tm->tm_id = num_timers + 1;
    num_timers++;
    return (tm);
}

static struct stream_rw *create_stream (int32_t rw)
{
    struct stream_rw *s = calloc(1, sizeof(struct stream_rw));
    assert(s);        
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, s->pair) != -1);
    fcntl(s->pair[1], F_SETFL, O_NONBLOCK);
    s->rw = rw;
    s->id = num_streams++;

    return (s);
}

static void *timer_dispatch_thread (void *arg)
{
    event_dispatch();
    return (NULL);
}

static void *stream_dispatch_thread (void *arg)
{
    event_dispatch();
    return (NULL);
}

static void *timer_add_del_thread (void *arg)
{
    int32_t i = 0, j = 0;
    struct periodic_tm *tm, *tm_next;

    LIST_HEAD(, periodic_tm) ll_tm;
    LIST_INIT(&ll_tm);

    while ( j++ < MAX_ADD_DEL_ITER) {
        if ((j % 10000) == 0)
            fprintf(stderr, "<<< Add-del iteration %d >>>> \n\n", j);

        i = 0;
        while ( i++ < MAX_ADD_DEL_TIMERS ) {
            tm = create_periodic_tm();
            LIST_INSERT_HEAD(&ll_tm, tm, lh);
#ifdef __WITH_LOCK
            g_lock_get();
#endif //__WITH_LOCK
            evtimer_set(&tm->ev, periodic_tm_cb, tm);
            evtimer_add(&tm->ev, &tm->tick_tm);
#ifdef __WITH_LOCK
            g_lock_put();
#endif //__WITH_LOCK
        }
        LIST_FOREACH_SAFE(tm, &ll_tm, lh, tm_next) {
            LIST_REMOVE(tm, lh);
#ifdef __WITH_LOCK
            g_lock_get();
#endif //__WITH_LOCK
            evtimer_del(&tm->ev);
#ifdef __WITH_LOCK
            g_lock_put();
#endif //__WITH_LOCK
            free(tm);
        }
    }

    done_add_del++;
    return (NULL);
}

static void *timer_del_thread (void *arg)
{
    struct periodic_tm *tm, *tm_next;

    while (!done_add_del)
        sleep(5);

    fprintf(stderr, "<<< Starting deletion >>>> \n\n");
#ifdef __WITH_LOCK
        g_lock_get();
#endif //__WITH_LOCK
    LIST_FOREACH_SAFE(tm, &ll_periodic_tm, lh, tm_next) {
        LIST_REMOVE(tm, lh);
        evtimer_del(&tm->ev);
        free(tm);
    }
#ifdef __WITH_LOCK
        g_lock_put();
#endif //__WITH_LOCK
    return (NULL);
}

static void *stream_wr_thread (void *arg)
{
    struct stream_rw *s, *s_next;
    char *test = "test running";

    LIST_FOREACH_SAFE(s, &ll_stream_rw, lh, s_next) {
        s->cnt = 0;
        write(s->pair[0], test, strlen(test)+1);
        while (!s->cnt) {
            nanosleep(&wakeup, NULL);
        }
    }

    return (NULL);
}

static void *stream_random_wr_thread (void *arg)
{
    struct stream_rw *s, *s_next;
    char *test = "test running";
    int32_t i = 0;

    while (1) {        
        assert(!pthread_mutex_lock(&llk));
        LIST_FOREACH_SAFE(s, &ll_stream_rw, lh, s_next) {
            if (!(++i % 5))
                break;
        }

        assert(!s->cnt);
        s->refcnt++;
        assert(!pthread_mutex_unlock(&llk));

        write(s->pair[0], test, strlen(test)+1);
        while (!s->cnt) {
            nanosleep(&wakeup, NULL);
        }
        s->cnt = 0;
        
        assert(!pthread_mutex_lock(&llk));
        if (!s->refcnt) {      
#ifdef __WITH_LOCK
            g_lock_get();
#endif //__WITH_LOCK
            event_del(&s->ev);

            assert(s->pair[0] < FDS_CLOSED_ARRAY_SIZE);
            assert(s->pair[1] < FDS_CLOSED_ARRAY_SIZE);
            fds_closed_array[s->pair[0]]++;
            fds_closed_array[s->pair[1]]++;
#ifdef __WITH_LOCK
            g_lock_put();
#endif //__WITH_LOCK      
            close(s->pair[0]);
            close(s->pair[1]);

            memset(s, 0, sizeof(*s));
            free(s);
        } else {
            s->refcnt--;
        }

        if (LIST_EMPTY(&ll_stream_rw)) {
            assert(!pthread_mutex_unlock(&llk));
            break;
        }
        assert(!pthread_mutex_unlock(&llk));        
    }

    return (NULL);
}

static void *stream_random_desc_add_del_thread (void *arg)
{
    struct stream_rw *s = NULL, *s_next, *sn;
    int32_t i, iter = 0, del_id, del_pair;

    while (iter++ < MAX_FD_ADD_DEL_ITER) {
        sn = create_stream(EV_READ|EV_PERSIST);
        event_set(&sn->ev, sn->pair[1], sn->rw, periodic_stream_cb, sn);

        assert(!pthread_mutex_lock(&llk));
        i = 0;
        LIST_FOREACH_SAFE(s, &ll_stream_rw, lh, s_next) {
            i++;
        }
        assert(i);
        i = rand() % i;
        LIST_FOREACH_SAFE(s, &ll_stream_rw, lh, s_next) {
            --i;
            if (i <= 0)
                break;
        }
        
        LIST_REMOVE(s, lh);
        del_id = s->id;
        del_pair = s->pair[1];
        if (s->refcnt) 
            s->refcnt--;
        else {
#ifdef __WITH_LOCK
            g_lock_get();
#endif //__WITH_LOCK
            event_del(&s->ev);

            assert(s->pair[0] < FDS_CLOSED_ARRAY_SIZE);
            assert(s->pair[1] < FDS_CLOSED_ARRAY_SIZE);
            fds_closed_array[s->pair[0]]++;
            fds_closed_array[s->pair[1]]++;
#ifdef __WITH_LOCK
            g_lock_put();
#endif //__WITH_LOCK
            close(s->pair[0]);
            close(s->pair[1]);

            memset(s, 0, sizeof(*s));
            free(s);
        }

        LIST_INSERT_HEAD(&ll_stream_rw, sn, lh);

#ifdef __WITH_LOCK
        g_lock_get();
#endif //__WITH_LOCK
        event_add(&sn->ev, NULL);
#ifdef __WITH_LOCK
        g_lock_put();
#endif //__WITH_LOCK
        assert(!pthread_mutex_unlock(&llk)); 

        if (0) {
            printf("<< %d:%d:%d:%d:%d >>\n\n", 
                   iter, del_id, sn->id, del_pair, sn->pair[1]);
        }
    }

    assert(!pthread_mutex_lock(&llk));
    LIST_FOREACH_SAFE(s, &ll_stream_rw, lh, s_next) {
        LIST_REMOVE(s, lh);
        if (s->refcnt) 
            s->refcnt--;
        else {
#ifdef __WITH_LOCK
            g_lock_get();
#endif //__WITH_LOCK
            event_del(&s->ev);

            assert(s->pair[0] < FDS_CLOSED_ARRAY_SIZE);
            assert(s->pair[1] < FDS_CLOSED_ARRAY_SIZE);
            fds_closed_array[s->pair[0]]++;
            fds_closed_array[s->pair[1]]++;
#ifdef __WITH_LOCK
            g_lock_put();
#endif //__WITH_LOCK
            close(s->pair[0]);
            close(s->pair[1]);

            memset(s, 0, sizeof(*s));
            free(s);
        }
    }
    assert(!pthread_mutex_unlock(&llk));
    random_rw_done++;

    return (NULL);
}

static void *stream_random_desc_thrash_thread (void *arg)
{
#define S_ARRAY_SIZE 64
    struct stream_rw *s_array[S_ARRAY_SIZE], *s;
    int32_t i;

    while (!random_rw_done) {
        for (i = 0; i < S_ARRAY_SIZE; i++) {
            s_array[i] = create_stream(EV_READ|EV_PERSIST);
            s = s_array[i];
            event_set(&s->ev, s->pair[1], s->rw, periodic_stream_cb, s);

#ifdef __WITH_LOCK
            g_lock_get();
#endif //__WITH_LOCK
            event_add(&s->ev, NULL);
#ifdef __WITH_LOCK
            g_lock_put();
#endif //__WITH_LOCK
        }

        for (i = 0; i < S_ARRAY_SIZE; i++) {
            s = s_array[i];
            assert(!pthread_mutex_lock(&llk));

            assert(s->pair[0] < FDS_CLOSED_ARRAY_SIZE);
            assert(s->pair[1] < FDS_CLOSED_ARRAY_SIZE);
            fds_closed_array[s->pair[0]]++;
            fds_closed_array[s->pair[1]]++;

#ifdef __WITH_LOCK
            g_lock_get();
#endif //__WITH_LOCK            
            event_del(&s->ev);
#ifdef __WITH_LOCK
            g_lock_put();
#endif //__WITH_LOCK

            assert(!pthread_mutex_unlock(&llk));

            close(s->pair[0]);
            close(s->pair[1]);
        
            memset(s, 0, sizeof(*s));
            free(s);
        }
    }

    return (NULL);
}

//-------------------------------------------------------------------------
static void
test1 (void)
{
    pthread_t tid1, tid2, tid3;
    int32_t i;
    struct periodic_tm *tm;

    done_add_del = 0;

    for (i = 0; i < MAX_TIMERS; i++) {
        tm = create_periodic_tm();
        LIST_INSERT_HEAD(&ll_periodic_tm, tm, lh);
        evtimer_set(&tm->ev, periodic_tm_cb, tm);
        evtimer_add(&tm->ev, &tm->tick_tm);
    }

    pthread_create(&tid1, NULL, timer_dispatch_thread, NULL);
    pthread_create(&tid2, NULL, timer_add_del_thread, NULL);
    pthread_create(&tid3, NULL, timer_del_thread, NULL);

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
}

//-------------------------------------------------------------------------
static void
test2 (void)
{
    pthread_t tid1, tid2;
    int32_t i, j, k;
    struct stream_rw *s, *s_next;

    for (i = 0; i < NEVENT; i++) {
        s = create_stream(EV_READ);
        LIST_INSERT_HEAD(&ll_stream_rw, s, lh);
	event_set(&s->ev, s->pair[1], s->rw, periodic_stream_cb, s);
        event_add(&s->ev, NULL);
    }

    pthread_create(&tid1, NULL, stream_dispatch_thread, NULL);
    pthread_create(&tid2, NULL, stream_wr_thread, NULL);

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    LIST_FOREACH_SAFE(s, &ll_stream_rw, lh, s_next) {
        LIST_REMOVE(s, lh);
        close(s->pair[0]);
        close(s->pair[1]);
        free(s);
    }


    for (i = 0; i < MAX_FD_ITER; i++) {
        printf("<< Iter %d >>\n\n", i);

        for (j = 0; j < SOMEPRIME_MOD500; j++) {
            for (k = 0; k < NEVENT; k++) {
                s = create_stream(EV_READ);
                LIST_INSERT_HEAD(&ll_stream_rw, s, lh);
                event_set(&s->ev, s->pair[1], s->rw, periodic_stream_cb, s);
            }

            LIST_FOREACH_SAFE(s, &ll_stream_rw, lh, s_next) {
                event_add(&s->ev, NULL);
            }

            pthread_create(&tid1, NULL, stream_dispatch_thread, NULL);
            pthread_create(&tid2, NULL, stream_wr_thread, NULL);
            
            pthread_join(tid1, NULL);
            pthread_join(tid2, NULL);
        }

        LIST_FOREACH_SAFE(s, &ll_stream_rw, lh, s_next) {
            LIST_REMOVE(s, lh);
            close(s->pair[0]);
            close(s->pair[1]);
            free(s);
        }
    }

}

//-------------------------------------------------------------------------
static void
test3 (void)
{
    pthread_t tid1, tid2, tid3, tid4, tid5;
    int32_t i;
    struct stream_rw *s;

    for (i = 0; i < MAX_FDS; i++) {
        s = create_stream(EV_READ|EV_PERSIST);
        LIST_INSERT_HEAD(&ll_stream_rw, s, lh);
	event_set(&s->ev, s->pair[1], s->rw, periodic_stream_cb, s);
        event_add(&s->ev, NULL);
    }

    pthread_create(&tid1, NULL, stream_dispatch_thread, NULL);
    pthread_create(&tid2, NULL, stream_random_wr_thread, NULL);
    pthread_create(&tid3, NULL, stream_random_desc_add_del_thread, NULL);
    pthread_create(&tid4, NULL, stream_random_desc_thrash_thread, NULL);
    pthread_create(&tid5, NULL, stream_random_desc_thrash_thread, NULL);

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    pthread_join(tid4, NULL);
    pthread_join(tid5, NULL);
}


int
main (int argc, char **argv)
{
    struct event_base *base;

#ifdef __WITH_LOCK
    struct event_mutex m = {&g_lock_get, &g_lock_put};
    base = event_init_with_lock(&m);
#else 
    base = event_init();
#endif // __WITH_LOCK

    fprintf(stderr, "<<<< %s >>>> \n\n", event_get_method());
#ifdef __WITH_LOCK
    fprintf(stderr, "**** With Locks **** \n\n");
#endif // __WITH_LOCK

    printf("\n\n Starting test 1 \n\n");
    test1();
    printf("\n\n Starting test 2 \n\n");
    test2();
    printf("\n\n Starting test 3 \n\n");
    test3();
    printf("\n\n Starting test 2 \n\n");
    test2();
    printf("\n\n Starting test 1 \n\n");
    test1();

    return (0);
}

