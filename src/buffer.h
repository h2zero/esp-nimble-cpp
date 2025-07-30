//
// Created by hoeflich on 30.07.2025.
//
#include <sys/types.h>

#ifndef BUFFER_H
#define BUFFER_H

class buffer {
  public:

  u_int8_t* getPointer() {
    return data;
  }

  size_t getSize() {
    return size;
  }

  explicit buffer(size_t size){
    if(size == 0) {
      this->data = nullptr;
      this->size = 0;
      return;
    }

    this->data = new uint8_t[size];
    this->size = size;
  }

  ~buffer(){
    delete data;
  }

private:
  uint8_t *data;
  size_t size;
};

#endif //BUFFER_H
