//
// Created by hoeflich on 30.07.2025.
//

#ifndef NIMBLEREADEVENTARGS_H
#define NIMBLEREADEVENTARGS_H

#include <cstring>

#include "buffer.h"

class NimBLEReadEventArgs{
public:
    void overwriteReturnValue(const uint8_t* value, int size) {
        data = new buffer(size);
        memcpy(data->getPointer(), value, size);
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

#endif //NIMBLEREADEVENTARGS_H
