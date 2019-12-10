//
// Created by kiwipodptak on 25.05.19.
//

#include <iostream>
#include <boost/program_options.hpp>
#include <string>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstdint>
#include <sys/stat.h>
#include <dirent.h>
#include <chrono>
#include <fcntl.h>

#include "server.h"
#include "msg_handler.h"

const char* server::wrong_socket_index_ex::what() const noexcept {
    return "Wrong socket index!";
}

const char* server::invalid_fail_ex::what() const noexcept {
    return "Fail on invalid socket!";
}

server::server(std::string ma, unsigned p, uint64_t ms, std::string sf,
               unsigned t, logger* logger) : handler(SERVER) {
    mcast_addr = std::move(ma);
    port = p;
    max_space = ms;
    shrd_fldr = std::move(sf);
    timeout = t;
    this->_logger = logger;
}

server::~server() {
    for (pollfd& socket_info : sockets)
        if (socket_info.fd >= 0)
            close(socket_info.fd);
}

void server::pass_quit_flag(std::atomic<bool>* flag_ptr) {
    quit_flag = flag_ptr;
}

void server::init() {
    prepare_file_list();

    char multicast_dotted_address[mcast_addr.length() + 1];
    strcpy(multicast_dotted_address, mcast_addr.c_str());

    in_port_t local_port = (in_port_t)port;

    sockaddr_in local_address;

    int comm_socket;
    comm_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (comm_socket < 0) {
        syserr("socket");
        throw syserr_ex("socket");
    }

    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(multicast_dotted_address, &ip_mreq.imr_multiaddr) == 0) {
        syserr("inet_aton");
        throw syserr_ex("inet_aton");
    }

    if (setsockopt(comm_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0) {
        syserr("setsockopt");
        throw syserr_ex("setsockopt");
    }

    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(local_port);
    if (bind(comm_socket, (sockaddr *)&local_address, sizeof local_address) < 0) {
        syserr("bind");
        throw syserr_ex("bind");
    }

    add_udp_socket(comm_socket);
}

void server::add_tcp_socket(int socket, std::string fpath, off_t fsize, int timeout, MSG_CMD type) {
    pollfd socket_info{socket, POLLIN, 0};

    sockets.push_back(socket_info);
    tcp_socket_data tsd = tcp_socket_data(socket, std::move(fpath), fsize, timeout, type);
    tcp_sockets_data.insert(std::pair<int, tcp_socket_data>(socket, tsd));
    tcp_sockets_data.at(socket).init();
}

void server::add_udp_socket(int socket) {
    pollfd socket_info{socket, POLLIN, 0};
    sockets.push_back(socket_info);
}

void server::run(){
    while(true) {
        fit_sockets_vector();
        set_poll_flags();
        std::pair<int, int> min_timeout = get_min_timeout(); //pair (index, timeout)
        _logger->log(std::string("min_timeout: ") + std::to_string(min_timeout.first) + ", " + std::to_string(min_timeout.second));

        auto start = std::chrono::system_clock::now();
        int changes = poll(sockets.data(), sockets.size(), min_timeout.second);
        auto end = std::chrono::system_clock::now();
        std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        long diff_ms = diff.count();
        _logger->log(std::string("poll duration: ") + std::to_string(diff_ms));

        if (changes < 0) {
            if (errno == EINTR) {
                errno = 0;
                return;
            }

            throw syserr_ex("poll");
        }

        if (changes == 0) {
            if (min_timeout.first > 0) {
                _logger->log(std::string("timeout expired, deleting socket ") + std::to_string(min_timeout.first));
                delete_socket(min_timeout.first);
                update_timeouts(min_timeout.second);
            }
        } else {
            //handle udp
            if (sockets[0].revents & POLLIN) {
                try {
                    handle_conn_socket();
                } catch (syserr_ex& e) {}
            }

            //handle tcp
            for (int i = 1; i < sockets.size(); i++)
                handle_tcp_socket(i, diff_ms);
        }

        if ((*quit_flag).load())
            return;
    }
}

void server::fit_sockets_vector() {
    while (sockets.back().fd < 0)
        sockets.pop_back();
}

void server::set_poll_flags() {
    sockets[0].events = POLLIN;
    sockets[0].revents = 0;

    for (int i = 1; i < sockets.size(); i++) {
        if (sockets[i].fd > 0) {
            if (tcp_sockets_data.at(sockets[i].fd).state == WAITING) {
                sockets[i].events = POLLIN;
                sockets[i].revents = 0;
            } else if (tcp_sockets_data.at(sockets[i].fd).state == CONNECTED) {
                if (tcp_sockets_data.at(sockets[i].fd).type == ADD) {
                    sockets[i].events = POLLIN;
                    sockets[i].revents = 0;
                } else { //GET
                    sockets[i].events = POLLOUT;
                    sockets[i].revents = 0;
                }
            }
        }
    }
}

void server::delete_socket(int socket) {
    if (socket >= 0 && socket < sockets.size()) {
        if (sockets[socket].fd < 0)
            throw invalid_fail_ex();

        tcp_sockets_data.at(sockets[socket].fd).close_sockets();
        tcp_sockets_data.at(sockets[socket].fd).clean();
        tcp_sockets_data.erase(sockets[socket].fd);
        sockets[socket].fd = -1;
    } else {
        throw wrong_socket_index_ex();
    }
}

void server::update_timeouts(int diff) {
    for (int i = 1; i < sockets.size(); i++) {
        if (sockets[i].fd > 0) {
            if (tcp_sockets_data.at(sockets[i].fd).state == WAITING) {
                if (tcp_sockets_data.at(sockets[i].fd).timeout > diff) {
                    tcp_sockets_data.at(sockets[i].fd).timeout -= diff;
                } else {
                    _logger->log(std::string("timeout expired, deleting socket ") + std::to_string(i));
                    delete_socket(i);
                }
            }
        }
    }
}

void server::handle_conn_socket() {
    ssize_t rcv_len;

    sockaddr_in client_address;
    socklen_t rcva_len = (socklen_t) sizeof(client_address);

    int comm_socket = sockets[0].fd;
    /* read msg */
    rcv_len = recvfrom(comm_socket, comm_buffer,
                       MAX_RCV_PACKET_SIZE, 0,
                       (sockaddr *) &client_address, &rcva_len);
    if (rcv_len < 0) {
        throw syserr_ex("read");
    } else {
        try {
            handler.decode(comm_buffer, rcv_len);
            handle_msg((sockaddr *) &client_address, rcva_len);
        } catch (msg_handler::invalid_packet_ex& e) {
            std::cerr
                    << "[PCKG ERROR] Skipping invalid package from "
                    <<
                    inet_ntoa(client_address.sin_addr) << ":" <<
                    client_address.sin_port << "." << e.what();
        }
    }
}

void server::handle_tcp_socket(int index, int diff) {
    if (index < 1 || index >= sockets.size())
        throw wrong_socket_index_ex();

    if (sockets[index].fd < 0)
        return;

    if (sockets[index].revents & POLLERR) {
        _logger->log(std::string("error at socket " + std::to_string(index) + ", deleting it and its paired socket"));
        delete_socket(index);
    }
    // 1.czeka na polaczenie
    //   a) jesli ktos chce to accept_conn
    //   b) jesli nie zbijamy diffa
    // 2.jest polaczony
    //   a) potrzebuje POLLIN
    //   b) potrzebuje POLLOUT

    int fd = sockets[index].fd;

    if (tcp_sockets_data.at(fd).state == SET) {
        tcp_sockets_data.at(fd).begin_waiting();
        _logger->log(std::string("set socket ") + std::to_string(index) + " to waiting");
    } else if (tcp_sockets_data.at(fd).state == WAITING) {
        if (sockets[index].revents & POLLIN) {
            int msg_sock = tcp_sockets_data.at(fd).accept_conn();

            if (msg_sock > 0) { //accept succeeded
                replace_socket(index, msg_sock);
                _logger->log(
                        std::string("put in socket ") + std::to_string(index) +
                        " fd " + std::to_string(msg_sock));
            } else {
               _logger->log( std::string("accepting connection in socket ") + std::to_string(index) +
                " failed");
            }
        } else if (tcp_sockets_data.at(fd).timeout > diff) {
            tcp_sockets_data.at(fd).timeout -= diff;
            _logger->log(std::string("reduced timeout of socket ") + std::to_string(index));
        } else {
            _logger->log(std::string("timeout expired, deleting socket ") + std::to_string(index) + " and its paired socket");
            delete_socket(index);
        }
    } else if (tcp_sockets_data.at(fd).state == FAILED) {
        if (tcp_sockets_data.at(fd).type == ADD) {
            off_t filesize = tcp_sockets_data.at(fd).get_file_info().second;
            free_space += filesize;
        }

        _logger->log(std::string("transferring failed, deleting socket ") + std::to_string(index) + " and its paired socket");
        delete_socket(index);
    } else {
        if (sockets[index].revents & POLLHUP) {
            tcp_sockets_data.at(fd).state = FAILED;
        } else if ((tcp_sockets_data.at(fd).type == GET && (sockets[index].revents & POLLOUT)) ||
            (tcp_sockets_data.at(fd).type == ADD && (sockets[index].revents & POLLIN))) {
            tcp_sockets_data.at(fd).transfer();

            if (tcp_sockets_data.at(fd).state == SUCCEED) {
                _logger->log(std::string("finished transferring file, deleting socket ") + std::to_string(fd) + " and its paired socket");
                if (tcp_sockets_data.at(fd).type == ADD)
                    files.push_back(tcp_sockets_data.at(fd).get_file_info()); //add file info
                delete_socket(index);
            }
        }
    }
}

void server::replace_socket(int old_socket_index, int new_socket) {
    tcp_sockets_data.insert(std::pair<int, tcp_socket_data>(new_socket, tcp_sockets_data.at(sockets[old_socket_index].fd)));
    tcp_sockets_data.erase(sockets[old_socket_index].fd);
    sockets[old_socket_index].fd = new_socket;
}

/* handle proper message (need to check validity earlier)
 * -----------possible commands info------------
 * from client:
 * "HELLO", SIMPL, empty data
 * "LIST", SIMPL, empty or nonempty data
 * "GET", SIMPL, nonempty data
 * "DEL", SIMPL, nonempty data
 * "ADD", CMPLX, nonempty data
 * */
void server::handle_msg(sockaddr* client_addr, socklen_t addr_len) {
    if (handler.cmd == HELLO)
        handle_hello(client_addr, addr_len);
    else if (handler.cmd == LIST)
        handle_list(client_addr, addr_len);
    else if (handler.cmd == GET)
        handle_get(client_addr, addr_len);
    else if (handler.cmd == DEL)
        handle_delete(handler.data, handler.data_len);
    else //ADD
        handle_add(client_addr, addr_len);
}

void server::prepare_file_list() {
    files.clear();
    free_space = max_space;

    struct dirent* file;
    DIR* path;

    path = opendir(shrd_fldr.c_str());
    if (!path) {
        syserr("opendir");
        throw syserr_ex("opendir");
    }

    errno = 0;
    file = readdir(path);
    if (!file && errno != 0) {
        syserr("readdir");
        throw syserr_ex("readdir");
    }

    while (file) {
        if (std::string(file->d_name).size() > MAX_CHUNK_SIZE)
            continue;

        std::string file_path(shrd_fldr);
        file_path.push_back('/');
        file_path += file->d_name;
        struct stat file_info;
        lstat(file_path.c_str(), &file_info);

        if (S_ISREG(file_info.st_mode)) {
            files.emplace_back(std::string(file->d_name), file_info.st_size);

            if (file_info.st_size > free_space)
                free_space = 0;
            else
                free_space -= file_info.st_size;
        }

        file = readdir(path);

        if (!file && errno != 0) {
            syserr("readdir");
            throw syserr_ex("readdir");
        }
    }

    if (closedir(path) < 0) {
        syserr("closedir");
        throw syserr_ex("closedir");
    }
}

void server::handle_hello(sockaddr *client_addr, socklen_t addr_len) {
    int comm_socket = sockets[0].fd;

    size_t msg_len = sizeof(CMPLX_CMD) + mcast_addr.size();
    handler.encode(comm_buffer, "GOOD_DAY", handler.cmd_seq,
                                    free_space, mcast_addr, CMPLX);

    if (sendto(comm_socket, comm_buffer, msg_len, 0, client_addr, addr_len) < 0)
        throw syserr_ex("sendto");
}

void server::handle_list(sockaddr *client_addr,
                         socklen_t addr_len) {
    std::vector<std::string> filtered_files;

    if (handler.data_len == 0) {
        for (std::pair<std::string, off_t> file_info : files)
            filtered_files.push_back(file_info.first);
    } else {
        std::string seq;

        for (int i = 0; i < handler.data_len; i++)
            seq.push_back(handler.data[i]);

        for (std::pair<std::string, off_t> file_info : files) {
            if (file_info.first.find(seq) != std::string::npos)
                filtered_files.push_back(file_info.first);
        }
    }

    if (!filtered_files.empty()) {
        std::vector<std::string> my_lists; //in case list is too large to send in one packet
        my_lists.emplace_back();

        for (const std::string& s : filtered_files) { //very nonoptimal data division
            if (s.size() + my_lists.back().size() > MAX_CHUNK_SIZE) {
                if (my_lists.back().back() == '\n')
                    my_lists.back().pop_back();

                my_lists.emplace_back();
            }


            my_lists.back().append(s);
            my_lists.back().push_back('\n');
        }

        if (my_lists.back().back() == '\n')
            my_lists.back().pop_back();

        int comm_socket = sockets[0].fd;

        for (const std::string& list : my_lists) {
            size_t msg_len = sizeof(SIMPL_CMD) + list.size();
            handler.encode(comm_buffer, "MY_LIST", handler.cmd_seq, 0, list, SIMPL);

            if (sendto(comm_socket, comm_buffer, msg_len, 0, client_addr, addr_len) < 0)
                throw syserr_ex("sendto");
        }
    }
}

void server::handle_get(sockaddr* client_addr, socklen_t addr_len) {
    std::string filename_str;

    for (int i = 0; i < handler.data_len; i++)
        filename_str.push_back(handler.data[i]);

    bool found = false;
    int i;
    for (i = 0; i < files.size(); i++) {
        if (files[i].first == filename_str) {
            found = true;
            break;
        }
    }

    if (!found)
        throw msg_handler::invalid_packet_ex("there is no such file specified in packet");

    sockaddr_in server_address;

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        throw syserr_ex("socket");

    if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0)
        throw syserr_ex("socket");

    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = 0; // listening on port PORT_NUM

    // bind the socket to a concrete address
    if (bind(sock, (sockaddr*) &server_address, sizeof(server_address)) < 0)
        throw syserr_ex("bind");

    socklen_t addrlen = sizeof(server_address);
    if (getsockname(sock, (sockaddr*)&server_address, &addrlen) < 0)
        throw syserr_ex("getsockname");

    if (listen(sock, DEFAULT_QUEUE_LENGTH) < 0)
        throw syserr_ex("listen");

    add_tcp_socket(sock, shrd_fldr + "/" + files[i].first, files[i].second, timeout*1000, GET);

    int comm_socket = sockets[0].fd;

    size_t msg_len = sizeof(CMPLX_CMD) + filename_str.size();
    handler.encode(comm_buffer, "CONNECT_ME", handler.cmd_seq,
                   ntohs(server_address.sin_port), filename_str, CMPLX);

    if (sendto(comm_socket, comm_buffer, msg_len, 0, client_addr, addr_len) < 0)
        throw syserr_ex("sendto");
}

void server::handle_delete(char* const filename, size_t filename_len) {
    std::string filename_str;

    for (int i = 0; i < filename_len; i++)
        filename_str.push_back(filename[i]);

    bool found = false;
    for (const std::pair<std::string, off_t>& file_info : files) {
        if (file_info.first == filename_str) {
            found = true;
            break;
        }
    }

    if (found) {
        std::string path = shrd_fldr + "/" + filename_str;

        if (remove(path.c_str()) < 0)
            throw syserr_ex("remove");

        for (auto it = files.begin(); it != files.end(); ++it) { //delete file from file list
            if (it->first == filename_str) {
                free_space += it->second;
                files.erase(it);
                break;
            }
        }
    }
}

void server::handle_add(sockaddr *client_addr, socklen_t addr_len) {
    int comm_socket = sockets[0].fd;
    bool can_add = true;

    std::string filename;
    for (int i = 0; i < handler.data_len; i++)
        filename.push_back(handler.data[i]);

    if (filename.empty() || (filename.find('/') != std::string::npos) || free_space < handler.param)
        can_add = false;

    if (can_add) {
        for (const std::pair<std::string, off_t>& file_info : files) {
            if (file_info.first == filename) {
                can_add = false;
                break;
            }
        }
    }

    if (!can_add) {
        size_t msg_len = sizeof(SIMPL_CMD) + handler.data_len;
        handler.encode(comm_buffer, "NO_WAY", handler.cmd_seq, 0, filename, SIMPL);

        if (sendto(comm_socket, comm_buffer, msg_len, 0, client_addr, addr_len) < 0)
            throw syserr_ex("sendto");

        return;
    }

    free_space -= handler.param; //reserve space for file

    sockaddr_in server_address;

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        throw syserr_ex("socket");

    if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0)
        throw syserr_ex("socket");

    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = 0; // listening on port PORT_NUM

    // bind the socket to a concrete address
    if (bind(sock, (sockaddr*) &server_address, sizeof(server_address)) < 0)
        throw syserr_ex("bind");

    socklen_t addrlen = sizeof(server_address);
    if (getsockname(sock, (sockaddr*)&server_address, &addrlen) < 0)
        throw syserr_ex("getsockname");

    if (listen(sock, DEFAULT_QUEUE_LENGTH) < 0)
        throw syserr_ex("listen");

    add_tcp_socket(sock, shrd_fldr + "/" + filename, handler.param, timeout*1000, ADD);

    size_t msg_len = sizeof(CMPLX_CMD);
    handler.encode(comm_buffer, "CAN_ADD", handler.cmd_seq,
                   ntohs(server_address.sin_port), std::string(), CMPLX);

    if (sendto(comm_socket, comm_buffer, msg_len, 0, client_addr, addr_len) < 0)
        throw syserr_ex("sendto");
}

std::pair<int, int> server::get_min_timeout() {
    int index = -1;
    int min_timeout = -1;
    int i;

    for (i = 1; i < sockets.size(); i++) {
        int fd = sockets[i].fd;

        if (fd > 0) {
            if (tcp_sockets_data.at(fd).state == WAITING) {
                index = i;
                min_timeout = tcp_sockets_data.at(fd).timeout;
                break;
            }
        }
    }

    i++;

    for (; i < sockets.size(); i++) {
        int fd = sockets[i].fd;

        if (fd > 0) {
            if (tcp_sockets_data.at(fd).state == WAITING && tcp_sockets_data.at(fd).timeout < min_timeout) {
                index = i;
                min_timeout = tcp_sockets_data.at(fd).timeout;
            }
        }
    }

    return std::pair<int, int>(index, min_timeout);
}