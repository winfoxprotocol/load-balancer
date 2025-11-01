#include "protocol.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>

using namespace std;

bool send_line(int sockfd, const string &message)
{
  string msg = message + "\n";
  ssize_t total_sent = 0;
  ssize_t len = msg.length();

  while (total_sent < len)
  {
    ssize_t sent = send(sockfd, msg.c_str() + total_sent, len - total_sent, 0);
    if (sent <= 0)
    {
      return false;
    }
    total_sent += sent;
  }
  return true;
}

bool recv_line(int sockfd, string &line)
{
  line.clear();
  char c;

  while (true)
  {
    ssize_t received = recv(sockfd, &c, 1, 0);
    if (received <= 0)
    {
      return false;
    }
    if (c == '\n')
    {
      break;
    }
    line += c;
  }
  return true;
}

bool send_file(int sockfd, const vector<string> &lines, int packet_size)
{
  size_t i = 0;
  while (i < lines.size())
  {
    string packet;
    size_t end = min(i + packet_size, lines.size());

    for (size_t j = i; j < end; ++j)
    {
      packet += lines[j] + "\n";
    }

    ssize_t total_sent = 0;
    ssize_t len = packet.length();

    while (total_sent < len)
    {
      ssize_t sent = send(sockfd, packet.c_str() + total_sent, len - total_sent, 0);
      if (sent <= 0)
      {
        return false;
      }
      total_sent += sent;
    }

    i = end;
  }

  return send_line(sockfd, PROTOCOL_END);
}

bool recv_file(int sockfd, size_t size, vector<string> &lines)
{
  lines.clear();
  size_t received = 0;

  while (received < size)
  {
    string line;
    if (!recv_line(sockfd, line))
    {
      return false;
    }

    if (line == PROTOCOL_END)
    {
      break;
    }

    lines.push_back(line);
    received += line.length() + 1;
  }

  return true;
}

bool parse_request(int sockfd, Request &request)
{
  string command;
  if (!recv_line(sockfd, command))
  {
    return false;
  }

  istringstream iss(command);
  string cmd, filename;
  iss >> cmd >> filename;

  if (cmd == PROTOCOL_PUT)
  {
    request.type = RequestType::PUT;
    request.filename = filename;

    string size_line;
    if (!recv_line(sockfd, size_line))
    {
      return false;
    }

    istringstream size_iss(size_line);
    string size_cmd;
    size_iss >> size_cmd >> request.file_size;

    if (size_cmd != PROTOCOL_SIZE)
    {
      return false;
    }

    if (!recv_file(sockfd, request.file_size, request.file_lines))
    {
      return false;
    }

    return true;
  }
  else if (cmd == PROTOCOL_GET)
  {
    request.type = RequestType::GET;
    request.filename = filename;
    return true;
  }

  return false;
}