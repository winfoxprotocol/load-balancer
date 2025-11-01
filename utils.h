#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <chrono>

using namespace std;

long long get_current_time_ns();

double ns_to_ms(long long ns);

bool read_file_lines(const string& filename, vector<string>& lines);

bool write_file_lines(const string& filename, const vector<string>& lines);

size_t get_file_size(const vector<string>& lines);

bool list_files(const string& dir_path, vector<string>& files);

bool is_directory(const string& path);

string get_filename(const string& path);

#endif