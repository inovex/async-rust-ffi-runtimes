#include "Runtime.hpp"

#include "http.hpp"
#include "mylib.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

#include <boost/asio/io_context.hpp>

namespace asio = boost::asio;

namespace {

class StringDataHolder : public mylib::DataHolderBase {
public:
    StringDataHolder(std::string data)
        : mylib::DataHolderBase{nullptr, 0}, m_data{std::move(data)} {
        ptr = reinterpret_cast<std::uint8_t const*>(m_data.c_str());
        len = m_data.length();
    }

    ~StringDataHolder() override = default;

private:
    std::string m_data;
};

class MockDataAccess : public mylib::DataAccess {
public:
    MockDataAccess(boost::asio::io_context& io_context) : m_io_context{io_context} {}
    ~MockDataAccess() override = default;

    virtual ::FfiFuture<::FfiDataHolder*> get_data() override {
        // TODO: use the actual URL
        http::get(m_io_context, "api.stromgedacht.de", "/v1/now?zip=76137",
                  [](std::string const& result) { std::cout << result << std::endl; });
        return asyncrt::make_future<::FfiDataHolder*>([]() {
            auto* p = new StringDataHolder{R"({"state":1})"};
            return static_cast<::FfiDataHolder*>(p);
        });
    }

private:
    boost::asio::io_context& m_io_context;
};

}  // namespace

int main() {
    try {
        asio::io_context io_context{};
        asyncrt::Executor executor{io_context};

        auto data_access = std::make_unique<MockDataAccess>(io_context);

        auto lib = mylib::Lib{std::move(data_access)};

        io_context.dispatch([&executor, &lib]() {
            auto future = lib.should_run(76137);
            executor.await(std::move(future), [](bool const& result) {
                std::cout << "received " << result << " from mylib" << std::endl;
            });
        });

        io_context.run();
        std::cout.flush();
    } catch (std::exception const& err) {
        std::cerr << "fatal exception: " << err.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}
