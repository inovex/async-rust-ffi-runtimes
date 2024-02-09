#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>

#include <iostream>

#include "Runtime.hpp"

extern "C" {

struct FfiDataHolder;

typedef void (*FfiDataHolderDropFn)(const FfiDataHolder *);

struct FfiDataHolder {
    const std::uint8_t *ptr;
    std::size_t len;
    FfiDataHolderDropFn drop;
};

struct FfiLib;

} // extern "C"

namespace mylib {

class DataHolderBase : public ::FfiDataHolder {
public:
    DataHolderBase(const std::uint8_t *ptr, std::size_t len)
        : ::FfiDataHolder {
            .ptr = ptr,
            .len = len,
            .drop = &DataHolderBase::free,
        } {}

    virtual ~DataHolderBase();

private:
    static void free(const ::FfiDataHolder *self) {
        std::cerr << "DataHolderBase::free" << std::endl;
        const DataHolderBase *p = static_cast<const DataHolderBase *>(self);
        delete p;
    }
};

class DataAccess {
public:
    virtual ~DataAccess();

    virtual ::FfiFuture<::FfiDataHolder*> get_data() = 0;
};

class Lib {
public:
    Lib(std::unique_ptr<DataAccess> data_access);
    ~Lib();

    // postcode is checked by the library to keep the API surface smaller
    asyncrt::Future<bool> should_run(std::uint32_t postcode);

private:
    ::FfiLib *m_mylib;
};

} // namespace mylib
