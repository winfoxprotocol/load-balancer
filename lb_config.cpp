#include "lb_config.h"
#include <fstream>
#include <sstream>
#include <iostream>

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

LBConfig parse_lb_config(const string &filename)
{
    ifstream file(filename);
    if (!file.is_open())
    {
        throw runtime_error("Cannot open config file: " + filename);
    }

    LBConfig config;
    string line;
    bool found_lb_ip = false, found_lb_port = false;
    vector<string> backend_ips(4);
    vector<int> backend_ports(4);
    vector<bool> found_backends(4, false);

    while (getline(file, line))
    {
        line = trim(line);

        if (line.find("lb_ip") != string::npos)
        {
            config.lb_ip = extract_string_value(line);
            found_lb_ip = true;
        }
        else if (line.find("lb_port") != string::npos)
        {
            config.lb_port = extract_int_value(line);
            found_lb_port = true;
        }
        else
        {
            for (int i = 1; i <= 4; ++i)
            {
                string server_ip_key = "server" + to_string(i) + "_ip";
                string server_port_key = "server" + to_string(i) + "_port";

                if (line.find(server_ip_key) != string::npos)
                {
                    backend_ips[i - 1] = extract_string_value(line);
                    found_backends[i - 1] = true;
                }
                else if (line.find(server_port_key) != string::npos)
                {
                    backend_ports[i - 1] = extract_int_value(line);
                }
            }
        }
    }

    if (!found_lb_ip || !found_lb_port)
    {
        throw runtime_error("Missing lb_ip or lb_port in config");
    }

    for (int i = 0; i < 4; ++i)
    {
        if (!found_backends[i])
        {
            throw runtime_error("Missing server" + to_string(i + 1) + " configuration");
        }
        config.backends.emplace_back(i + 1, backend_ips[i], backend_ports[i]);
    }

    if (config.lb_port < 1024 || config.lb_port > 65535)
    {
        throw runtime_error("lb_port must be between 1024 and 65535");
    }

    return config;
}