#include "server.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <vector>


void Server::Run() {
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(8080);

  if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("bind failed");
    return;
  }

  if (listen(server_fd, SOMAXCONN) < 0) {
    perror("listen failed");
    return;
  }

  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    perror("epoll_create1");
    return;
  }

  epoll_event ev = {0};
  ev.events = EPOLLIN;
  ev.data.fd = server_fd;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
    perror("epoll_ctl (server_fd)");
    return;
  }

  std::cout << "Server listening on port 8080\n";

  std::vector<epoll_event> events(1024);
  while (true) {
    int n = epoll_wait(epoll_fd, events.data(), events.size(), -1);
    for (int i = 0;i < n;++i) {
      int fd = events[i].data.fd;
      if (fd == server_fd) {
        handle_new_connection(epoll_fd);
      } else {
        handle_client_event(epoll_fd, fd);
      }
    }
  }
}

void Server::handle_new_connection(int epoll_fd) {
  sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  int client_fd = accept4(server_fd, (struct sockaddr*)&client_addr, &client_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (client_fd < 0) {
    perror("accepted failed");
    return;
  }

  epoll_event client_ev{};
  client_ev.events = EPOLLIN | EPOLLET;
  client_ev.data.fd = client_fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);

  auto userPtr = std::make_shared<User>(client_fd);
  {
    std::lock_guard<std::mutex> lock(mut);
    m1[client_fd] = userPtr;
  }
  std::cout << "Client " << client_fd << " connected\n";
}

void Server::handle_client_event(int epoll_fd, int client_fd) {
  std::vector<char> buffer(1024);
  ssize_t valread = read(client_fd, buffer.data(), sizeof(buffer));
  if (valread <= 0) {
    remove_client(epoll_fd, client_fd);
    return;
  }
  if (valread < buffer.size()) buffer[valread] = '\0';
  std::string s(buffer.data(), valread);

  //std::lock_guard<std::mutex> lock(mut);
  auto it = m1.find(client_fd);
  if (it != m1.end()) {
    auto& user = (it->second);
    if (user->username.empty()) {
      user->username = s;
    }
    std::cout << "[" << client_fd << "]" << s;
    if (s == "list\n") {
      list_users(client_fd);
    } else if (s == "quit\n") {
      remove_client(epoll_fd, client_fd);
      return;
    } else {
      handle_client_event(client_fd, s);
    }
  }
}

void Server::handle_client_event(int client_fd, const std::string& message) {
  std::istringstream iss(message);
  std::string cmd, key, value;
  iss >> cmd >> key >> value;
  if (cmd == "SET") {
    store_.set(std::stoi(key), value);
  } else if (cmd == "GET") {
    std::string res = store_.get(std::stoi(key));
    res += "\n";
    if (!res.empty()) {
      send(client_fd, res.data(), res.size(), 0);
    } else {
      send(client_fd, "NOT FOUND\n", 10, 0);
    }
  } else if (cmd == "KEYS") {
    if (key == "*") {
      std::string res = store_.get_all();
      send(client_fd, res.data(), res.size(), 0);
    }
  }
}

void Server::remove_client(int epoll_fd, int client_fd) {
  close(client_fd);
  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
  {
    std::lock_guard<std::mutex> lock(mut);
    m1.erase(client_fd);
  }
  std::cout << "Client " << client_fd << " disconnected\n";
}


void Server::list_users(int client_fd) {
  std::string user_list = "User List:\n";
  {
    for (const auto& pair : m1) {
      user_list += std::to_string(pair.first) + " " + pair.second->username;
    }
  }
  send(client_fd, user_list.data(), user_list.size(), 0);
}
