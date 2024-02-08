#include "Runtime.hpp"

#include "mylib.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

class StringDataHolder : public mylib::DataHolderBase {
public:
    StringDataHolder(std::string data)
        : mylib::DataHolderBase{nullptr, 0}
        , m_data{std::move(data)} {
            ptr = reinterpret_cast<std::uint8_t const*>(m_data.c_str());
            len = m_data.length();
        }

    ~StringDataHolder() override = default;

private:
    std::string m_data;
};

class MockDataAccess : public mylib::DataAccess {
public:
    ~MockDataAccess() override = default;

    virtual ::FfiFuture/* <DataHolder> */ get_data() override {
        return asyncrt::make_future([]() {
                auto *p = new StringDataHolder{R"({"state":1})"};
                return static_cast<::FfiDataHolder*>(p);
            });
    }
};

int main() {
    try {
        boost::asio::io_context ioCtx{};
        asyncrt::Executor executor{ioCtx};

        auto data_access = std::make_unique<MockDataAccess>();

        auto lib = mylib::Lib{std::move(data_access)};

        ioCtx.dispatch([&executor, &lib]() {
            auto future = lib.should_run();
            executor.await(std::move(future));
        });

        ioCtx.run();
        std::cout.flush();
    } catch (std::exception const& err) {
        std::cerr << "fatal exception: " << err.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}
