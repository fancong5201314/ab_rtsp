#include "ab_rtsp.h"

#include "ab_log/ab_logger.h"

#include <stdbool.h>
#include <unistd.h>
#include <sys/signal.h>

static bool g_quit = true;

static void signal_catch(int signal_num) {
    if (SIGINT == signal_num) {
        g_quit = true;
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_catch);

    ab_logger_init(AB_LOGGER_OUTPUT_TO_STDOUT, ".", "log", 100, 1024 * 1024);
    AB_LOGGER_INFO("startup.\n");

    ab_rtsp_t rtsp = ab_rtsp_new(AB_RTSP_OVER_TCP);

    AB_LOGGER_INFO("RTSP server startup.\n");

    g_quit = false;
    while (!g_quit)
        sleep(1);

    AB_LOGGER_INFO("RTSP server quit.\n");

    ab_rtsp_free(&rtsp);

    AB_LOGGER_INFO("shutdown.\n");
    ab_logger_deinit();

    return 0;
}
