#include "utils.h"
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <iostream>

using namespace std;

long long get_current_time_ns()
{
  auto now = chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  return chrono::duration_cast<chrono::nanoseconds>(duration).count();
}

double ns_to_ms(long long ns)
{
  return static_cast<double>(ns) / 1000000.0;
}

bool read_file_lines(const string &filename, vector<string> &lines)
{
  ifstream file(filename);
  if (!file.is_open())
  {
    cerr << "Error: Cannot open file " << filename << endl;
    return false;
  }

  lines.clear();
  string line;
  while (getline(file, line))
  {
    lines.push_back(line);
  }

  file.close();
  return true;
}

bool write_file_lines(const string &filename, const vector<string> &lines)
{
  ofstream file(filename);
  if (!file.is_open())
  {
    cerr << "Error: Cannot create file " << filename << endl;
    return false;
  }

  for (const auto &line : lines)
  {
    file << line << "\n";
  }

  file.close();
  return true;
}

size_t get_file_size(const vector<string> &lines)
{
  size_t size = 0;
  for (const auto &line : lines)
  {
    size += line.length() + 1;
  }
  return size;
}

bool is_directory(const string &path)
{
  struct stat statbuf;
  if (stat(path.c_str(), &statbuf) != 0)
  {
    return false;
  }
  return S_ISDIR(statbuf.st_mode);
}

bool list_files(const string &dir_path, vector<string> &files)
{
  DIR *dir = opendir(dir_path.c_str());
  if (!dir)
  {
    return false;
  }

  files.clear();
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr)
  {
    string name = entry->d_name;
    if (name != "." && name != "..")
    {
      string full_path = dir_path + "/" + name;
      if (!is_directory(full_path))
      {
        files.push_back(full_path);
      }
    }
  }

  closedir(dir);
  return true;
}

string get_filename(const string &path)
{
  size_t pos = path.find_last_of("/\\");
  if (pos == string::npos)
  {
    return path;
  }
  return path.substr(pos + 1);
}