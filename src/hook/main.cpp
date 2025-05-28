#include <cstring>
#include <cstdio>
#include <jni.h>
#include <fcntl.h>
#include <cerrno>
#include <fstream>
#include <string>
#include "elf_util.h"
#include "utils.h"
#include "data_parser.h"

#define LIB_PATH "/system/lib64/libsensor.so"
#define CONFIG_PATH "/data/misc/yuuki/sensor_config"
#define FUNC_SIGN_PATH "/data/misc/yuuki/func_sign"

typedef ssize_t (*OriginalBitTubeSendObjects)(void* tube, void const* events, size_t count, size_t objSize);
OriginalBitTubeSendObjects OriginalBitTubeSendObjectsFunc = nullptr;
void* bitTubeSendObjectsAddr = nullptr;

data_parser* parser = nullptr;

ssize_t hookedBitTubeSendObjects(void* tube, void const* events, size_t count, size_t objSize) {
    std::vector<sensor_data> config = parser ? parser->get_config() : std::vector<sensor_data>();

    const char* event_ptr = reinterpret_cast<const char*>(events);
    for (size_t i = 0; i < count; ++i) {
        const char* current_event = event_ptr + i * objSize;
        int event_type = *(int32_t*)(current_event + 8);

        for (const auto& sensor : config) {
            if (sensor.type == event_type) {
                float* p_float_data = (float*)(current_event + 24);
                LOGD("Modifying ASensorEvent->data[16] for event %zu, type %d", i, event_type);
                for (int j = 0; j < 16; ++j) {
                    p_float_data[j] = sensor.data[j];
                }
                break; 
            }
        }
    }

    if (OriginalBitTubeSendObjectsFunc) {
        return OriginalBitTubeSendObjectsFunc(tube, events, count, objSize);
    }
    LOGE("OriginalBitTubeSendObjectsFunc = nullptr");
    return -1;
}

int doBitTubeSendObjectsHook(const char* func_sign) {
    SandHook::ElfImg sensorService(LIB_PATH);

    if (!sensorService.isValid()) {
        LOGE("Failed to load %s", LIB_PATH);
        return -1;
    }

    bitTubeSendObjectsAddr = sensorService.getSymbolAddress<void*>(func_sign);

    if (!bitTubeSendObjectsAddr) {
        LOGE("Failed to calculate BitTube::sendObjects address");
        return -1;
    }
    LOGD("BitTube::sendObjects found at %p", bitTubeSendObjectsAddr);

    OriginalBitTubeSendObjectsFunc = (OriginalBitTubeSendObjects)InlineHook(bitTubeSendObjectsAddr, (void*)hookedBitTubeSendObjects);
    if (OriginalBitTubeSendObjectsFunc == nullptr) {
        LOGE("Failed to hook BitTube::sendObjects");
        return -1;
    } else {
        LOGD("Success to hook BitTube::sendObjects");
    }

    return 0;
}

int doUnBitTubeSendObjectsHook() {
    int result = -1;

    if (OriginalBitTubeSendObjectsFunc != nullptr) {
        result = DobbyDestroy(bitTubeSendObjectsAddr);
        if (result == 0) {
            LOGD("Success to unhook BitTube::sendObjects");
            OriginalBitTubeSendObjectsFunc = nullptr;
            bitTubeSendObjectsAddr = nullptr;
        } else {
            LOGE("Failed to unhook BitTube::sendObjects");
        }
    }

    return result;
}

int createFile(char* path) {
    int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0644);

    if (fd == -1) {
        if (errno == EEXIST) {
            LOGW("File already exists: %s", path);
            return 0;  // 不算错误，已存在
        } else {
            LOGE("Failed to create file: %s", strerror(errno));
            return -1;
        }
    }

    close(fd);
    LOGD("File created successfully: %s", path);
    return 0;
}

std::string readFuncSign() {
    std::ifstream file(FUNC_SIGN_PATH);
    if (!file.is_open()) {
        LOGE("Failed to open func_sign file: %s", FUNC_SIGN_PATH);
        return "";
    }

    std::string func_sign;
    std::getline(file, func_sign);
    file.close();

    if (func_sign.empty()) {
        LOGE("Function signature is empty in file: %s", FUNC_SIGN_PATH);
        return "";
    }

    LOGD("Successfully read function signature: %s", func_sign.c_str());
    return func_sign;
}

__attribute__((constructor))
void initialize_hook() {
    LOGD("constructor called for BitTube::sendObjects hook");
    createFile(CONFIG_PATH);
    parser = new data_parser(CONFIG_PATH); // 初始化 parser

    std::string func_sign = readFuncSign();
    if (!func_sign.empty()) {
        doBitTubeSendObjectsHook(func_sign.c_str());
    } else {
        LOGE("Failed to read function signature, skipping hook");
    }
}

__attribute__((destructor))
void cleanup_hook() {
    LOGD("Cleaning up hook resources");
    doUnBitTubeSendObjectsHook();
    if (parser) {
        delete parser;
        parser = nullptr;
    }
}