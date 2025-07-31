//
// Created by hoeflich on 30.07.2025.
//

#ifndef NIMBLEEVENTS_H
#define NIMBLEEVENTS_H

#if CONFIG_BT_ENABLED && (CONFIG_BT_NIMBLE_ROLE_PERIPHERAL || CONFIG_BT_NIMBLE_ROLE_CENTRAL)
#include "buffer.h"
#include <cstring>
#include <string>

class NimBLEReadEventArgs{
public:
    void overwriteReturnValue(const uint8_t* value, int size) {
        data = new buffer(size);
        std::memcpy(data->getPointer(), value, size);
    }

    bool isDataOverwritten() const { return data != nullptr; }

    buffer* getData() { return data; }

    NimBLEReadEventArgs() {}

    ~NimBLEReadEventArgs() {
        if(data != nullptr)
            delete data;
    }
private:
    buffer* data = nullptr;
};

class NimBLEWriteEventArgs{
public:
    void Cancel(){
        canceled = true;
    }

    bool isCanceled(){
        return canceled;
    }

    std::string GetOldData(){
        return oldData;
    }

    std::string GetNewData(){
        return newData;
    }

    NimBLEWriteEventArgs(std::string oldData, std::string newData) {
        this->oldData = oldData;
        this->newData = newData;
    }

    ~NimBLEWriteEventArgs() {
    }
private:
    std::string oldData;
    std::string newData;
    bool canceled = false;
};

#endif
#endif //NIMBLEEVENTS_H
