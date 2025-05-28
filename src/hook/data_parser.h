#ifndef DAILYSTEPS_DATA_PARSER_H
#define DAILYSTEPS_DATA_PARSER_H

#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

struct sensor_data {
    int type; 
    float data[16]; 
};

class data_parser {
private:
    std::vector<sensor_data> config; 
    std::mutex config_mutex; 
    std::string config_path;
    int inotify_fd; 
    int watch_fd; 
    std::thread watch_thread; 
    std::atomic<bool> running; 

    bool init_inotify();
    bool parse_config_file();
    void cleanup();
    void watch_file_changes();

public:
    data_parser(const std::string& path);
    ~data_parser();

    std::vector<sensor_data> get_config();
};

#endif