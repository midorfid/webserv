#pragma once

#include <vector>
#include <string>
#include <map>

class Server;
class HttpRequest;

class Environment {
    public:

        Environment(const HttpRequest &req);
        ~Environment();

        Environment &operator=(const Environment &other);
        Environment(const Environment &other);

        void build();
  
        char * const*   getEnvp() const;

        static void init_env(char **envp);

        static const size_t     dfl_size = 17;
        static const size_t     static_size = 2;
        char const              *static_env[static_size] = {
			"GATEWAY_INTERFACE=CGI/1.1", "SERVER_SOFTWARE=webserv/1.0"};


    private:
        friend int main(int, char **, char **);

        const HttpRequest           &_req;
        // const Location				&_loc;
        std::vector<std::string>    _vsenv;
        char                        **_cenv;

        static char                 **_parent_env;
        static size_t               _parent_env_size;
        
        void            append(const std::string &key, const std::string &val);
        void            append(const std::map<std::string, std::string> &);
        std::string     env_str(const std::string &key, const std::string &val);
        static char            trans_char(char c);

        void            _build_cenv();

};
