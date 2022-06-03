#ifndef CPPTORRENT_SOCKET_H
#define CPPTORRENT_SOCKET_H

#include <boost/asio.hpp>

#include "Message.h"
#include "NetExceptions.h"

#include <sstream>

#include <list>
#include <memory>
#include <string>
#include <utility>

using SendAnyData = bittorrent::SendingMessage;          // the most based sending message class
using ReceiveAnyData = bittorrent::ReceivingPeerMessage; // the most inherited receiving message class

namespace network {
    namespace asio = boost::asio;
    using boost::system::error_code;

    template <typename Protocol, typename Executor> struct Impl : public std::enable_shared_from_this<Impl<Protocol, Executor>> {
    public:
        using base_type = Impl<Protocol, Executor>;

        static constexpr bool is_datagram = std::is_same_v<Protocol, asio::ip::udp>;
        using socket_type =
            std::conditional_t<is_datagram, asio::basic_datagram_socket<Protocol, Executor>, asio::basic_stream_socket<Protocol, Executor>>;
        using resolver_type = asio::ip::basic_resolver<Protocol, Executor>;
        using endpoint_iter_type = typename resolver_type::iterator;

        using std::enable_shared_from_this<Impl>::shared_from_this;

        using ConnectCallback = std::function<void()>;
        using PromoteCallback = std::function<void()>;
        using WriteCallback = std::function<void(size_t)>;
        using ReadCallback = std::function<void(const ReceiveAnyData &)>;
        using ErrorCallback = std::function<void(boost::system::error_code ec)>;

        explicit Impl(Executor executor) : resolver_(executor), socket_(executor), timeout_(executor) {
#ifdef OS_WIN
            SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
#endif
        }

        explicit Impl(socket_type sock) : resolver_(sock.get_executor()), socket_(std::move(sock)), timeout_(socket_.get_executor()) {
#ifdef OS_WIN
            SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
#endif
        }

        template <typename Token> decltype(auto) Post(Token &&callback) {
            return asio::post(socket_.get_executor(), std::forward<Token>(callback));
        }

        void Connect(std::string Host, std::string Port, const ConnectCallback &connect_callback, const ErrorCallback &error_callback) {
            Post([=, self = shared_from_this()] { self->do_resolve(Host, Port, connect_callback, error_callback); });
        }

        void Await(boost::posix_time::time_duration ms) {
            Post([this, self = shared_from_this(), ms] {
                stop_await();
                timeout_.expires_from_now(ms);
                timeout_.template async_wait([this, self](error_code ec) {
                    if (!ec) {
                        cancel_operation();
                    }
                });
            });
        }

        template <typename Handler> void Await(boost::posix_time::time_duration ms, Handler f) {
            Post([this, self = shared_from_this(), ms, f] {
                stop_await();
                timeout_.expires_from_now(ms);
                timeout_.template async_wait([this, self, f = std::move(f)](error_code ec) {
                    if (!ec) {
                        cancel_operation();
                        asio::dispatch(std::move(f));
                    }
                });
            });
        }

        void Cancel() {
            Post([this, self = shared_from_this()] { cancel_operation(); });
        }

        template <typename Derived> inline std::shared_ptr<Derived> shared_from(Derived *that) {
            return std::static_pointer_cast<Derived>(shared_from_this());
        }

    protected:
        void stop_await() { timeout_.cancel(); }

        void do_resolve(std::string const &host, std::string const &port, ConnectCallback connect_callback, ErrorCallback error_callback) {
            resolver_.async_resolve(host, port, [=, self = shared_from_this()](error_code ec, endpoint_iter_type endpoints) {
                if (!ec) {
                    endpoint_iter_ = std::move(endpoints);
                    if constexpr (is_datagram) {
                        stop_await();
                        socket_.open(endpoint_iter_->endpoint().protocol());
                        connect_callback();
                    } else {
                        do_connect(endpoint_iter_, connect_callback, error_callback);
                    }
                } else {
                    error_callback(ec);
                }
            });
        }

        void do_connect(endpoint_iter_type endpoints, ConnectCallback connect_callback, ErrorCallback error_callback) {
            async_connect( //
                socket_, std::move(endpoints), [=, self = shared_from_this()](error_code ec, endpoint_iter_type) {
                    stop_await();
                    if (!ec) {
                        connect_callback();
                    } else {
                        error_callback(ec);
                    }
                });
        }

        void cancel_operation() {
            timeout_.cancel();
            resolver_.cancel();
            if (socket_.is_open()) {
                socket_.cancel();
            }
        }

        resolver_type resolver_;
        endpoint_iter_type endpoint_iter_;
        socket_type socket_;
        asio::deadline_timer timeout_;

        boost::asio::streambuf read_streambuff_;

        std::mutex queue_mut_;
        std::list<SendAnyData> queue_send_buff_;
    };

    using StrandEx = asio::executor;

    struct TCPSocket : Impl<asio::ip::tcp, StrandEx> {
        using base_type::base_type;

        void Send(SendAnyData msg, WriteCallback write_callback, ErrorCallback error_callback) {
            std::unique_lock lock(queue_mut_);
            auto it = queue_send_buff_.insert(queue_send_buff_.end(), std::move(msg));
            lock.unlock();

            Post([=, self = shared_from_this()] {
                async_write(socket_, asio::buffer(it->GetBufferData()), [=, self = self](error_code ec, size_t xfr) {
                    std::unique_lock lock(queue_mut_);
                    queue_send_buff_.erase(it);
                    lock.unlock();

                    if (!ec) {
                        write_callback(xfr);
                    } else {
                        error_callback(ec);
                    }
                });
            });
        }

        void Read(size_t size, ReadCallback read_callback, ErrorCallback error_callback) {
            Post([=, self = shared_from_this()] {
                async_read(socket_, asio::buffer(read_streambuff_.prepare(size)),
                    [this, self, read_callback, error_callback](error_code ec, size_t length) {
                        stop_await();
                        read_streambuff_.commit(length);
                        if (!ec) {
                            auto data = asio::buffer_cast<const uint8_t *>(read_streambuff_.data());
                            read_callback({data, read_streambuff_.size()});
                        } else {
                            error_callback(ec);
                        }
                        read_streambuff_.consume(length);
                    });
            });
        }

        void ReadUntil(std::string until_str, ReadCallback read_callback, ErrorCallback error_callback) {
            Post([=, self = shared_from_this()] {
                async_read_until(
                    socket_, read_streambuff_, until_str, [self, this, read_callback, error_callback](error_code ec, size_t xfr) {
                        stop_await();
                        if (!ec) {
                            auto data = asio::buffer_cast<const uint8_t *>(read_streambuff_.data());
                            read_callback({data, read_streambuff_.size()});
                        } else {
                            error_callback(ec);
                        }
                        read_streambuff_.consume(xfr);
                    });
            });
        }

        void ReadToEof(ReadCallback read_callback, ReadCallback eof_callback, ErrorCallback error_callback) {
            Post([=, self = shared_from_this()] {
                async_read(
                    socket_, read_streambuff_, [this, self, read_callback, eof_callback, error_callback](error_code ec, size_t length) {
                        stop_await();
                        if (!ec) {
                            auto data = asio::buffer_cast<const uint8_t *>(read_streambuff_.data());
                            read_callback({data, read_streambuff_.size()});
                        } else if (ec == boost::asio::error::eof) {
                            auto data = asio::buffer_cast<const uint8_t *>(read_streambuff_.data());
                            eof_callback({data, read_streambuff_.size()});
                        } else {
                            error_callback(ec);
                        }
                        read_streambuff_.consume(length);
                    });
            });
        }
    };

    struct UDPSocket : Impl<asio::ip::udp, StrandEx> {
        using base_type::base_type;

        void Send(SendAnyData msg, WriteCallback write_callback, ErrorCallback error_callback) {
            std::unique_lock lock(queue_mut_);
            auto it = queue_send_buff_.insert(queue_send_buff_.end(), std::move(msg));
            lock.unlock();

            Post([=, self = shared_from_this()] {
                auto endpoint_ptr = std::make_shared<asio::ip::udp::endpoint>(*endpoint_iter_);
                socket_.async_send_to(asio::buffer(it->GetBufferData()), *endpoint_ptr, [=, self = self](error_code ec, size_t xfr) {
                    std::unique_lock lock(queue_mut_);
                    queue_send_buff_.erase(it);
                    lock.unlock();

                    if (!ec) {
                        write_callback(xfr);
                    } else {
                        error_callback(ec);
                    }
                });
            });
        }

        void Read(size_t max_size, ReadCallback read_callback, ErrorCallback error_callback) {
            Post([=, self = shared_from_this()] {
                auto endpoint_ptr = std::make_shared<asio::ip::udp::endpoint>(*endpoint_iter_);
                socket_.async_receive_from(boost::asio::buffer(read_streambuff_.prepare(max_size)), *endpoint_ptr,
                    [this, endpoint_ptr, self, read_callback, error_callback](error_code ec, size_t xfr) {
                        this->stop_await();
                        read_streambuff_.commit(xfr);
                        if (!ec) {
                            auto data = asio::buffer_cast<const uint8_t *>(read_streambuff_.data());
                            read_callback({data, read_streambuff_.size()});
                        } else {
                            error_callback(ec);
                        }
                        read_streambuff_.consume(xfr);
                    });
            });
        }

        void Promote() {
            if (++endpoint_iter_ == boost::asio::ip::udp::resolver::iterator()) is_endpoints_dried = true;
        }

        bool IsEndpointsDried() const { return is_endpoints_dried; }

    protected:
        bool is_endpoints_dried = false;
    };
} // namespace network

#endif // CPPTORRENT_SOCKET_H