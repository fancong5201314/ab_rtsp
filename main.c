#include "ab_rtsp.h"

#include "ab_base/ab_mem.h"

#include "ab_log/ab_logger.h"

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/signal.h>

static bool g_quit = true;

static void signal_catch(int signal_num) {
    if (SIGINT == signal_num)
        g_quit = true;
}

int main(int argc, char *argv[]) {
    if (argc < 2)
        return -1;

    const char *in_file = argv[1];

    signal(SIGINT, signal_catch);

    ab_logger_init(AB_LOGGER_OUTPUT_TO_STDOUT, ".", "log", 100, 1024 * 1024);
    AB_LOGGER_INFO("startup.\n");

    ab_rtsp_t rtsp = ab_rtsp_new(AB_RTSP_OVER_TCP);

    AB_LOGGER_INFO("RTSP server startup.\n");

    g_quit = false;
    int nread = 0;
    const unsigned int data_buf_size = 10 * 1024;
    char *data_buf = (char *) ALLOC(data_buf_size);
    FILE *file = fopen(in_file, "rb");
    while (!g_quit) {
        if (file != NULL) {
            nread = fread(data_buf, 1, data_buf_size, file);
            if (nread > 0)
                ab_rtsp_send(rtsp, data_buf, nread);
            else {
                ab_rtsp_send(rtsp, NULL, 0);
                fseek(file, 0, SEEK_SET);
            }
        }
        usleep(40 * 1000);
    }

    FREE(data_buf);
    data_buf = NULL;

    fclose(file);

    AB_LOGGER_INFO("RTSP server quit.\n");

    ab_rtsp_free(&rtsp);

    AB_LOGGER_INFO("shutdown.\n");
    ab_logger_deinit();

    return 0;
}
