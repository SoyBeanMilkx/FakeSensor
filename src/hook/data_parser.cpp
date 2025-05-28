#include "data_parser.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <sys/inotify.h>
#include <unistd.h>
#include "utils.h"

data_parser::data_parser(const std::string& path)
        : config_path(path), inotify_fd(-1), watch_fd(-1), running(false) {
    parse_config_file();
    if (init_inotify()) {
        running = true;
        watch_thread = std::thread(&data_parser::watch_file_changes, this);
    }
}

data_parser::~data_parser() {
    cleanup();
}

bool data_parser::init_inotify() {
    inotify_fd = inotify_init();
    if (inotify_fd < 0) {
        LOGE("Failed to initialize inotify: %s", strerror(errno));
        return false;
    }

    watch_fd = inotify_add_watch(inotify_fd, config_path.c_str(), IN_MODIFY);
    if (watch_fd < 0) {
        LOGE("Failed to add watch for %s: %s", config_path.c_str(), strerror(errno));
        close(inotify_fd);
        inotify_fd = -1;
        return false;
    }

    LOGD("Successfully initialized inotify for %s", config_path.c_str());
    return true;
}

void data_parser::cleanup() {
    running = false;
    if (watch_thread.joinable()) {
        watch_thread.join();
    }
    if (watch_fd >= 0) {
        inotify_rm_watch(inotify_fd, watch_fd);
    }
    if (inotify_fd >= 0) {
        close(inotify_fd);
    }
}

bool data_parser::parse_config_file() {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        LOGE("Failed to open config file: %s", config_path.c_str());
        return false;
    }

    std::vector<sensor_data> new_config;
    std::string line;
    int line_number = 0;

    while (std::getline(file, line)) {
        line_number++;
        if (line.empty()) continue;

        std::istringstream iss(line);
        sensor_data data;
        data.type = line_number;
        for (int i = 0; i < 16; ++i) {
            if (!(iss >> data.data[i])) {
                LOGW("Invalid data format at line %d, setting remaining to 0", line_number);
                while (i < 16) {
                    data.data[i++] = 0.0f;
                }
                break;
            }
        }
        new_config.push_back(data);
    }

    file.close();

    {
        std::lock_guard<std::mutex> lock(config_mutex);
        config = std::move(new_config);
    }

    LOGD("Successfully parsed config file with %zu entries", config.size());
    return true;
}

void data_parser::watch_file_changes() {
    char buffer[1024];
    while (running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(inotify_fd, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ret = select(inotify_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (ret < 0) {
            LOGE("Select error: %s", strerror(errno));
            break;
        }
        if (ret == 0) {
            continue; 
        }

        int len = read(inotify_fd, buffer, sizeof(buffer));
        if (len < 0) {
            LOGE("Failed to read inotify events: %s", strerror(errno));
            break;
        }

        for (char* ptr = buffer; ptr < buffer + len;) {
            struct inotify_event* event = (struct inotify_event*)ptr;
            if (event->mask & IN_MODIFY) {
                LOGD("Config file modified, reloading...");
                parse_config_file();
            }
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
}

std::vector<sensor_data> data_parser::get_config() {
    std::lock_guard<std::mutex> lock(config_mutex);
    return config;
}