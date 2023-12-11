#include "Http.h"
#include "Rtsp.h"
#include "Generator.h"
#include "ReassemblyHelper.h"

#include <fstream>
#include <ranges>
#include <chrono>
#include <variant>
#include <algorithm>
#include <unordered_set>
#include <coroutine>
#include <functional>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#pragma warning( push )
#pragma warning( disable : 4996)
#include <Packet.h>
#include <PcapFileDevice.h>
#include <TcpLayer.h>
#include <HttpLayer.h>
#include <IPv4Layer.h>

#pragma warning( pop )

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;


Generator<pcpp::Packet> generatePackets(std::string inputPath)
{
    auto deleter = [](auto* reader) {
        reader->close();
        delete reader;
    };

    std::unique_ptr<pcpp::IFileReaderDevice, decltype(deleter)> reader{ pcpp::IFileReaderDevice::getReader(inputPath), deleter };

    if (!reader->open()) {
        std::cerr << "Error opening the pcap file!" << std::endl;
        co_return;
    }

    pcpp::RawPacket rawPacket;

    while (reader->getNextPacket(rawPacket)) {
        pcpp::Packet parsedPacket(&rawPacket);
        co_yield parsedPacket;
    }
}

void onTcpMessageReady(int8_t side, const pcpp::TcpStreamData& tcpData, void* userCookie) {
    auto* reassemblyHell = reinterpret_cast<ReassemblyHelper*>(userCookie);
    reassemblyHell->onTcpMessageReady(side, tcpData);
}

void onTcpConnectionStart(const pcpp::ConnectionData& connData, void* userCookie) {
    auto* reassemblyHell = reinterpret_cast<ReassemblyHelper*>(userCookie);
    reassemblyHell->onTcpConnectionStart(connData);
}

void onTcpConnectionEnd(const pcpp::ConnectionData& connData, pcpp::TcpReassembly::ConnectionEndReason reason, void* userCookie) {
    auto* reassemblyHell = reinterpret_cast<ReassemblyHelper*>(userCookie);
    reassemblyHell->onTcpConnectionEnd(connData, reason);
}

struct Data
{
    http_requests_map_t httpRequests;
    rtsp_stream_vec_t rtspStreams;
};

Data prepareData(std::string inputPath) {
    ReassemblyHelper reassembly;

    pcpp::TcpReassembly tcpReasembly{ onTcpMessageReady, &reassembly, onTcpConnectionStart, onTcpConnectionEnd };

    for (pcpp::Packet packet : generatePackets(inputPath)) {
        auto res = tcpReasembly.reassemblePacket(packet);
    }

    return { reassembly.getHttpRequests(), reassembly.getRtspStreams() };
}

net::awaitable<void> handle_session(tcp::socket socket, http_requests_map_t http) {
    for (;;) {
        beast::error_code ec;  // Declare error_code inside the coroutine
        beast::flat_buffer buffer;

        http::request<http::string_body> req;
        // The operation needs to co_await to wait for completion
        co_await http::async_read(socket, buffer, req, net::redirect_error(net::use_awaitable, ec));
        if (ec == http::error::end_of_stream)
            break;
        if (ec)
            co_return;

        auto reqs = http[req.target()];

        auto resp = *reqs->begin();
        http::response<http::string_body> res{ static_cast<http::status>(resp.response.m_code), req.version() };

        for (auto& [header, val] : resp.response.m_headers) {
            res.set(header, val);
            res.body() = resp.response.m_body;
        }

        co_await http::async_write(socket, res, net::redirect_error(net::use_awaitable, ec));
        if (ec)
            co_return;
        break;
    }

    beast::error_code ec;  // Declare a new error_code for the shutdown operation
    socket.shutdown(tcp::socket::shutdown_send, ec);
    // Handle the shutdown error if needed
}

net::awaitable<void> listener(tcp::endpoint endpoint, http_requests_map_t http) {
    beast::error_code ec; // Declare error_code before use
    auto executor = co_await net::this_coro::executor;
    tcp::acceptor acceptor(executor, endpoint);

    for (;;) {
        tcp::socket socket = co_await acceptor.async_accept(net::redirect_error(net::use_awaitable, ec));
        if (ec)
            co_return;

        net::co_spawn(executor, handle_session(std::move(socket), http), net::detached);
    }
}

int main(int argc, char* argv[]) {
    std::string inputPath = R"(C:\Users\irahm\Documents\PcapParserVcpg\RaysharpLoginVideo.pcapng)";
    
    auto [httpRequests, rtspStreams] = prepareData(inputPath);

    try {
        net::io_context ioc{ 1 };
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](auto, auto) { ioc.stop(); });
        net::co_spawn(ioc, listener({ tcp::v4(), 80 }, httpRequests), net::detached);
        ioc.run();
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
        
    return 0;
}