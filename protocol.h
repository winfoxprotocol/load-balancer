#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <vector>

using namespace std;

const string PROTOCOL_PUT = "PUT";
const string PROTOCOL_GET = "GET";
const string PROTOCOL_OK = "OK";
const string PROTOCOL_ERROR = "ERROR";
const string PROTOCOL_SIZE = "SIZE";
const string PROTOCOL_END = "END";

const string PROTOCOL_HEALTH = "HEALTH";
const string PROTOCOL_HEALTH_OK = "HEALTH_OK";

enum class RequestType {
    PUT,
    GET,
    UNKNOWN
};

struct Request {
    RequestType type;
    string filename;
    size_t file_size;
    vector<string> file_lines;
    int client_id;

    long long arrival_time;
    long long start_time;
    long long finish_time;

    size_t lines_processed = 0;

    Request() : type(RequestType::UNKNOWN), file_size(0), client_id(0),
                arrival_time(0), start_time(0), finish_time(0) {}
};

bool send_line(int sockfd, const string& message);

bool recv_line(int sockfd, string& line);

bool send_file(int sockfd, const vector<string>& lines, int packet_size);

bool recv_file(int sockfd, size_t size, vector<string>& lines);

bool parse_request(int sockfd, Request& request);

#endif