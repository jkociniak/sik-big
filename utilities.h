//
// Created by kiwipodptak on 25.05.19.
//

#ifndef SIK_DUZY_HELPER_H
#define SIK_DUZY_HELPER_H

#include <unistd.h>
#include <boost/program_options.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstdarg>
#include <cerrno>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstring>

#define DEFAULT_MAX_SPACE 52428800
#define DEFAULT_TIMEOUT 5
#define MAX_RCV_PACKET_SIZE 65507
#define MAX_CHUNK_SIZE 500
#define DEFAULT_QUEUE_LENGTH 128

struct __attribute__((__packed__)) SIMPL_CMD {
    char cmd[10];
    uint64_t cmd_seq;
};

struct __attribute__((__packed__)) CMPLX_CMD {
    char cmd[10];
    uint64_t cmd_seq;
    uint64_t param;
};

void syserr(const char *fmt, ...);

enum MSG_TYPE {
    SIMPL,
    CMPLX,
    ERROR
};

enum MSG_CMD {
    HELLO,
    LIST,
    GET,
    DEL,
    ADD,
    GOOD_DAY,
    MY_LIST,
    CONNECT_ME,
    NO_WAY,
    CAN_ADD
};

enum RUN_MODE {
    SERVER,
    CLIENT
};

class syserr_ex : std::exception {
  private:
    std::string fail_cause;
  public:
    syserr_ex(std::string fc);
    const char* what() const noexcept override;
};

#endif //SIK_DUZY_HELPER_H
