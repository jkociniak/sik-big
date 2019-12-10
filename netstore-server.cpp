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
#include <csignal>

#include "parser.h"
#include "server.h"

std::atomic<bool> quit(false);
void got_signal(int) {
    quit.store(true);
}

int main(int argc, char* argv[]) {
    struct sigaction sa;
    memset( &sa, 0, sizeof(sa) );
    sa.sa_handler = got_signal;
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT,&sa,NULL);

    /* parse args to variables map*/
    parser p;
    bpo::variables_map vm;

    try {
        vm = p.parse_options(argc, argv);
    } catch (parser::options_exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        std::cerr << e.get_desc().rdbuf() << std::endl;
        return 1;
    }

    /* extract args from map */
    std::string mcast_addr = vm["mcast_addr"].as<std::string>();
    unsigned port = vm["cmd_port"].as<unsigned>();
    uint64_t max_space = vm["max_space"].as<uint64_t>();
    std::string shrd_fldr = vm["shrd_fldr"].as<std::string>();
    unsigned timeout = vm["timeout"].as<unsigned>();

    /* initialize logger and server */
    logger l = logger();
    //l.enable_print_msgs();

    server srv = server(mcast_addr, port, max_space, shrd_fldr, timeout, &l);
    srv.pass_quit_flag(&quit);
    try {
        srv.init();
    } catch (syserr_ex& e) {
        return 1;
    }

    srv.run();

    return 0;
}

