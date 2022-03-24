//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include "PeerClient.h"

#include "auxiliary.h"

#include <utility>

network::PeerClient::PeerClient(const std::shared_ptr<bittorrent::MasterPeer> &master_peer, bittorrent::Peer slave_peer,
    const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor)
    : master_peer_(*master_peer), slave_peer_(std::move(slave_peer)), socket_(executor), resolver_(executor), timeout_(executor)
{
}

network::PeerClient::PeerClient(
    const std::shared_ptr<bittorrent::MasterPeer> &master_peer, ba::ip::tcp::socket socket, uint8_t *handshake_ptr)
    : master_peer_(*master_peer), socket_(std::move(socket)), resolver_(socket_.get_executor()), timeout_(socket_.get_executor())
{
    for (size_t i = 0; i < bittorrent_constants::handshake_length; i++)
    {
        buff[i] = handshake_ptr[i];
    }
}

network::PeerClient::~PeerClient()
{
    LOG(GetStrIP(), " : ", "destruction");

    timeout_.cancel();
    socket_.close();
}

// TODO реализовать полный дисконнект
void network::PeerClient::Disconnect()
{
    if (is_disconnected)
        return;

    LOG(GetStrIP(), " : ", __FUNCTION__);

    timeout_.cancel();
    socket_.cancel();
    master_peer_.Unsubscribe(Get());
    is_disconnected = true;
}

void network::PeerClient::try_again_connect()
{
    if (is_disconnected)
        return;

    auto self = Get();
    post(resolver_.get_executor(),
        [this, self]
        {
            if (--connect_attempts)
            {
                LOG(GetStrIP(), " : attempts ", connect_attempts);
                do_resolve();
                timeout_.async_wait(
                    [this, self](boost::system::error_code const &ec)
                    {
                        if (!ec)
                        {
                            LOG(GetStrIP(), " : ", "deadline timer to make resolve");
                            deadline();
                        }
                    });
            }
            else
                Disconnect();
        });
}

void network::PeerClient::StartConnection()
{
    if (socket_.is_open())
    {
        do_verify();
    }
    else
    {
        do_resolve();
    }
    auto self = Get();
    timeout_.async_wait(
        [this, self](boost::system::error_code const &ec)
        {
            if (!ec)
            {
                LOG(GetStrIP(), " : ", "deadline timer to make resolve");
                Disconnect();
            }
        });
}

void network::PeerClient::do_resolve()
{
    LOG(GetStrIP(), " : ", __FUNCTION__);

    auto self(Get());
    resolver_.async_resolve(GetStrIP(), std::to_string(slave_peer_.GetPort()),
        [this, self](boost::system::error_code ec, ba::ip::tcp::resolver::iterator endpoint)
        {
            if (!ec)
            {
                do_connect(std::move(endpoint));
                LOG(GetStrIP(), " : ", "start timer!");
                timeout_.async_wait(
                    [this, self](boost::system::error_code const &ec)
                    {
                        if (!ec)
                        {
                            LOG(GetStrIP(), " : ", "deadline timer from do_resolve");
                            deadline();
                        }
                    });
            } else {
                Disconnect();
            }
        });
    timeout_.expires_from_now(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
}

void network::PeerClient::do_connect(ba::ip::tcp::resolver::iterator endpoint)
{
    LOG(GetStrIP(), " : ", __FUNCTION__);

    auto self(Get());
    ba::async_connect(socket_, std::move(endpoint),
        [this, self](boost::system::error_code ec, [[maybe_unused]] const ba::ip::tcp::resolver::iterator &)
        {
            timeout_.cancel();
            if (!ec)
            {
                // do handshake
                do_write(std::string(reinterpret_cast<const char *>(master_peer_.GetHandshake()), bittorrent_constants::handshake_length),
                    [this, self]()
                    {
                        ba::async_read(socket_, ba::buffer(buff, 68),
                            [this, self](boost::system::error_code ec, std::size_t bytes_transferred /*length*/)
                            {
                                timeout_.cancel();
                                if ((!ec || ec == ba::error::eof) && bytes_transferred >= 68 && buff[0] == 0x13 &&
                                    memcmp(&buff[1], "BitTorrent protocol", 19) == 0 &&
                                    memcmp(&buff[28], &master_peer_.GetHandshake()[28], 20) == 0)
                                {
                                    std::cerr << GetStrIP() << " was connected!" << std::endl;
                                    read_message([] {});
                                }
                                else
                                {
                                    try_again_connect();
                                }
                            });
                        timeout_.expires_from_now(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
                    });
            }
            else
            {
                LOG(GetStrIP(), " : ", ec.message());
                try_again_connect();
            }
        });
    timeout_.expires_from_now(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
}

void network::PeerClient::do_verify()
{
    LOG(GetStrIP(), " : ", __FUNCTION__);

    if (buff[0] == 0x13 && memcmp(&buff[1], "BitTorrent protocol", 19) == 0 &&
        memcmp(&buff[28], &master_peer_.GetHandshake()[28], 20) == 0)
    {
        auto self(Get());
        do_write(std::string(reinterpret_cast<const char*>(buff), bittorrent_constants::handshake_length),
            [this, self](){

            });
    }
    else
    {
        Disconnect();
    }
}

void network::PeerClient::deadline()
{
    LOG(GetStrIP(), " : ", __FUNCTION__);

    if (timeout_.expires_at() <= ba::deadline_timer::traits_type::now())
    {
        timeout_.cancel();
        socket_.cancel();
    }
    else
    {
        LOG(GetStrIP(), " : ", "start timer!");
        auto self(Get());
        timeout_.async_wait(
            [this, self](boost::system::error_code ec)
            {
                if (!ec)
                {
                    LOG(GetStrIP(), " : ", "deadline timer from deadline!");
                    deadline();
                }
            });
    }
}

std::string network::PeerClient::GetStrIP() const
{
    if (!cash_ip_.empty())
        return cash_ip_;

    size_t ip = slave_peer_.GetIP();
    std::vector<uint8_t> ip_address;
    while (ip)
    {
        ip_address.push_back(ip & 255);
        ip >>= 8;
    }
    std::reverse(ip_address.begin(), ip_address.end());

    std::string ip_address_str;
    bool is_first = true;
    for (auto &el : ip_address)
    {
        if (!is_first)
        {
            ip_address_str += '.';
        }
        is_first = false;
        ip_address_str += std::to_string(el);
    }

    cash_ip_ = ip_address_str;
    return cash_ip_;
}