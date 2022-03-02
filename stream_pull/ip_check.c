#include "ip_check.h"

#include <stdlib.h>
#include <stdbool.h>

#define IPV4_FLAG 1
#define IPV6_FLAG 2

char IPv4Split = '.';
char IPv6Split = ':';

bool isValidIPV4Num(const char *ip, int begin, int end) {
    int num = 0;
    int order = 0;
    if (ip[begin] == '0' && begin != end)
        return false;

    if (end - begin > 2)
        return false;

    for (int i = begin; i <= end; ++i, ++order) {
        if (ip[i] < '0' || ip[i] > '9')
            return false;
        num = num * 10 + (ip[i] - '0');
    }

    if (num > 255)
        return false;

    return true;
}

bool isValidIPV6Num(const char *IP, int begin, int end) {
    if (end - begin > 3)
        return false;

    for (int i = begin; i <= end ; ++i) {
        if (IP[i] >= '0' && IP[i] <= '9')
            continue;

        if (IP[i] >= 'a' && IP[i] <= 'f')
            continue;

        if (IP[i] >= 'A' && IP[i] <= 'F')
            continue;

        return false;
    }

    return true;
}

bool isValidIPNum(const char *IP, int begin, int end, char split) {
    if (split == IPv6Split)
        return isValidIPV6Num(IP, begin, end);
    return isValidIPV4Num(IP, begin, end);
}

int doCheck(const char *IP, char split, int needCount) {
    int beginIndex = 0;
    int endIndex = 0;
    int times = 0;
    int len = 0;

    for (; IP[len] != '\0'; ++len) {
        if (IP[len] != split)
            continue;

        ++times;
        endIndex = len - 1;
        if (IP[len + 1] == '\0' || IP[len] == IP[len + 1])
            return IP_VERSION_NONE;

        if (!isValidIPNum(IP, beginIndex, endIndex, split))
            return IP_VERSION_NONE;

        beginIndex = len + 1;
    }

    if (beginIndex < len) {
        ++times;
        if (!isValidIPNum(IP, beginIndex, len - 1, split))
            return IP_VERSION_NONE;
    }

    if (times != needCount)
        return IP_VERSION_NONE;

    if (split == IPv4Split)
        return IP_VERSION_4;

    return IP_VERSION_6;
}

int ip_check(const char *ip) {
    int flag = 0;
    for (int i = 0; ip[i] != '\0'; ++i) {
        if (ip[i] == IPv4Split) {
            flag = IPV4_FLAG;
            break;
        }

        if (ip[i] == IPv6Split) {
            flag = IPV6_FLAG;
            break;
        }
    }

    if (flag == IPV4_FLAG)
        return doCheck(ip, IPv4Split, 4);
    else if (flag == IPV6_FLAG)
        return doCheck(ip, IPv6Split, 8);
    return IP_VERSION_NONE;
}
