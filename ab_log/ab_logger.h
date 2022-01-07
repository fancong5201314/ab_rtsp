#ifndef AB_LOGGER_H_
#define AB_LOGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AB_LOGGER_LEVEL_FATAL           = 0,
    AB_LOGGER_LEVEL_ALERT           = 1,
    AB_LOGGER_LEVEL_CRIT            = 2,
    AB_LOGGER_LEVEL_ERROR           = 3,
    AB_LOGGER_LEVEL_WARN            = 4,
    AB_LOGGER_LEVEL_NOTICE          = 5,
    AB_LOGGER_LEVEL_INFO            = 6,
    AB_LOGGER_LEVEL_DEBUG           = 7,
    AB_LOGGER_LEVEL_TRACE           = 8,
    AB_LOGGER_LEVEL_NOTSET          = 9,
    AB_LOGGER_LEVEL_UNKNOWN         = 10
} ab_logger_level;

typedef enum {
    AB_LOGGER_OUTPUT_TO_STDOUT      = 0,
    AB_LOGGER_OUTPUT_TO_FILE
} ab_logger_output;

extern int ab_logger_init(ab_logger_output to,
    const char *path, const char *log_prefix,
    unsigned int log_max_nums, unsigned int log_max_size);
extern void ab_logger_message(int priority,
    const char *file, int line, const char *fun,
    const char *fmt, ...);
extern int ab_logger_deinit();

#define AB_LOGGER_FATAL(fmt, args...) \
    ab_logger_message(AB_LOGGER_LEVEL_FATAL, __FILE__, __LINE__, __FUNCTION__, fmt, ##args)
#define AB_LOGGER_ALERT(fmt, args...) \
    ab_logger_message(AB_LOGGER_LEVEL_ALERT, __FILE__, __LINE__, __FUNCTION__, fmt, ##args)
#define AB_LOGGER_CRIT(fmt, args...) \
    ab_logger_message(AB_LOGGER_LEVEL_CRIT, __FILE__, __LINE__, __FUNCTION__, fmt, ##args)
#define AB_LOGGER_ERROR(fmt, args...) \
    ab_logger_message(AB_LOGGER_LEVEL_ERROR, __FILE__, __LINE__, __FUNCTION__, fmt, ##args)
#define AB_LOGGER_WARN(fmt, args...) \
    ab_logger_message(AB_LOGGER_LEVEL_WARN, __FILE__, __LINE__, __FUNCTION__, fmt, ##args)
#define AB_LOGGER_NOTICE(fmt, args...) \
    ab_logger_message(AB_LOGGER_LEVEL_NOTICE, __FILE__, __LINE__, __FUNCTION__, fmt, ##args)
#define AB_LOGGER_INFO(fmt, args...) \
    ab_logger_message(AB_LOGGER_LEVEL_INFO, __FILE__, __LINE__, __FUNCTION__, fmt, ##args)
#define AB_LOGGER_DEBUG(fmt, args...) \
    ab_logger_message(AB_LOGGER_LEVEL_DEBUG, __FILE__, __LINE__, __FUNCTION__, fmt, ##args)
#define AB_LOGGER_TRACE(fmt, args...) \
    ab_logger_message(AB_LOGGER_LEVEL_TRACE, __FILE__, __LINE__, __FUNCTION__, fmt, ##args)
#define AB_LOGGER_NOTSET(fmt, args...) \
    ab_logger_message(AB_LOGGER_LEVEL_NOTSET, __FILE__, __LINE__, __FUNCTION__, fmt, ##args)
#define AB_LOGGER_UNKNOWN(fmt, args...) \
    ab_logger_message(AB_LOGGER_LEVEL_UNKNOWN, __FILE__, __LINE__, __FUNCTION__, fmt, ##args)

#ifdef __cplusplus
}
#endif

#endif // AB_LOGGER_H_
