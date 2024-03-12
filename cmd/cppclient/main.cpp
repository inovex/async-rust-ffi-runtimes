#include "AsyncFuture.hpp"
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
        // This does not follow the Rust semantics of Future::poll(), the Boost.Asio semantics.
        auto promise = asyncrt::Promise<std::string>{};
        auto future = promise.get_future();
        // TODO: use the actual URL
        http::get(m_io_context, "api.stromgedacht.de", "/v1/now?zip=76137",
                  [promise = std::move(promise)](std::string const& result) mutable {
                      std::cout << "+++ [C] resolving promise: " << result << std::endl;
                      promise.set_value(result);
                  });
        return asyncrt::make_cpp_future<::FfiDataHolder*>([future = std::move(future)](
                                                              ::FfiContext* context) mutable {
            std::cout << "+++ [C] cpp future callback             " << " with context "
                      << reinterpret_cast<void*>(context) << ", waker "
                      << reinterpret_cast<void const*>(context->waker) << ", vtable "
                      << reinterpret_cast<void const*>(context->waker->vtable) << ", wake func "
                      << reinterpret_cast<void const*>(context->waker->vtable->wake) << std::endl;
            if (future.is_ready()) {
                std::cout << "+++ [C] future is ready" << std::endl;
                auto* p = new StringDataHolder{future.value()};
                std::cout << "+++ [C] returning poll status READY" << std::endl;
                return asyncrt::make_poll_status(static_cast<::FfiDataHolder*>(p));
            }
            // this is free'd by the call to wake()
            auto const* waker = context->waker->vtable->clone(context->waker);
            std::cout << "+++ [C] cloned waker " << reinterpret_cast<void const*>(context->waker)
                      << " as " << reinterpret_cast<void const*>(waker) << std::endl;
            future.await([waker]() {
                // This will cause `future.poll_fn()` to be called again, this time the first
                // branch will be taken.
                std::cout << "+++ [C] cpp future await callback       " << " with waker "
                          << reinterpret_cast<void const*>(waker) << ", vtable "
                          << reinterpret_cast<void const*>(waker->vtable) << std::endl;
                std::cout << "+++ [C] wake function "
                          << reinterpret_cast<void const*>(waker->vtable->wake) << std::endl;
                waker->vtable->wake(waker);
                std::cout << "+++ [C] future done" << std::endl;
            });
            std::cout << "+++ [C] returning poll status PENDING" << std::endl;
            return asyncrt::make_poll_status<::FfiDataHolder*>(asyncrt::PollStatus::Pending);
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
