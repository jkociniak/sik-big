//
// Created by kiwipodptak on 30.05.19.
//

#ifndef SIK_DUZY_TCP_SOCKET_DATA_H
#define SIK_DUZY_TCP_SOCKET_DATA_H

#include "utilities.h"

enum CONN_STATE {
    SET,
    WAITING,
    CONNECTED,
    SUCCEED,
    FAILED
};

class tcp_socket_data {
  private:
    char buffer[MAX_CHUNK_SIZE];
    off_t buffer_offset;
    std::string file_path;
    off_t file_size;
    FILE* fp;
    size_t bytes_left;
    size_t last_packet_bytes_left;

    int sock;
    int msg_sock;

  public:
    MSG_CMD type;
    CONN_STATE state;
    int timeout;
    tcp_socket_data(int socket, std::string fpath, off_t fs, int timeout, MSG_CMD type);
    void clean();
    std::pair<std::string, off_t> get_file_info();
    void begin_waiting();
    void close_sockets();
    void init();
    int accept_conn();
    void transfer();
};


#endif //SIK_DUZY_TCP_SOCKET_DATA_H
