#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include "tcp.h"

#define abs(x) ((x) < 0 ? -(x) : (x))

static volatile int stop = 0;

void usage(void)
{
    fprintf(stderr, "tcping, (C) 2003 folkert@vanheusden.com\n\n");
    fprintf(stderr, "host     (e.g. localhost)\n");
    fprintf(stderr, "-p port    portnumber (e.g. 80)\n");
    fprintf(stderr, "-c count    how many times to connect\n");
    fprintf(stderr, "-i interval    delay between each connect\n");
    fprintf(stderr, "-f        flood connect (no delays)\n");
    fprintf(stderr, "-q        quiet, only returncode\n");
    fprintf(stderr, "-t timeout  connection timeout in seconds\n\n"); 
}

void handler(int sig)
{
    (void)sig; 
    stop = 1;
}

int main(int argc, char *argv[])
{
    char *hostname = NULL;
    char *portnr = "7";
    int c;
    int count = -1, curncount = 0;
    int wait = 1, quiet = 0;
    int ok = 0, err = 0;
    double min = 999999999999999.0, avg = 0.0, max = 0.0;
    struct addrinfo *resolved;
    int errcode;
    int seen_addrnotavail = 0;
    int timeout = 5; 
    while((c = getopt(argc, argv, "h:p:c:i:f:t:q?")) != -1)
    {
        switch(c)
        {
            case 'p':
                portnr = optarg;
                break;

            case 'c':
                count = atoi(optarg);
                break;

            case 'i':
                wait = atoi(optarg);
                break;

            case 'f':
                wait = 0;
                break;

            case 'q':
                quiet = 1;
                break;
                
            case 't':
                timeout = atoi(optarg);
                break;

            case '?':
            default:
                usage();
                return 0;
        }
    }

    if (optind >= argc)
    {
        fprintf(stderr, "No hostname given\n");
        usage();
        return 3;
    }
    hostname = argv[optind];

    signal(SIGINT, handler);
    signal(SIGTERM, handler);

    if ((errcode = lookup(hostname, portnr, &resolved)) != 0)
    {
        fprintf(stderr, "%s\n", gai_strerror(errcode));
        return 2;
    }

    if (!quiet)
        printf("TCPING %s:%s\n", hostname, portnr);

    while((curncount < count || count == -1) && stop == 0)
    {
        double ms;
        struct timeval rtt;

        if ((errcode = connect_to(resolved, &rtt, timeout)) != 0)
        {
            if (errcode != -EADDRNOTAVAIL)
            {
                printf("Connected to %s[:%s]: seq=%d time out!\n", hostname, portnr, curncount + 1);
                err++;
            }
            else
            {
                if (seen_addrnotavail)
                {
                    printf(".");
                    fflush(stdout);
                }
                else
                {
                    printf("Connected to %s[:%s]: seq=%d time out!\n", hostname, portnr, curncount + 1);
                }
                seen_addrnotavail = 1;
            }
        }
        else
        {
            seen_addrnotavail = 0;
            ok++;

            ms = ((double)rtt.tv_sec * 1000.0) + ((double)rtt.tv_usec / 1000.0);
            avg += ms;
            min = min > ms ? ms : min;
            max = max < ms ? ms : max;

           
            printf("Connected to %s[:%s]: seq=%d time=%.2f ms\n", hostname, portnr, curncount + 1, ms);
        }

        curncount++;

        if (curncount != count)
            sleep(wait);
    }

    if (!quiet)
    {
        
        printf("\n--- %s[:%s] tcping statistics ---\n", hostname, portnr);
        printf("%d connections, %d successed, %d failed, %.2f%% success rate\n", curncount, ok, err, ((double)ok / curncount * 100.0));
        printf("minimum = %.2fms, maximum = %.2fms, average = %.2fms\n", min, max, avg / ok);
    }

    freeaddrinfo(resolved);
    if (ok)
        return 0;
    else
        return 127;
}
