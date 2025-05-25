#include <iostream>
#include <memory>
#include <map>
#include <mutex>
#include <string>
#include "store.h"

struct User {
  int client_fd;
  std::string username;

  User(int fd) : client_fd(fd), username("") {}
};

class Server {
public:
  void Run();
private:
  void handle_new_connection(int epoll_fd);
  void handle_client_event(int epoll_fd, int client_fd);
  void remove_client(int epoll_fd, int client_fd);
  void list_users(int client_fd);
  void handle_client_event(int client_fd, const std::string& message);

  
  int server_fd;
  std::map<int, std::shared_ptr<User>> m1;
  std::map<int, std::string> store;  
  std::mutex mut;
  Store<int, std::string> store_;
};
