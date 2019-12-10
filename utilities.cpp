//
// Created by kiwipodptak on 02.06.19.
//

#include "utilities.h"

void syserr(const char *fmt, ...) {
    va_list fmt_args;
    int err = errno;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end (fmt_args);

    fprintf(stderr," (%d; %s)\n", err, strerror(err));
}

syserr_ex::syserr_ex(std::string fc) {
    fail_cause = std::move(fc);
}

const char* syserr_ex::what() const noexcept {
    return fail_cause.c_str();
}