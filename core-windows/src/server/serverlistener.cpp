#include <winsock2.h>
#include "serverlistener.h"
#include <iostream>
#include <regex>
#include <cstdlib>
#include <string>
#include <list>
#include <future>
#include <chrono>
#include <thread>
#include <sstream>
#include <fstream>
#include "requestparser.h"
#include "router.h"
#include "lib/json.hpp"
#include "settings.h"
#define DEFAULT_BUFLEN 1452


ServerListener::ServerListener(size_t buffer_size) {
    this->buffer_size = buffer_size;
    this->server_running = false;
    this->listen_socket = INVALID_SOCKET;

    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw ServerStartupException();
    }
}

std::string ServerListener::init() {
    int port = 0;
    json options = settings::getConfig();
    if(!options["port"].is_null())
        port = options["port"];
    string mode = settings::getMode();

    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listen_socket == INVALID_SOCKET) {
        throw SocketCreationException(WSAGetLastError());
    }
    struct sockaddr_in servAddr;
    memset( & servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    if(mode == "cloud")
        servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servAddr.sin_port = htons(port);

    if(::bind(listen_socket, (struct sockaddr*)(&servAddr), sizeof(servAddr)) == SOCKET_ERROR) {
        closesocket(listen_socket);
        throw SocketBindingException(WSAGetLastError());
    }

    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(this->listen_socket, (struct sockaddr *)&sin, &len) == -1) {
        perror("getsockname");
    }
    else {
        port = ntohs(sin.sin_port);
        settings::setPort(port);
    }
    string navigationUrl = "http://localhost:" + std::to_string(port);
    if(!options["url"].is_null()) {
        string url = options["url"];
        if (regex_match(url, regex("^/.*")))
            navigationUrl += url;
        else
            navigationUrl = url;
    }

    if(listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listen_socket);
        throw ListenException(WSAGetLastError());
    }
    return navigationUrl;
}

void ServerListener::run(std::function<void(ClientAcceptationException)> client_acceptation_error_callback) {
    std::map<SOCKET, std::thread> threads;
    bool server_running = true;
    while(server_running) {
        SOCKET client_socket;
        try {
            client_socket = accept(listen_socket, NULL, NULL);
            if(client_socket == INVALID_SOCKET) {
                throw ClientAcceptationException(WSAGetLastError());
            }
        } catch(ClientAcceptationException &e) {
            client_acceptation_error_callback(e);
            continue;
        }
        threads[client_socket] = std::thread(ServerListener::clientHandler, client_socket, buffer_size);
        threads[client_socket].detach();
    }
}

void ServerListener::stop() {
    server_running = false;
    if(listen_socket != INVALID_SOCKET) {
        shutdown(listen_socket, SD_BOTH);
        closesocket(listen_socket);
    }
}

void ServerListener::clientHandler(SOCKET client_socket, size_t buffer_size) {
    int recvbuflen = buffer_size;
    char *recvbuf = new char[recvbuflen];
    int bytes_received;
    RequestParser parser;

    sockaddr_in client_info;
    int client_info_len = sizeof(sockaddr_in);
    char *client_ip;

    if(getpeername(client_socket, (sockaddr*)(&client_info), &client_info_len) == SOCKET_ERROR) {
        goto cleanup;
    }
    client_ip = inet_ntoa(client_info.sin_addr);

    while(1) {
        parser.reset();

        bool headers_ready = false;
        while(!parser.isParsingDone()) {
            bytes_received = recv(client_socket, recvbuf, recvbuflen, 0);
            if(bytes_received > 0) {
                parser.processChunk(recvbuf, bytes_received);
                if(parser.allHeadersAvailable()) {
                    headers_ready = true;
                }
            } else {
                goto cleanup;
            }
        }
        delete[] recvbuf;
        std::string response_body = "";
        pair<string, string> responseGen =  routes::handle(parser.getPath(), parser.getBody(), parser.getHeader("Authorization"));
        response_body = responseGen.first;

        std::string response_headers = "HTTP/1.1 200 OK\r\n"
        "Content-Type: " + responseGen.second + "\r\n"
        "Connection: close\r\n"
        "Content-Length: " + std::to_string(response_body.size()) + "\r\n\r\n";

        std::string response = response_headers + response_body;

        char sndbuf[DEFAULT_BUFLEN];
        int sndbuflen = DEFAULT_BUFLEN;
        int iResult = 0;
        int count = 0;
        int len = 0;
        int responseLen = response.length();
        while(count < responseLen) {
            len = min(responseLen - count, sndbuflen);
            memcpy(sndbuf, response.data() + count, len);
            // Sends a buffer
            iResult = send(client_socket, sndbuf, len, 0);
            if (iResult != SOCKET_ERROR) {
                if(iResult > 0)
                    count += iResult;
                else
                    break;
            }
        }
    }
cleanup:
    closesocket(client_socket);
}

ServerListener::~ServerListener() {
    WSACleanup();
}
