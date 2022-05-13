#ifndef CPPTORRENT_SOCKET_H
#define CPPTORRENT_SOCKET_H

#include <boost/asio.hpp>

#include "Message.h"
#include "NetExceptions.h"

#include <memory>
#include <string>
#include <utility>

using Data = bittorrent::Message;
using PeerData = bittorrent::PeerMessage;

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
        using ReadCallback = std::function<void(const Data &)>;
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
            Post([this, self = shared_from_this()] {
                cancel_operation();
            });
        }

    protected:
        void stop_await() { timeout_.cancel(); }

        void do_resolve(std::string const &host, std::string const &port, ConnectCallback connect_callback, ErrorCallback error_callback) {
            resolver_.async_resolve(host, port, [=, this, self = shared_from_this()](error_code ec, endpoint_iter_type endpoints) {
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
                socket_, std::move(endpoints), [=, this, self = shared_from_this()](error_code ec, endpoint_iter_type) {
                    stop_await();
                    if (!ec) {
                        connect_callback();
                    } else {
                        error_callback(ec);
                    }
                });
        }

        void cancel_operation(){
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
        boost::asio::streambuf buff_;
    };

    using StrandEx = asio::executor;

    struct TCPSocket : Impl<asio::ip::tcp, StrandEx> {
        using base_type::base_type;

        void Send(const Data& msg, WriteCallback write_callback, ErrorCallback error_callback) {
            Post([=, this, self = shared_from_this()] {
                async_write(socket_, msg.GetBuf().data(), [self, write_callback, error_callback](error_code ec, size_t xfr) {
                    if (!ec) {
                        write_callback(xfr);
                    } else {
                        error_callback(ec);
                    }
                });
            });
        }

        void Read(size_t size, ReadCallback read_callback, ErrorCallback error_callback) {
            Post([=, this, self = shared_from_this()] {
                async_read(
                    socket_, asio::buffer(buff_.prepare(size)), [this, self, read_callback, error_callback](error_code ec, size_t length) {
                        stop_await();
                        if (!ec) {
                            read_callback(bittorrent::Message(&buff_));
                        } else {
                            error_callback(ec);
                        }
                        buff_.consume(length);
                    });
            });
        }

        void ReadUntil(std::string until_str, ReadCallback read_callback, ErrorCallback error_callback) {
            Post([=, this, self = shared_from_this()] {
                async_read_until(socket_, buff_, until_str, [self, this, read_callback, error_callback](error_code ec, size_t xfr) {
                    stop_await();
                    if (!ec) {
                        read_callback(bittorrent::Message(&buff_));
                    } else {
                        error_callback(ec);
                    }
                    buff_.consume(xfr);
                });
            });
        }

        void ReadToEof(ReadCallback read_callback, ReadCallback eof_callback, ErrorCallback error_callback) {
            Post([=, this, self = shared_from_this()] {
                async_read(
                    socket_, buff_, [this, self, read_callback, eof_callback, error_callback](error_code ec, size_t length) {
                        stop_await();
                        if (!ec) {
                            read_callback(bittorrent::Message(&buff_));
                        } else if (ec == boost::asio::error::eof) {
                            eof_callback(bittorrent::Message(&buff_));
                        } else {
                            error_callback(ec);
                        }
                        buff_.consume(length);
                    });
            });
        }
    };

    struct UDPSocket : Impl<asio::ip::udp, StrandEx> {
        using base_type::base_type;

        void Send(const Data& msg, WriteCallback write_callback, ErrorCallback error_callback) {
            Post([=, this, self = shared_from_this()] {
                socket_.async_send_to(
                    msg.GetBuf().data(), *endpoint_iter_, [=](error_code ec, size_t xfr) {
                        if (!ec) {
                            write_callback(xfr);
                        } else {
                            error_callback(ec);
                        }
                    });
            });
        }

        void Read(size_t max_size, ReadCallback read_callback, ErrorCallback error_callback) {
            Post([=, this, self = shared_from_this()] {
                sender_ = *endpoint_iter_;
                socket_.async_receive_from(
                    boost::asio::buffer(buff_.prepare(max_size)), sender_, [max_size, this, self, read_callback, error_callback](error_code ec, size_t xfr) {
                        this->stop_await();
                        buff_.commit(xfr);
                        if (!ec) {
                            std::cerr << buff_.size() << std::endl;
                            read_callback(bittorrent::Message(&buff_));
                        } else {
                            error_callback(ec);
                        }
                        buff_.consume(xfr);
                    });
            });
        }

        void Promote() {
            if (++endpoint_iter_ == boost::asio::ip::udp::resolver::iterator())
                is_endpoints_dried = true;
        }

        bool IsEndpointsDried () const {
            return is_endpoints_dried;
        }

    protected:
        asio::ip::udp::endpoint sender_;
        bool is_endpoints_dried = false;
    };
} // namespace network

#endif // CPPTORRENT_SOCKET_H
