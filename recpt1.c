/* -*- tab-width: 4; indent-tabs-mode: nil -*- */
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <ctype.h>
#include <libgen.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/ipc.h>
#include <sys/msg.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
//#include "pt1_ioctl.h"

#include "config.h"
#include "decoder.h"
#include "recpt1core.h"
#include "recpt1.h"
#include "mkpath.h"

#include "tssplitter_lite.h"

/* maximum write length at once */
#define SIZE_CHANK 1316

/* ipc message size */
#define MSGSZ     255

/* globals */
extern boolean f_exit;

#define NUM_PRESET_CH 88
struct {
	int lch;
	char *channel;
	int tsid;
} preset_ch[NUM_PRESET_CH] = {
	{ 101, "bs15", 0x40f1 }, { 103, "bs03", 0x4031 },
	{ 141, "bs13", 0x40d0 }, { 151, "bs01", 0x4010 },
	{ 161, "bs01", 0x4011 }, { 171, "bs01", 0x4012 },
	{ 181, "bs13", 0x40d1 }, { 191, "bs03", 0x4030 },
	{ 192, "bs05", 0x4450 }, { 193, "bs05", 0x4451 },
	{ 200, "bs09", 0x4091 }, { 201, "bs15", 0x40f2 },
	{ 202, "bs15", 0x40f2 }, { 211, "bs09", 0x4490 },
	{ 222, "bs09", 0x4092 }, { 231, "bs11", 0x46b2 },
	{ 232, "bs11", 0x46b2 }, { 233, "bs11", 0x46b2 },
	{ 234, "bs19", 0x4730 }, { 236, "bs13", 0x46d2 },
	{ 238, "bs11", 0x46b0 }, { 241, "bs11", 0x46b1 },
	{ 242, "bs19", 0x4731 }, { 243, "bs19", 0x4732 },
	{ 244, "bs21", 0x4751 }, { 245, "bs21", 0x4752 },
	{ 251, "bs23", 0x4770 }, { 252, "bs21", 0x4750 },
	{ 255, "bs23", 0x4771 }, { 256, "bs03", 0x4632 },
	{ 258, "bs23", 0x4772 }, { 531, "bs11", 0x46b2 },
	{ 910, "bs15", 0x40f2 }, { 929, "bs15", 0x40f1 },
	{ 296, "nd02", 0x6020 }, { 298, "nd02", 0x6020 },
	{ 299, "nd02", 0x6020 }, { 100, "nd04", 0x7040 },
	{ 223, "nd04", 0x7040 }, { 227, "nd04", 0x7040 },
	{ 250, "nd04", 0x7040 }, { 342, "nd04", 0x7040 },
	{ 363, "nd04", 0x7040 }, { 294, "nd06", 0x7060 },
	{ 323, "nd06", 0x7060 }, { 329, "nd06", 0x7060 },
	{ 340, "nd06", 0x7060 }, { 341, "nd06", 0x7060 },
	{ 354, "nd06", 0x7060 }, {  55, "nd08", 0x6080 },
	{ 218, "nd08", 0x6080 }, { 219, "nd08", 0x6080 },
	{ 326, "nd08", 0x6080 }, { 339, "nd08", 0x6080 },
	{ 800, "nd10", 0x60a0 }, { 801, "nd10", 0x60a0 },
	{ 802, "nd10", 0x60a0 }, { 805, "nd10", 0x60a0 },
	{ 254, "nd12", 0x70c0 }, { 325, "nd12", 0x70c0 },
	{ 330, "nd12", 0x70c0 }, { 292, "nd14", 0x70e0 },
	{ 293, "nd14", 0x70e0 }, { 310, "nd14", 0x70e0 },
	{ 290, "nd16", 0x7100 }, { 305, "nd16", 0x7100 },
	{ 311, "nd16", 0x7100 }, { 333, "nd16", 0x7100 },
	{ 343, "nd16", 0x7100 }, { 353, "nd16", 0x7100 },
	{ 240, "nd18", 0x7120 }, { 262, "nd18", 0x7120 },
	{ 314, "nd18", 0x7120 }, { 307, "nd20", 0x7140 },
	{ 308, "nd20", 0x7140 }, { 309, "nd20", 0x7140 },
	{ 161, "nd22", 0x7160 }, { 297, "nd22", 0x7160 },
	{ 312, "nd22", 0x7160 }, { 322, "nd22", 0x7160 },
	{ 331, "nd22", 0x7160 }, { 351, "nd22", 0x7160 },
	{ 257, "nd24", 0x7180 }, { 229, "nd24", 0x7180 },
	{ 300, "nd24", 0x7180 }, { 321, "nd24", 0x7180 },
	{ 350, "nd24", 0x7180 }, { 362, "nd24", 0x7180 }
};

void set_lch(char *lch, char **ppch, char **sid, unsigned int *tsid)
{
	int i, ch;
	ch = atoi(lch);
	for (i = 0; i < NUM_PRESET_CH; i++)
		if (preset_ch[i].lch == ch) break;
	if (i < NUM_PRESET_CH ) {
		*ppch = preset_ch[i].channel;
		if (*tsid == 0) *tsid = preset_ch[i].tsid;
		if (*sid == NULL) *sid = lch;
	}
}


//read 1st line from socket
void read_line(int socket, char *p){
	int i;
	for (i=0; i < 255; i++){
		int ret;
		ret = read(socket, p, 1);
			if ( ret == -1 ){
				perror("read");
				exit(1);
			} else if ( ret == 0 ){
				break;
			}
		if ( *p == '\n' ){
			p++;
			break;
		}
		p++;
	}
	*p = '\0';
}


/* will be ipc message receive thread */
void *
mq_recv(void *t)
{
    thread_data *tdata = (thread_data *)t;
    message_buf rbuf;
    char channel[16];
	char service_id[32] = {0};
    int recsec = 0, time_to_add = 0;
    unsigned int tsid = 0;

    while(1) {
        if(msgrcv(tdata->msqid, &rbuf, MSGSZ, 1, 0) < 0) {
            return NULL;
        }

		sscanf(rbuf.mtext, "ch=%s t=%d e=%d sid=%s tsid=%d", channel, &recsec, &time_to_add, service_id, &tsid);


            /* wait for remainder */
            while(tdata->queue->num_used > 0) {
                usleep(10000);
            }
//            if(close_tuner(tdata) != 0)
//                return NULL;

            tune(channel, tdata, 0, tsid);

        if(time_to_add) {
            tdata->recsec += time_to_add;
            fprintf(stderr, "Extended %d sec\n", time_to_add);
        }

        if(recsec) {
            time_t cur_time;
            time(&cur_time);
            if(cur_time - tdata->start_time > recsec) {
                f_exit = TRUE;
            }
            else {
                tdata->recsec = recsec;
                fprintf(stderr, "Total recording time = %d sec\n", recsec);
            }
        }

        if(f_exit)
            return NULL;
    }
}


QUEUE_T *
create_queue(size_t size)
{
    QUEUE_T *p_queue;
    int memsize = sizeof(QUEUE_T) + size * sizeof(BUFSZ*);

    p_queue = (QUEUE_T*)calloc(memsize, sizeof(char));

    if(p_queue != NULL) {
        p_queue->size = size;
        p_queue->num_avail = size;
        p_queue->num_used = 0;
        pthread_mutex_init(&p_queue->mutex, NULL);
        pthread_cond_init(&p_queue->cond_avail, NULL);
        pthread_cond_init(&p_queue->cond_used, NULL);
    }

    return p_queue;
}

void
destroy_queue(QUEUE_T *p_queue)
{
    if(!p_queue)
        return;

    pthread_mutex_destroy(&p_queue->mutex);
    pthread_cond_destroy(&p_queue->cond_avail);
    pthread_cond_destroy(&p_queue->cond_used);
    free(p_queue);
}

/* enqueue data. this function will block if queue is full. */
void
enqueue(QUEUE_T *p_queue, BUFSZ *data)
{
    struct timeval now;
    struct timespec spec;
    int retry_count = 0;

    pthread_mutex_lock(&p_queue->mutex);
    /* entered critical section */

    /* wait while queue is full */
    while(p_queue->num_avail == 0) {

        gettimeofday(&now, NULL);
        spec.tv_sec = now.tv_sec + 1;
        spec.tv_nsec = now.tv_usec * 1000;

        pthread_cond_timedwait(&p_queue->cond_avail,
                               &p_queue->mutex, &spec);
        retry_count++;
        if(retry_count > 60) {
            f_exit = TRUE;
        }
        if(f_exit) {
            pthread_mutex_unlock(&p_queue->mutex);
            return;
        }
    }

    p_queue->buffer[p_queue->in] = data;

    /* move position marker for input to next position */
    p_queue->in++;
    p_queue->in %= p_queue->size;

    /* update counters */
    p_queue->num_avail--;
    p_queue->num_used++;

    /* leaving critical section */
    pthread_mutex_unlock(&p_queue->mutex);
    pthread_cond_signal(&p_queue->cond_used);
}

/* dequeue data. this function will block if queue is empty. */
BUFSZ *
dequeue(QUEUE_T *p_queue)
{
    struct timeval now;
    struct timespec spec;
    BUFSZ *buffer;
    int retry_count = 0;

    pthread_mutex_lock(&p_queue->mutex);
    /* entered the critical section*/

    /* wait while queue is empty */
    while(p_queue->num_used == 0) {

        gettimeofday(&now, NULL);
        spec.tv_sec = now.tv_sec + 1;
        spec.tv_nsec = now.tv_usec * 1000;

        pthread_cond_timedwait(&p_queue->cond_used,
                               &p_queue->mutex, &spec);
        retry_count++;
        if(retry_count > 60) {
            f_exit = TRUE;
        }
        if(f_exit) {
            pthread_mutex_unlock(&p_queue->mutex);
            return NULL;
        }
    }

    /* take buffer address */
    buffer = p_queue->buffer[p_queue->out];

    /* move position marker for output to next position */
    p_queue->out++;
    p_queue->out %= p_queue->size;

    /* update counters */
    p_queue->num_avail++;
    p_queue->num_used--;

    /* leaving the critical section */
    pthread_mutex_unlock(&p_queue->mutex);
    pthread_cond_signal(&p_queue->cond_avail);

    return buffer;
}

/* this function will be reader thread */
void *
reader_func(void *p)
{
    thread_data *tdata = (thread_data *)p;
    QUEUE_T *p_queue = tdata->queue;
    decoder *dec = tdata->decoder;
    splitter *splitter = tdata->splitter;
    int wfd = tdata->wfd;
    boolean use_b25 = dec ? TRUE : FALSE;
    boolean use_udp = tdata->sock_data ? TRUE : FALSE;
    boolean fileless = FALSE;
    boolean use_splitter = splitter ? TRUE : FALSE;
    int sfd = -1;
    pthread_t signal_thread = tdata->signal_thread;
//    struct sockaddr_in *addr = NULL;
    BUFSZ *qbuf;
    static splitbuf_t splitbuf;
    ARIB_STD_B25_BUFFER sbuf, dbuf, buf;
    int code;
    int split_select_finish = TSS_ERROR;

    buf.size = 0;
    buf.data = NULL;
    splitbuf.buffer_size = 0;
    splitbuf.buffer = NULL;

    if(wfd == -1)
        fileless = TRUE;

    if(use_udp) {
        sfd = tdata->sock_data->sfd;
//        addr = &tdata->sock_data->addr;
    }

    while(1) {
        ssize_t wc = 0;
        int file_err = 0;
        qbuf = dequeue(p_queue);
        /* no entry in the queue */
        if(qbuf == NULL) {
            break;
        }

        sbuf.data = qbuf->buffer;
        sbuf.size = qbuf->size;

        buf = sbuf; /* default */

        if(use_b25) {
            code = b25_decode(dec, &sbuf, &dbuf);
            if(code < 0) {
                fprintf(stderr, "b25_decode failed (code=%d). fall back to encrypted recording.\n", code);
                use_b25 = FALSE;
            }
            else
                buf = dbuf;
        }


        if(use_splitter) {
            splitbuf.buffer_filled = 0;

            /* allocate split buffer */
            if(splitbuf.buffer_size < buf.size && buf.size > 0) {
                splitbuf.buffer = realloc(splitbuf.buffer, buf.size);
                if(splitbuf.buffer == NULL) {
                    fprintf(stderr, "split buffer allocation failed\n");
                    use_splitter = FALSE;
                    goto fin;
                }
            }

            while(buf.size) {
                /* $BJ,N%BP>](JPID$B$NCj=P(J */
                if(split_select_finish != TSS_SUCCESS) {
                    split_select_finish = split_select(splitter, &buf);
                    if(split_select_finish == TSS_NULL) {
                        /* malloc$B%(%i!<H/@8(J */
                        fprintf(stderr, "split_select malloc failed\n");
                        use_splitter = FALSE;
                        goto fin;
                    }
                    else if(split_select_finish != TSS_SUCCESS) {
                        /* $BJ,N%BP>](JPID$B$,40A4$KCj=P$G$-$k$^$G=PNO$7$J$$(J
                         * 1$BICDxEYM>M5$r8+$k$H$$$$$+$b(J
                         */
                        time_t cur_time;
                        time(&cur_time);
                        if(cur_time - tdata->start_time > 4) {
                            use_splitter = FALSE;
                            goto fin;
                        }
                        break;
                    }
                }

                /* $BJ,N%BP>]0J30$r$U$k$$Mn$H$9(J */
                code = split_ts(splitter, &buf, &splitbuf);
                if(code == TSS_NULL) {
                    fprintf(stderr, "PMT reading..\n");
                }
                else if(code != TSS_SUCCESS) {
                    fprintf(stderr, "split_ts failed\n");
                    break;
                }

                break;
            } /* while */

            buf.size = splitbuf.buffer_filled;
            buf.data = splitbuf.buffer;
        fin:
            ;
        } /* if */


        if(!fileless) {
            /* write data to output file */
            int size_remain = buf.size;
            int offset = 0;

            while(size_remain > 0) {
                int ws = size_remain < SIZE_CHANK ? size_remain : SIZE_CHANK;

                wc = write(wfd, buf.data + offset, ws);
                if(wc < 0) {
                    perror("write");
                    file_err = 1;
                    pthread_kill(signal_thread,
                                 errno == EPIPE ? SIGPIPE : SIGUSR2);
                    break;
                }
                size_remain -= wc;
                offset += wc;
            }
        }

        if(use_udp && sfd != -1) {
            /* write data to socket */
            int size_remain = buf.size;
            int offset = 0;
            while(size_remain > 0) {
                int ws = size_remain < SIZE_CHANK ? size_remain : SIZE_CHANK;
                wc = write(sfd, buf.data + offset, ws);
                if(wc < 0) {
                    if(errno == EPIPE)
                        pthread_kill(signal_thread, SIGPIPE);
                    break;
                }
                size_remain -= wc;
                offset += wc;
            }
        }

        free(qbuf);
        qbuf = NULL;

        /* normal exit */
        if((f_exit && !p_queue->num_used) || file_err) {

            buf = sbuf; /* default */

            if(use_b25) {
                code = b25_finish(dec, &sbuf, &dbuf);
                if(code < 0)
                    fprintf(stderr, "b25_finish failed\n");
                else
                    buf = dbuf;
            }

            if(use_splitter) {
                /* $BJ,N%BP>]0J30$r$U$k$$Mn$H$9(J */
                code = split_ts(splitter, &buf, &splitbuf);
                if(code == TSS_NULL) {
                    split_select_finish = TSS_ERROR;
                    fprintf(stderr, "PMT reading..\n");
                }
                else if(code != TSS_SUCCESS) {
                    fprintf(stderr, "split_ts failed\n");
                    break;
                }

                buf.data = splitbuf.buffer;
                buf.size = splitbuf.buffer_size;
            }

            if(!fileless && !file_err) {
                wc = write(wfd, buf.data, buf.size);
                if(wc < 0) {
                    perror("write");
                    file_err = 1;
                    pthread_kill(signal_thread,
                                 errno == EPIPE ? SIGPIPE : SIGUSR2);
                }
            }

            if(use_udp && sfd != -1) {
                wc = write(sfd, buf.data, buf.size);
                if(wc < 0) {
                    if(errno == EPIPE)
                        pthread_kill(signal_thread, SIGPIPE);
                }
            }

            if(use_splitter) {
                free(splitbuf.buffer);
                splitbuf.buffer = NULL;
                splitbuf.buffer_size = 0;
            }

            break;
        }
    }

    time_t cur_time;
    time(&cur_time);
    fprintf(stderr, "Recorded %dsec\n",
            (int)(cur_time - tdata->start_time));

    return NULL;
}

void
show_usage(char *cmd)
{
#ifdef HAVE_LIBARIB25
    fprintf(stderr, "Usage: \n%s [--b25 [--round N] [--strip] [--EMM]] [--udp [--addr hostname --port portnumber]] [--http portnumber] [--dev devicenumber] [--lnb voltage] [--sid SID1,SID2] [--tsid TSID] [--lch] channel rectime destfile\n", cmd);
#else
    fprintf(stderr, "Usage: \n%s [--udp [--addr hostname --port portnumber]] [--http portnumber] [--dev devicenumber] [--lnb voltage] [--sid SID1,SID2] [--tsid TSID] [--lch] channel rectime destfile\n", cmd);
#endif
    fprintf(stderr, "\n");
    fprintf(stderr, "Remarks:\n");
    fprintf(stderr, "if channel begins with 'bs##' or 'nd##', means BS/CS channel, '##' is numeric.\n");
    fprintf(stderr, "if rectime  is '-', records indefinitely.\n");
    fprintf(stderr, "if destfile is '-', stdout is used for output.\n");
}

void
show_options(void)
{
    fprintf(stderr, "Options:\n");
#ifdef HAVE_LIBARIB25
    fprintf(stderr, "--b25:               Decrypt using BCAS card\n");
    fprintf(stderr, "  --round N:         Specify round number\n");
    fprintf(stderr, "  --strip:           Strip null stream\n");
    fprintf(stderr, "  --EMM:             Instruct EMM operation\n");
#endif
    fprintf(stderr, "--udp:               Turn on udp broadcasting\n");
    fprintf(stderr, "  --addr hostname:   Hostname or address to connect\n");
    fprintf(stderr, "  --port portnumber: Port number to connect\n");
    fprintf(stderr, "--http portnumber:   Turn on http broadcasting (run as a daemon)\n");
    fprintf(stderr, "--dev N:             Use DVB device /dev/dvb/adapterN\n");
    fprintf(stderr, "--lnb voltage:       Specify LNB voltage (0, 11, 15)\n");
    fprintf(stderr, "--sid SID1,SID2,...: Specify SID number in CSV format (101,102,...)\n");
    fprintf(stderr, "--tsid TSID:         Specify TSID in decimal or hex, hex begins '0x'\n");
    fprintf(stderr, "--lch:               Specify channel as BS/CS logical channel instead of physical one\n");
    fprintf(stderr, "--help:              Show this help\n");
    fprintf(stderr, "--version:           Show version\n");
}

void
cleanup(thread_data *tdata)
{
    /* stop recording */
//    ioctl(tdata->tfd, STOP_REC, 0);

    f_exit = TRUE;

    pthread_cond_signal(&tdata->queue->cond_avail);
    pthread_cond_signal(&tdata->queue->cond_used);
}

/* will be signal handler thread */
void *
process_signals(void *t)
{
    sigset_t waitset;
    int sig;
    thread_data *tdata = (thread_data *)t;

    sigemptyset(&waitset);
    sigaddset(&waitset, SIGPIPE);
    sigaddset(&waitset, SIGINT);
    sigaddset(&waitset, SIGTERM);
    sigaddset(&waitset, SIGUSR1);
    sigaddset(&waitset, SIGUSR2);

    sigwait(&waitset, &sig);

    switch(sig) {
    case SIGPIPE:
        fprintf(stderr, "\nSIGPIPE received. cleaning up...\n");
        cleanup(tdata);
        break;
    case SIGINT:
        fprintf(stderr, "\nSIGINT received. cleaning up...\n");
        cleanup(tdata);
        break;
    case SIGTERM:
        fprintf(stderr, "\nSIGTERM received. cleaning up...\n");
        cleanup(tdata);
        break;
    case SIGUSR1: /* normal exit*/
        cleanup(tdata);
        break;
    case SIGUSR2: /* error */
        fprintf(stderr, "Detected an error. cleaning up...\n");
        cleanup(tdata);
        break;
    }

    return NULL; /* dummy */
}

void
init_signal_handlers(pthread_t *signal_thread, thread_data *tdata)
{
    sigset_t blockset;

    sigemptyset(&blockset);
    sigaddset(&blockset, SIGPIPE);
    sigaddset(&blockset, SIGINT);
    sigaddset(&blockset, SIGTERM);
    sigaddset(&blockset, SIGUSR1);
    sigaddset(&blockset, SIGUSR2);

    if(pthread_sigmask(SIG_BLOCK, &blockset, NULL))
        fprintf(stderr, "pthread_sigmask() failed.\n");

    pthread_create(signal_thread, NULL, process_signals, tdata);
}

int
main(int argc, char **argv)
{
    time_t cur_time;
    pthread_t signal_thread;
    pthread_t reader_thread;
    pthread_t ipc_thread;
    QUEUE_T *p_queue = create_queue(MAX_QUEUE);
    BUFSZ   *bufptr;
    decoder *decoder = NULL;
    splitter *splitter = NULL;
    static thread_data tdata;
    decoder_options dopt = {
        4,  /* round */
        0,  /* strip */
        0   /* emm */
    };
    tdata.dopt = &dopt;
    tdata.lnb = 0;
    tdata.tfd = -1;

    int result;
    int option_index;
    struct option long_options[] = {
#ifdef HAVE_LIBARIB25
        { "b25",       0, NULL, 'b'},
        { "B25",       0, NULL, 'b'},
        { "round",     1, NULL, 'r'},
        { "strip",     0, NULL, 's'},
        { "emm",       0, NULL, 'm'},
        { "EMM",       0, NULL, 'm'},
#endif
        { "LNB",       1, NULL, 'n'},
        { "lnb",       1, NULL, 'n'},
        { "udp",       0, NULL, 'u'},
        { "addr",      1, NULL, 'a'},
        { "port",      1, NULL, 'p'},
        { "http",      1, NULL, 'H'},
        { "dev",       1, NULL, 'd'},
        { "help",      0, NULL, 'h'},
        { "version",   0, NULL, 'v'},
        { "sid",       1, NULL, 'i'},
        { "tsid",      1, NULL, 't'},
        { "lch",       0, NULL, 'c'},
        {0, 0, NULL, 0} /* terminate */
    };

    boolean use_b25 = FALSE;
    boolean use_udp = FALSE;
    boolean use_http = FALSE;
    boolean fileless = FALSE;
    boolean use_stdout = FALSE;
    boolean use_splitter = FALSE;
    boolean use_lch = FALSE;
    char *host_to = NULL;
    int port_to = 1234;
    int port_http = 12345;
    sock_data *sockdata = NULL;
    int dev_num = 0;
    int val;
    char *voltage[] = {"0V", "11V", "15V"};
    char *sid_list = NULL;
    unsigned int tsid = 0;
	int connected_socket, listening_socket;
	unsigned int len;
	char *channel, *pch = NULL;

    while((result = getopt_long(argc, argv, "br:smn:ua:H:p:d:hvitcl:",
                                long_options, &option_index)) != -1) {
        switch(result) {
        case 'b':
            use_b25 = TRUE;
            fprintf(stderr, "using B25...\n");
            break;
        case 's':
            dopt.strip = TRUE;
            fprintf(stderr, "enable B25 strip\n");
            break;
        case 'm':
            dopt.emm = TRUE;
            fprintf(stderr, "enable B25 emm processing\n");
            break;
        case 'u':
            use_udp = TRUE;
            host_to = "localhost";
            fprintf(stderr, "enable UDP broadcasting\n");
            break;
        case 'H':
            use_http = TRUE;
            port_http = atoi(optarg);
            fprintf(stderr, "creating a http daemon\n");
            break;
        case 'h':
            fprintf(stderr, "\n");
            show_usage(argv[0]);
            fprintf(stderr, "\n");
            show_options();
            fprintf(stderr, "\n");
            exit(0);
            break;
        case 'v':
            fprintf(stderr, "%s %s\n", argv[0], version);
            fprintf(stderr, "recorder command for DVB tuner.\n");
            exit(0);
            break;
        /* following options require argument */
        case 'n':
            val = atoi(optarg);
            switch(val) {
            case 11:
                tdata.lnb = 1;
                break;
            case 15:
                tdata.lnb = 2;
                break;
            default:
                tdata.lnb = 0;
                break;
            }
            fprintf(stderr, "LNB = %s\n", voltage[tdata.lnb]);
            break;
        case 'r':
            dopt.round = atoi(optarg);
            fprintf(stderr, "set round %d\n", dopt.round);
            break;
        case 'a':
            use_udp = TRUE;
            host_to = optarg;
            fprintf(stderr, "UDP destination address: %s\n", host_to);
            break;
        case 'p':
            port_to = atoi(optarg);
            fprintf(stderr, "UDP port: %d\n", port_to);
            break;
        case 'd':
            dev_num = atoi(optarg);
            fprintf(stderr, "using device: /dev/dvb/adapter%d\n", dev_num);
            break;
        case 'i':
            use_splitter = TRUE;
            sid_list = optarg;
            break;
        case 't':
            tsid = atoi(optarg);
            if(strlen(optarg) > 2){
                if((optarg[0] == '0') && ((optarg[1] == 'X') ||(optarg[1] == 'x'))){
                    sscanf(optarg+2, "%x", &tsid);
                }
            }
            fprintf(stderr, "tsid = 0x%x\n", tsid);
            break;
	case 'c':
	    use_lch = TRUE;
	    break;
        }
    }

if(use_http){	// http-server add-
	fprintf(stderr, "run as a daemon..\n");
	if(daemon(1,1)){
		perror("failed to start");
		return 1;
	}
	fprintf(stderr, "pid = %d\n", getpid());

	struct sockaddr_in sin;
	int ret;
	int sock_optval = 1;
		
	listening_socket = socket(AF_INET, SOCK_STREAM, 0);
	if ( listening_socket == -1 ){
		perror("socket");
		return 1;
	}
		
	if ( setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR,
			&sock_optval, sizeof(sock_optval)) == -1 ){
		perror("setsockopt");
		return 1;
	}
		
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port_http);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

		
	if ( bind(listening_socket, (struct sockaddr *)&sin, sizeof(sin)) < 0 ){
		perror("bind");
		return 1;
	}
		
	ret = listen(listening_socket, SOMAXCONN);
	if ( ret == -1 ){
		perror("listen");
		return 1;
	}
	fprintf(stderr,"listening at port %d\n", port_http);
	//set rectime to the infinite
	if(parse_time("-",&tdata.recsec) != 0){
		return 1;
	}
	if(tdata.recsec == -1)
		tdata.indefinite = TRUE;
}else{	// -http-server add
    if(argc - optind < 3) {
        if(argc - optind == 2 && use_udp) {
            fprintf(stderr, "Fileless UDP broadcasting\n");
            fileless = TRUE;
            tdata.wfd = -1;
        }
        else {
            fprintf(stderr, "Some required parameters are missing!\n");
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 1;
        }
    }

    fprintf(stderr, "pid = %d\n", getpid());

    if(use_lch){
        set_lch(argv[optind], &pch, &sid_list, &tsid);
        if(sid_list) use_splitter = TRUE;
        fprintf(stderr, "tsid = 0x%x\n", tsid);
    }
    if(pch == NULL) pch = argv[optind];

    /* tune */
    if(tune(pch, &tdata, dev_num, tsid) != 0)
        return 1;

    /* set recsec */
    if(parse_time(argv[optind + 1], &tdata.recsec) != 0) // no other thread --yaz
        return 1;

    if(tdata.recsec == -1)
        tdata.indefinite = TRUE;

    /* open output file */
    char *destfile = argv[optind + 2];
    if(destfile && !strcmp("-", destfile)) {
        use_stdout = TRUE;
        tdata.wfd = 1; /* stdout */
    }
    else {
        if(!fileless) {
            int status;
            char *path = strdup(argv[optind + 2]);
            char *dir = dirname(path);
            status = mkpath(dir, 0777);
            if(status == -1)
                perror("mkpath");
            free(path);

            tdata.wfd = open(argv[optind + 2], (O_RDWR | O_CREAT | O_TRUNC), 0666);
            if(tdata.wfd < 0) {
                fprintf(stderr, "Cannot open output file: %s\n",
                        argv[optind + 2]);
                return 1;
            }
        }
    }
}	// http-server add

    /* initialize decoder */
    if(use_b25) {
        decoder = b25_startup(&dopt);
        if(!decoder) {
            fprintf(stderr, "Cannot start b25 decoder\n");
            fprintf(stderr, "Fall back to encrypted recording\n");
            use_b25 = FALSE;
        }
    }

while(1){	// http-server add-
	if(use_http){
		struct hostent *peer_host;
		struct sockaddr_in peer_sin;
		pch = NULL;
		sid_list = NULL;

		len = sizeof(peer_sin);

		connected_socket = accept(listening_socket, (struct sockaddr *)&peer_sin, &len);
		if ( connected_socket == -1 ){
			perror("accept");
			return 1;
		}

		peer_host = gethostbyaddr((char *)&peer_sin.sin_addr.s_addr,
				  sizeof(peer_sin.sin_addr), AF_INET);
		if ( peer_host == NULL ){
			fprintf(stderr, "gethostbyname failed\n");
			return 1;
		}

		fprintf(stderr,"connect from: %s [%s] port %d\n", peer_host->h_name, inet_ntoa(peer_sin.sin_addr), ntohs(peer_sin.sin_port));

		char buf[256];
		read_line(connected_socket, buf);
		fprintf(stderr,"request command is %s\n",buf);
		char s0[256],s1[256],s2[256];
		sscanf(buf,"%s%s%s",s0,s1,s2);
		char delim[] = "/";
		channel = strtok(s1,delim);
		char *sidflg = strtok(NULL,delim);
		if(sidflg)
			sid_list = sidflg;
		if(use_lch)
			set_lch(channel, &pch, &sid_list, &tsid);
		if(pch == NULL) pch = channel;
		fprintf(stderr,"channel is %s\n",channel);

		if(sid_list == NULL){
			use_splitter = FALSE;
			splitter = NULL;
		}else if(!strcmp(sid_list,"all")){
			use_splitter = FALSE;
			splitter = NULL;
		}else{
			use_splitter = TRUE;
		}
	}	// -http-server add

    /* initialize splitter */
    if(use_splitter) {
        splitter = split_startup(sid_list);
        if(splitter->sid_list == NULL) {
            fprintf(stderr, "Cannot start TS splitter\n");
            return 1;
        }
    }

	if(use_http){	// http-server add-
		char header[] =  "HTTP/1.1 200 OK\r\nContent-Type: video/mpeg\r\nCache-Control: no-cache\r\n\r\n";
		write(connected_socket, header, strlen(header));

		//set write target to http
		tdata.wfd = connected_socket;

		//tune
		if(tune(pch, &tdata, dev_num, tsid) != 0){
			fprintf(stderr, "Tuner cannot start recording\n");
			continue;
		}
	}else{	// -http-server add
    /* initialize udp connection */
    if(use_udp) {
      sockdata = calloc(1, sizeof(sock_data));
      struct in_addr ia;
      ia.s_addr = inet_addr(host_to);
      if(ia.s_addr == INADDR_NONE) {
            struct hostent *hoste = gethostbyname(host_to);
            if(!hoste) {
                perror("gethostbyname");
                return 1;
            }
            ia.s_addr = *(in_addr_t*) (hoste->h_addr_list[0]);
        }
        if((sockdata->sfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("socket");
            return 1;
        }

        sockdata->addr.sin_family = AF_INET;
        sockdata->addr.sin_port = htons (port_to);
        sockdata->addr.sin_addr.s_addr = ia.s_addr;

        if(connect(sockdata->sfd, (struct sockaddr *)&sockdata->addr,
                   sizeof(sockdata->addr)) < 0) {
            perror("connect");
            return 1;
        }
    }
	}	// http-server add
    /* prepare thread data */
    tdata.queue = p_queue;
    tdata.decoder = decoder;
    tdata.splitter = splitter;
    tdata.sock_data = sockdata;
    tdata.tune_persistent = FALSE;

    /* spawn signal handler thread */
    init_signal_handlers(&signal_thread, &tdata);

    /* spawn reader thread */
    tdata.signal_thread = signal_thread;
    pthread_create(&reader_thread, NULL, reader_func, &tdata);

    /* spawn ipc thread */
    key_t key;
    key = (key_t)getpid();

    if ((tdata.msqid = msgget(key, IPC_CREAT | 0666)) < 0) {
        perror("msgget");
    }
    pthread_create(&ipc_thread, NULL, mq_recv, &tdata);
    fprintf(stderr, "\nRecording...\n");

    time(&tdata.start_time);

    /* read from tuner */
    while(1) {
        if(f_exit)
            break;

        time(&cur_time);
        bufptr = malloc(sizeof(BUFSZ));
        if(!bufptr) {
            f_exit = TRUE;
            break;
        }
        bufptr->size = read(tdata.tfd, bufptr->buffer, MAX_READ_SIZE);
        if(bufptr->size <= 0) {
            if((cur_time - tdata.start_time) >= tdata.recsec && !tdata.indefinite) {
                f_exit = TRUE;
                enqueue(p_queue, NULL);
                break;
            }
            else {
                free(bufptr);
                continue;
            }
        }
        enqueue(p_queue, bufptr);

        /* stop recording */
        time(&cur_time);
        if((cur_time - tdata.start_time) >= tdata.recsec && !tdata.indefinite) {
            break;
        }
    }

    /* delete message queue*/
    msgctl(tdata.msqid, IPC_RMID, NULL);

    pthread_kill(signal_thread, SIGUSR1);

    /* wait for threads */
    pthread_join(reader_thread, NULL);
    pthread_join(signal_thread, NULL);
    pthread_join(ipc_thread, NULL);

    /* close tuner */
    if(close_tuner(&tdata) != 0)
        return 1;

    /* release queue */
    destroy_queue(p_queue);
	if(use_http){	// http-server add-
		//reset queue
		p_queue = create_queue(MAX_QUEUE);

		/* close http socket */
		close(tdata.wfd);

		fprintf(stderr,"connection closed. still listening at port %d\n",port_http);
		f_exit = FALSE;
	}else{	// -http-server add
    /* close output file */
	if(!use_stdout){
		fsync(tdata.wfd);
        close(tdata.wfd);
	}

    /* free socket data */
    if(use_udp) {
        close(sockdata->sfd);
        free(sockdata);
    }

    /* release decoder */
    if(!use_http)
    if(use_b25) {
        b25_shutdown(decoder);
    }
	}	// http-server add
    if(use_splitter) {
        split_shutdown(splitter);
    }

	if(!use_http)	// http-server add
    return 0;
}	// http-server add
}
