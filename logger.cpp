//
// Created by kiwipodptak on 31.05.19.
//

#include <iostream>
#include "logger.h"

logger::logger() {
    print_msgs = false;
}

void logger::enable_print_msgs() {
    print_msgs = true;
}

void logger::log(std::string msg) {
    if (print_msgs)
        std::cout << "[DEBUG] " << msg << std::endl;
}
