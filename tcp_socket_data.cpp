//
// Created by kiwipodptak on 30.05.19.
//

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstdint>
#include <sys/stat.h>
#include <dirent.h>
#include <iostream>

#include "tcp_socket_data.h"

tcp_socket_data::tcp_socket_data(int socket, std::string fpath, off_t fs,
                                 int timeout, MSG_CMD type) {
    sock = socket;
    msg_sock = -1;
    file_path = std::move(fpath);
    file_size = fs;
    this->timeout = timeout;
    this->type = type;
    bytes_left = static_cast<size_t>(file_size);
    last_packet_bytes_left = 0;
    state = SET;
    fp = NULL;
}

void tcp_socket_data::clean() {
    if (state == FAILED && type == ADD) {
        remove(file_path.c_str());
    }

    if (fp != NULL)
        fclose(fp);
}

void tcp_socket_data::begin_waiting() {
    state = WAITING;
}

void tcp_socket_data::close_sockets() {
    close(sock);
    close(msg_sock);
}

void tcp_socket_data::init() {
    if (type == ADD) {
        fp = fopen(file_path.c_str(), "w");

        if (fp == NULL)
           state = FAILED;

        fseek(fp, 0, SEEK_SET);
    } else {
        fp = fopen(file_path.c_str(), "r");

        if (fp == NULL)
            state = FAILED;

        fseek(fp, 0, SEEK_SET);
    }
}

int tcp_socket_data::accept_conn() {
    sockaddr client_address;
    socklen_t client_address_len = sizeof(sockaddr);

    msg_sock = accept(sock, &client_address,
                       &client_address_len);

    if (msg_sock < 0) {
        state = FAILED;
    } else
        state = CONNECTED;
    return msg_sock;
}

void tcp_socket_data::transfer() {
    if (type == GET) {
        if (last_packet_bytes_left == 0) {
            size_t chunk_size = bytes_left < MAX_CHUNK_SIZE ? bytes_left : MAX_CHUNK_SIZE;
            fread(buffer, 1, chunk_size, fp);
            buffer_offset = 0;
            last_packet_bytes_left = chunk_size;
        }

        ssize_t len = write(msg_sock, buffer + buffer_offset, last_packet_bytes_left);
        if (len < 0)
            state = FAILED;

        last_packet_bytes_left -= len;
        bytes_left -= len;

        if (bytes_left == 0)
            state = SUCCEED;
    } else {
        ssize_t len = read(msg_sock, buffer, MAX_CHUNK_SIZE);
        if (len < 0)
            state = FAILED;

        auto u_len = static_cast<size_t>(len);
        fwrite(buffer, 1, u_len, fp);

        bytes_left -= len;

        if (bytes_left == 0)
            state = SUCCEED;
    }
}

std::pair<std::string, off_t> tcp_socket_data::get_file_info() {
    size_t slash_pos = file_path.find('/');
    std::string filename = file_path.substr(slash_pos + 1);
    return std::pair<std::string, off_t>(filename, file_size);
}