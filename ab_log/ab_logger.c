#include "ab_logger.h"

#include <log4c.h>
#include <log4c/appender_type_rollingfile.h>
#include <log4c/rollingpolicy_type_sizewin.h>

#include <string.h>
#include <assert.h>
#include <time.h>

#define COLOR_NONE              "\033[m"
#define COLOR_RED               "\033[0;32;31m"
#define COLOR_LIGHT_RED         "\033[1;31m"
#define COLOR_GREEN             "\033[0;32;32m"
#define COLOR_LIGHT_GREEN       "\033[1;32m"
#define COLOR_BLUE              "\033[0;32;34m"
#define COLOR_LIGHT_BLUE        "\033[1;34m"
#define COLOR_DARY_GRAY         "\033[1;30m"
#define COLOR_CYAN              "\033[0;36m"
#define COLOR_LIGHT_CYAN        "\033[1;36m"
#define COLOR_PURPLE            "\033[0;35m"
#define COLOR_LIGHT_PURPLE      "\033[1;35m"
#define COLOR_BROWN             "\033[0;33m"
#define COLOR_YELLOW            "\033[1;33m"
#define COLOR_LIGHT_GRAY        "\033[0;37m"
#define COLOR_WHITE             "\033[1;37m"

static const char *log_priority_color(int priority) {
    const char *text_color = NULL;
    switch (priority) {
        case LOG4C_PRIORITY_FATAL:
            text_color = COLOR_RED;
        break;
        case LOG4C_PRIORITY_ALERT:
            text_color = COLOR_GREEN;
        break;
        case LOG4C_PRIORITY_CRIT:
            text_color = COLOR_BLUE;
        break;
        case LOG4C_PRIORITY_ERROR:
            text_color = COLOR_LIGHT_RED;
        break;
        case LOG4C_PRIORITY_WARN:
            text_color = COLOR_YELLOW;
        break;
        case LOG4C_PRIORITY_NOTICE:
            text_color = COLOR_LIGHT_BLUE;
        break;
        case LOG4C_PRIORITY_INFO:
            text_color = COLOR_LIGHT_GREEN;
        break;
        case LOG4C_PRIORITY_DEBUG:
            text_color = COLOR_BROWN;
        break;
        case LOG4C_PRIORITY_TRACE:
            text_color = COLOR_DARY_GRAY;
        break;
        case LOG4C_PRIORITY_NOTSET:
            text_color = COLOR_CYAN;
        break;
        default:
            text_color = COLOR_PURPLE;
        break;
    }

    return text_color;
}

static const char* ab_layout(const log4c_layout_t *a_layout,
    const log4c_logging_event_t *a_event) {
    static char buffer[4096];
    struct tm tm;
    time_t t;
    const char *text_color = log_priority_color(a_event->evt_priority);

    t = a_event->evt_timestamp.tv_sec;
    /* gmtime_r(&t, &tm); */
    localtime_r(&t, &tm);
#ifdef __APPLE_CC__
    snprintf(buffer, sizeof(buffer), "[%04d%02d%02d %02d:%02d:%02d.%03d] %s[%8s]%s %s",
#else
    snprintf(buffer, sizeof(buffer), "[%04d%02d%02d %02d:%02d:%02d.%03ld] %s[%8s]%s %s",
#endif
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             a_event->evt_timestamp.tv_usec / 1000,
             text_color,
             log4c_priority_to_string(a_event->evt_priority),
             COLOR_NONE,
             a_event->evt_msg);

    return buffer;
}

static int ab_appender_stdout_append(log4c_appender_t* this,
    const log4c_logging_event_t* a_event) {
    return fprintf(stdout, "%s", a_event->evt_rendered_msg);
}

const log4c_layout_type_t log4c_layout_type_ab  = {
    "ab_layout",
    ab_layout,
};

const log4c_appender_type_t log4c_appender_type_ab_stdout = {
    "ab_appender_stdout",
    NULL,
    ab_appender_stdout_append,
    NULL,
};

static log4c_category_t *ab_log4c_category= NULL;
static log4c_appender_t *ab_log4c_appender = NULL;
static log4c_layout_t *ab_log4c_layout = NULL;
static log4c_rollingpolicy_t *ab_log4c_rollingpolicy = NULL;

int ab_logger_init(ab_logger_output to,
    const char *path, const char *log_prefix,
    unsigned int log_max_nums, unsigned int log_max_size) {
    ab_log4c_layout = log4c_layout_new("ab_log4c_layout");
    log4c_layout_set_type(ab_log4c_layout, &log4c_layout_type_ab);

    ab_log4c_appender = log4c_appender_new("ab_log4c_appender");
    log4c_appender_set_layout(ab_log4c_appender, ab_log4c_layout);

    if (AB_LOGGER_OUTPUT_TO_FILE == to) {
        log4c_appender_set_type(ab_log4c_appender, &log4c_appender_type_rollingfile);

        ab_log4c_rollingpolicy = log4c_rollingpolicy_new("ab_log4c_rollingpolicy");
        log4c_rollingpolicy_set_type(ab_log4c_rollingpolicy, &log4c_rollingpolicy_type_sizewin);

        rollingfile_udata_t *ab_log4c_rollingfile_udata = rollingfile_make_udata();

        if (path)
            rollingfile_udata_set_logdir(ab_log4c_rollingfile_udata, path);
        else
            rollingfile_udata_set_logdir(ab_log4c_rollingfile_udata, ".");

        if (log_prefix)
            rollingfile_udata_set_files_prefix(ab_log4c_rollingfile_udata, log_prefix);
        else
            rollingfile_udata_set_files_prefix(ab_log4c_rollingfile_udata, "log");

        rollingpolicy_sizewin_udata_t *rollingpolicy_sizewin = sizewin_make_udata();

        if (log_max_size)
            sizewin_udata_set_file_maxsize(rollingpolicy_sizewin, log_max_size);
        else
            sizewin_udata_set_file_maxsize(rollingpolicy_sizewin, 1024 * 1024);

        if (log_max_nums)
            sizewin_udata_set_max_num_files(rollingpolicy_sizewin, log_max_nums);
        else
            sizewin_udata_set_max_num_files(rollingpolicy_sizewin, 1024);

        log4c_rollingpolicy_set_udata(ab_log4c_rollingpolicy, rollingpolicy_sizewin);
        rollingfile_udata_set_policy(ab_log4c_rollingfile_udata, ab_log4c_rollingpolicy);

        log4c_appender_set_udata(ab_log4c_appender, ab_log4c_rollingfile_udata);
    } else
        log4c_appender_set_type(ab_log4c_appender, &log4c_appender_type_ab_stdout);

    ab_log4c_category = log4c_category_new("ab_log4c_category");
    log4c_category_set_priority(ab_log4c_category, LOG4C_PRIORITY_ERROR);
    log4c_category_set_appender(ab_log4c_category, ab_log4c_appender);

    return 0;
}

void ab_logger_message(int priority,
    const char *file, int line, const char *fun,
    const char *fmt, ...) {
    char new_fmt[2048];
    const char * head_fmt = "[file:%s, line:%d, function:%s] ";
    va_list ap;
    int n;

    assert(ab_log4c_category != NULL);
    n = sprintf(new_fmt, head_fmt, file, line, fun);
    strcat(new_fmt + n,fmt);

    va_start(ap, fmt);
    log4c_category_vlog(ab_log4c_category, priority * 100, new_fmt, ap);
    va_end(ap);
}

int ab_logger_deinit() {
    if (ab_log4c_category != NULL)
        log4c_category_delete(ab_log4c_category);

    if (ab_log4c_rollingpolicy != NULL)
        log4c_rollingpolicy_delete(ab_log4c_rollingpolicy);

    if (ab_log4c_appender != NULL)
        log4c_appender_delete(ab_log4c_appender);

    if (ab_log4c_layout != NULL)
        log4c_layout_delete(ab_log4c_layout);

    return 0;
}
