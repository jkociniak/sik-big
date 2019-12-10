//
// Created by kiwipodptak on 26.05.19.
//

#include <netinet/in.h>
#include "msg_handler.h"

/* -----------possible commands info------------
 * from client:
 * "HELLO", SIMPL, empty data
 * "LIST", SIMPL, empty or nonempty data
 * "GET", SIMPL, nonempty data
 * "DEL", SIMPL, nonempty data
 * "ADD", CMPLX, nonempty data
 *
 * from server:
 * "GOOD_DAY", CMPLX, nonempty data
 * "MY_LIST", CMPLX, nonempty data
 * "CONNECT_ME", CMPLX, nonempty data
 * "NO_WAY", SIMPL, nonempty data
 * "CAN_ADD", CMPLX, empty data
 * */

msg_handler::msg_handler(RUN_MODE m) {
    mode = m;
}

msg_handler::invalid_packet_ex::invalid_packet_ex(std::string m) : msg(m) {}

const char* msg_handler::invalid_packet_ex::what() const noexcept {
    return msg.c_str();
}

void msg_handler::check_cmd(char* const buffer) {
    char cmd_str[11];
    cmd_str[10] = '\0';
    strncpy(cmd_str, buffer, 10);
    bool zeroes = true; //cmd should be padded with 0s

    if (strcmp(cmd_str, "HELLO") == 0 ||
        strcmp(cmd_str, "LIST") == 0 ||
        strcmp(cmd_str, "GET") == 0 ||
        strcmp(cmd_str, "DEL") == 0 ||
        strcmp(cmd_str, "NO_WAY") == 0) {
        for (size_t i = strlen(cmd_str); i < 10; i++) {
            if (cmd_str[i] != '\0')
                zeroes = false;
        }

        type = zeroes ? SIMPL : ERROR;

        if (strcmp(cmd_str, "HELLO") == 0)
            cmd = HELLO;
        else if (strcmp(cmd_str, "LIST") == 0)
            cmd = LIST;
        else if (strcmp(cmd_str, "GET") == 0)
            cmd = GET;
        else if (strcmp(cmd_str, "DEL") == 0)
            cmd = DEL;
        else
            cmd = NO_WAY;
    } else if (strcmp(cmd_str, "ADD") == 0 ||
               strcmp(cmd_str, "GOOD_DAY") == 0 ||
               strcmp(cmd_str, "MY_LIST") == 0 ||
               strcmp(cmd_str, "CONNECT_ME") == 0 ||
               strcmp(cmd_str, "CAN_ADD") == 0) {
        for (size_t i = strlen(cmd_str); i < 10; i++) {
            if (cmd_str[i] != '\0')
                zeroes = false;
        }

        type = zeroes? CMPLX : ERROR;

        if (strcmp(cmd_str, "ADD") == 0)
            cmd = ADD;
        else if (strcmp(cmd_str, "GOOD_DAY") == 0)
            cmd = GOOD_DAY;
        else if (strcmp(cmd_str, "MY_LIST") == 0)
            cmd = MY_LIST;
        else if (strcmp(cmd_str, "CONNECT_ME") == 0)
            cmd = CONNECT_ME;
        else
            cmd = CAN_ADD;
    } else {
        type = ERROR;
    }

    if (mode == CLIENT && (cmd == HELLO || cmd == LIST || cmd == GET || cmd == DEL || cmd == ADD))
        type = ERROR;

    if (mode == SERVER && (cmd == GOOD_DAY || cmd == MY_LIST || cmd == CONNECT_ME || cmd == NO_WAY || cmd == CAN_ADD))
        type = ERROR;
}


void msg_handler::decode(char *const buffer, ssize_t rcv_len) {
    if (rcv_len < sizeof(SIMPL_CMD))
        throw invalid_packet_ex("packet is too small to contain message compatible with protocol");

    check_cmd(buffer);
    memcpy(&cmd_seq, buffer+10, sizeof(uint64_t));
    cmd_seq = be64toh(cmd_seq);

    if (type == ERROR) {
        throw invalid_packet_ex("field cmd does not contain a proper command");
    } else if (type == SIMPL) {
        if (cmd == HELLO && rcv_len > sizeof(SIMPL_CMD))
            throw invalid_packet_ex("cmd HELLO with nonempty data field");
        else if (cmd == GET && rcv_len == sizeof(SIMPL_CMD))
            throw invalid_packet_ex("cmd GET with empty data field");
        else if (cmd == DEL && rcv_len == sizeof(SIMPL_CMD))
            throw invalid_packet_ex("cmd DEL with empty data field");
        else if (cmd == NO_WAY && rcv_len == sizeof(SIMPL_CMD))
            throw invalid_packet_ex("cmd NO_WAY with empty data field");

        if (cmd == LIST || cmd == GET || cmd == DEL || cmd == NO_WAY) {
            data_len = rcv_len - 18;
            strncpy(data, buffer + 18, data_len);
        }
    } else { //CMPLX
        if (rcv_len < sizeof(CMPLX_CMD))
            throw invalid_packet_ex("field cmd hints on CMPLX_CMD msg but packet is too small to contain CMPLX_CMD msg");

        memcpy(&param, buffer+18, sizeof(uint64_t));
        param = be64toh(param); //10 chars and uint64_t make 18 bytes

        if (cmd == CAN_ADD && rcv_len > sizeof(CMPLX_CMD))
            throw invalid_packet_ex("cmd CAN_ADD with nonempty data field");
        else if (cmd == ADD && rcv_len == sizeof(CMPLX_CMD))
            throw invalid_packet_ex("cmd ADD with empty data field");
        else if (cmd == GOOD_DAY && rcv_len == sizeof(CMPLX_CMD))
            throw invalid_packet_ex("cmd GOOD_DAY with empty data field");
        else if (cmd == MY_LIST && rcv_len == sizeof(CMPLX_CMD))
            throw invalid_packet_ex("cmd MY_LIST with empty data field");
        else if (rcv_len == sizeof(CMPLX_CMD)) //CONNECT_ME
            throw invalid_packet_ex("cmd CONNECT_ME with empty data field");

        if (cmd == ADD || cmd == GOOD_DAY || cmd == MY_LIST || cmd == CONNECT_ME) {
            data_len = rcv_len - 26; //10 chars and 2 times uint64_t make 26 bytes
            strncpy(data, buffer + 26, data_len);
        }
    }
}

void msg_handler::encode(char* const buffer, std::string cmd, uint64_t cmd_seq,
                         uint64_t param, std::string data, MSG_TYPE type) {
    size_t offset = 0;
    memcpy(buffer + offset, cmd.c_str(), cmd.size());
    offset += 10;;
    uint64_t cmd_seq_n = htobe64(cmd_seq);
    memcpy(buffer + offset, &cmd_seq_n, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    if (type == CMPLX) {
        uint64_t param_n = htobe64(param);
        memcpy(buffer + offset, &param_n, sizeof(uint64_t));
        offset += sizeof(uint64_t);
    }

    memcpy(buffer + offset, data.c_str(), data.size());
}