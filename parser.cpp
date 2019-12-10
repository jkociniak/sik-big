//
// Created by kiwipodptak on 25.05.19.
//

#include <iostream>
#include <regex>
#include "parser.h"
#include "utilities.h"

parser::parse_ex::parse_ex(std::string o, std::string val) {
    error = std::string("the argument ('") + val +
            "') for option '" + o + "' is invalid";
}

const char* parser::parse_ex::what() const noexcept {
    return error.c_str();
}

parser::options_exception::options_exception(bpo::options_description d, std::string msg) {
    desc_ss << d;
    what_string = msg;
}

const char* parser::options_exception::what() const noexcept {
    return what_string.c_str();
}

std::stringstream& parser::options_exception::get_desc() {
    return desc_ss;
}

void check_port(unsigned p) {
    if (p > 65535 || p < 1024) {
        throw parser::parse_ex("cmd_port", std::to_string(p));
    }
}

void check_timeout(unsigned t) {
    if (t == 0 || t > 300) {
        throw parser::parse_ex("timeout", std::to_string(t));
    }
}

void check_mcast_addr(std::string addr) {
    if (!std::regex_match(addr, std::regex("^[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}$")))
        throw parser::parse_ex("mcast_addr", addr);

    //first octet
    size_t start = 0;
    size_t dot = 0;
    while (addr[dot] != '.')
        dot++;

    std::string num = addr.substr(start, dot);
    if (std::stoi(num) < 224 || std::stoi(num) > 239)
        throw parser::parse_ex("mcast_addr", addr);

    //second octet
    dot++;
    start = dot;

    while (addr[dot] != '.')
        dot++;

    num = addr.substr(start, dot);
    if (std::stoi(num) < 0 || std::stoi(num) > 255)
        throw parser::parse_ex("mcast_addr", addr);

    //third octet
    dot++;
    start = dot;

    while (addr[dot] != '.')
        dot++;

    num = addr.substr(start, dot);
    if (std::stoi(num) < 0 || std::stoi(num) > 255)
        throw parser::parse_ex("mcast_addr", addr);

    //fourth octet
    start = dot+1;

    num = addr.substr(start);
    if (std::stoi(num) < 0 || std::stoi(num) > 255)
        throw parser::parse_ex("mcast_addr", addr);
}

bpo::variables_map parser::parse_options(int argc, char* argv[]) {
    bpo::options_description desc("Allowed parameters");

    desc.add_options()
            ("mcast_addr,g", bpo::value<std::string>()->required()->notifier(check_mcast_addr),
             ": set multicast address (required)")
            ("cmd_port,p",
             bpo::value<unsigned>()->required()->notifier(check_port),
             ": set port (required)")
            ("max_space,b",
             bpo::value<uint64_t>()->default_value(DEFAULT_MAX_SPACE),
             ": set maximal shared storage size in bytes (optional, default 50 megabytes)")
            ("shrd_fldr,f", bpo::value<std::string>()->required(),
             ": set storage path (required)")
            ("timeout,t", bpo::value<unsigned>()->default_value(
                    DEFAULT_TIMEOUT)->notifier(check_timeout),
             ": set connection timeout in seconds (optional, default 5 seconds, max 300 seconds)");

    bpo::variables_map vm;

    try {
        bpo::store(bpo::parse_command_line(argc, argv, desc), vm);
        bpo::notify(vm);
    } catch (std::exception& e) {
        throw options_exception(desc, e.what());
    }

    return vm;
}
