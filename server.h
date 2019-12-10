//
// Created by kiwipodptak on 25.05.19.
//

#ifndef SIK_DUZY_SERVER_H
#define SIK_DUZY_SERVER_H

#include <string>
#include <poll.h>
#include <map>
#include "utilities.h"
#include "msg_handler.h"
#include "tcp_socket_data.h"
#include "logger.h"

class server {
  private:
    std::atomic<bool>* quit_flag;
    std::string mcast_addr;
    unsigned port;
    uint64_t max_space;
    std::string shrd_fldr;
    unsigned timeout;

    logger* _logger;

    std::vector<struct pollfd> sockets; //0 socket is udp communication socket
    char comm_buffer[MAX_RCV_PACKET_SIZE];
    void fit_sockets_vector();
    std::map<int, tcp_socket_data> tcp_sockets_data;
    void add_tcp_socket(int socket, std::string fpath, off_t fsize, int timeout, MSG_CMD type);
    void add_udp_socket(int socket);
    std::pair<int, int> get_min_timeout();

    struct ip_mreq ip_mreq;

    msg_handler handler;

    std::vector<std::pair<std::string, off_t>> files;
    uint64_t free_space;
    void prepare_file_list();

    void handle_conn_socket();
    void handle_tcp_socket(int index, int diff);
    void delete_socket(int socket);
    void replace_socket(int old_socket_idx, int new_socket);
    void update_timeouts(int diff);
    void set_poll_flags();

    void handle_msg(sockaddr* client_addr, socklen_t addr_len);
    void handle_hello(sockaddr *client_addr, socklen_t addr_len);
    void handle_list(sockaddr *client_addr, socklen_t addr_len);
    void handle_get(sockaddr* client_addr, socklen_t addr_len);
    void handle_delete(char* const filename, size_t filename_len);
    void handle_add(sockaddr* client_addr, socklen_t addr_len);

    class wrong_socket_index_ex : std::exception {
      public:
        wrong_socket_index_ex() = default;
        const char* what() const noexcept override;
    };

    class invalid_fail_ex : std::exception {
      public:
        invalid_fail_ex() = default;
        const char* what() const noexcept override;
    };

  public:
    server(std::string ma, unsigned p, uint64_t ms, std::string sf, unsigned t, logger* logger);

    ~server();

    void pass_quit_flag(std::atomic<bool>* flag_ptr);

    void init();

    void run();
};

#endif //SIK_DUZY_SERVER_H
