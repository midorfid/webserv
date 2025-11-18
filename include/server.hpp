#pragma once

#include "Client.hpp"
#include "Config.hpp"
#include <sys/epoll.h>
#include "ParseConfig.hpp"
#include "RequestHandler.hpp"

class Server {
    public:

        Server();
        ~Server();

        Server(const Server &other);
        Server &operator=(const Server &other);

        void    run(const std::string &cfg_file);

        void    		        				disconnect_client(int client_fd);
        const std::string       				&port() const;
        std::pair<std::string, std::string>		getClientAddr(struct sockaddr_storage &client_addr);

        bool                                    epoll_add_cgi(std::pair<int, int> cgi_fds, int client_fd);

    private:

        std::string                             _port;
        int                                     _listen_sock;
        int                                     _epoll_fd;
        std::map<int, Client>                   _clients;
        Config                                  _config;
        ParseConfig				                _ConfigParser;
        RequestHandler                          _handler;
        std::map<int, std::pair<int, int> >     _client_cgi;
        
        
        std::string		generate_response(Client &client);
        void    		init_epoll(epoll_event *ev);
        void    		init_sockets(const char *port);
        void    		run_event_loop(epoll_event *ev);
        void    		hints_init(struct addrinfo *hints);
        void    		handle_new_connection();
        void    		handle_client_event(int client_fd);
};
