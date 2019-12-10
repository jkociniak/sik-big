//
// Created by kiwipodptak on 31.05.19.
//

#ifndef SIK_DUZY_LOGGER_H
#define SIK_DUZY_LOGGER_H

#include <string>

class logger {
  private:
    bool print_msgs;

  public:
    logger();
    void enable_print_msgs();
    void log(std::string msg);
};


#endif //SIK_DUZY_LOGGER_H
