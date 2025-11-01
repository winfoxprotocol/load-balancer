#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

using namespace std;

static string trim(const string &str)
{
  size_t first = str.find_first_not_of(" \t\n\r\"");
  if (first == string::npos)
    return "";
  size_t last = str.find_last_not_of(" \t\n\r\",\"");
  return str.substr(first, last - first + 1);
}

static string extract_string_value(const string &line)
{
  size_t colon = line.find(':');
  if (colon == string::npos)
    return "";
  string value = line.substr(colon + 1);
  return trim(value);
}

static int extract_int_value(const string &line)
{
  string value = extract_string_value(line);
  try
  {
    return stoi(value);
  }
  catch (...)
  {
    throw runtime_error("Invalid integer value in config: " + line);
  }
}

Config parse_config(const string &filename)
{
  ifstream file(filename);
  if (!file.is_open())
  {
    throw runtime_error("Cannot open config file: " + filename);
  }

  Config config;
  string line;
  bool found_ip = false, found_port = false, found_server_threads = false, found_client_threads = false;

  while (getline(file, line))
  {
    line = trim(line);

    if (line.find("server_ip") != string::npos)
    {
      config.server_ip = extract_string_value(line);
      found_ip = true;
    }
    else if (line.find("server_port") != string::npos)
    {
      config.server_port = extract_int_value(line);
      found_port = true;
    }
    else if (line.find("server_threads") != string::npos)
    {
      config.server_threads = extract_int_value(line);
      found_server_threads = true;
    }
    else if (line.find("client_threads") != string::npos)
    {
      config.client_threads = extract_int_value(line);
      found_client_threads = true;
    }
  }

  if (!found_ip || !found_port || !found_server_threads || !found_client_threads)
  {
    string missing = "Missing required fields in config.json:";
    if (!found_ip)
      missing += " server_ip";
    if (!found_port)
      missing += " server_port";
    if (!found_server_threads)
      missing += " server_threads";
    if (!found_client_threads)
      missing += " client_threads";
    throw runtime_error(missing);
  }

  if (config.server_port < 1024 || config.server_port > 65535)
  {
    throw runtime_error("server_port must be between 1024 and 65535");
  }
  if (config.server_threads < 1 || config.server_threads > 100)
  {
    throw runtime_error("server_threads must be between 1 and 100");
  }
  if (config.client_threads < 1 || config.client_threads > 1000)
  {
    throw runtime_error("client_threads must be between 1 and 1000");
  }

  return config;
}