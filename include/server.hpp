#pragma once

#include "Client.hpp"
#include "Config.hpp"
#include <sys/epoll.h>
#include "ParseConfig.hpp"
#include "RequestHandler.hpp"
#include "log.hpp"
#include "RouteRequest.hpp"
#include <memory>

class Server {
    public:

        Server();
        ~Server();

        Server(const Server &other) = delete;
        Server &operator=(const Server &other) = delete;

        void    run(const std::string &cfg_file);

        std::map<int, std::unique_ptr<Client>>::iterator disconnect_client(std::map<int, std::unique_ptr<Client>>::iterator &it, int client_fd);
        void    		        				disconnect_client(int client_fd);
        std::pair<std::string, std::string>		getClientAddr(struct sockaddr_storage &client_addr);

        bool                                    epoll_add_cgi(std::pair<int, int> cgi_fds, int client_fd);
        const Config                            &getConfig() const; // returns first vhost
        const Config                            &selectVhost(int port, const std::string &host) const;
        static void                             handle_signal(int signum);

    private:

        int                                     _epoll_fd;
        std::map<int, std::unique_ptr<Client>>  _clients;

        // Virtual hosting: one Config per server{} block
        std::vector<Config>                     _vhosts;
        // port → indices into _vhosts (first entry = default for that port)
        std::map<int, std::vector<size_t>>      _port_to_vhost_idx;
        // listen fd → port
        std::map<int, int>                      _sock_to_port;

        ParseConfig				                _ConfigParser;
		RouteRequest	                        _route_reslvr;
        RequestHandler                          _handler;
        std::map<int, int>                      _cgi_client;

        static int                              _signal_write_fd;

        int                                     _signal_read_fd;

        void            cleanup();
        void            terminateConnWithError(int client_fd, int error_code);
        void            handleDefault(Client &client, int client_fd);
        void            handle_cgi_write(int pipe_fd);
        void            handle_cgi_read(int &pipe_fd);
        std::string		generate_response(Client &client);
        void    		init_epoll(epoll_event *ev);
        int     		bind_port(int port); // binds port, returns listen fd
        void    		run_event_loop(epoll_event *ev);
        void    		hints_init(struct addrinfo *hints);
        void    		handle_new_connection();
        void    		handle_client_event(int client_fd);

        void            handleClientTimeout(int client_fd);
        double          diffTime(const time_t &client_tm);
        void            checkTimeouts();
        void            disconnectCgiFds(Client &client);
        void            parseAndQoutputBuf(Client &client, int client_fd);
        void            startChunkedCgiStream(Client &client, int client_fd);
};
