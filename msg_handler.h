//
// Created by kiwipodptak on 26.05.19.
//

#ifndef SIK_DUZY_MSG_PARSER_H
#define SIK_DUZY_MSG_PARSER_H

#include "utilities.h"

class msg_handler {
  public:
    RUN_MODE mode;
    MSG_CMD cmd;
    uint64_t cmd_seq;
    uint64_t param;
    char data[MAX_CHUNK_SIZE];
    size_t data_len;
    MSG_TYPE type;

    class invalid_packet_ex : std::exception {
      private:
        std::string msg;

      public:
        invalid_packet_ex(std::string m);
        const char* what() const noexcept override;
    };

    msg_handler(RUN_MODE m);

    void check_cmd(char* const buffer);
    void decode(char *const buffer, ssize_t rcv_len);
    void encode(char* const buffer, std::string cmd, uint64_t cmd_seq,
                uint64_t param, std::string data, MSG_TYPE type);
};


#endif //SIK_DUZY_MSG_PARSER_H
