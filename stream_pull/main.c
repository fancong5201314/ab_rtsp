/*
 * main.c
 *
 *  Created on: 2022年3月1日
 *      Author: ljm
 */

#include "ab_rtsp_pull_stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/signal.h>

static bool g_quit = true;

static void recv_rtsp_data(const unsigned char *data, unsigned int data_len, void *user_data) {
    FILE *file = (FILE *) user_data;
    if (file != NULL) {
        fwrite(data, 1, data_len, file);
        fflush(file);
    }
}

static void signal_catch(int signal_num) {
    if (SIGINT == signal_num) {
        g_quit = true;
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_catch);

    FILE *file = fopen("test.h264", "wb");
    
    ab_rtsp_pull_stream_t handle = ab_rtsp_pull_stream_new(argv[1], recv_rtsp_data, file);
    g_quit = false;
    while (!g_quit) {
        sleep(1);
    }

    fflush(file);
    fclose(file);

    ab_rtsp_pull_stream_free(&handle);
    return EXIT_SUCCESS;
}