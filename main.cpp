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
    req_res_holder_SP_t httpRequests;
    rtsp_stream_map_SP_t rtspStreams;
};

Data prepareData(std::string inputPath) {
    ReassemblyHelper reassembly;

    pcpp::TcpReassembly tcpReasembly{ onTcpMessageReady, &reassembly, onTcpConnectionStart, onTcpConnectionEnd };

    for (pcpp::Packet packet : generatePackets(inputPath)) {
        auto res = tcpReasembly.reassemblePacket(packet);
    }

    return { reassembly.getHttpRequests(), reassembly.getRtspStreams() };
}

net::awaitable<void> handle_http_session(tcp::socket socket, req_res_holder_SP_t http) {
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

        auto reqres = (*http)[req.target()];

        auto& resp = reqres.response;
        http::response<http::string_body> res{ static_cast<http::status>(resp.m_code), req.version() };

        for (auto& [header, val] : resp.m_headers) {
            res.set(header, val);
            res.body() = resp.m_body;
        }

        co_await http::async_write(socket, res, net::redirect_error(net::use_awaitable, ec));
        if (ec)
            co_return;
    }

    beast::error_code ec;  // Declare a new error_code for the shutdown operation
    socket.shutdown(tcp::socket::shutdown_send, ec);
    // Handle the shutdown error if needed
}

net::awaitable<void> http_listener(tcp::endpoint endpoint, req_res_holder_SP_t http) {
    beast::error_code ec; // Declare error_code before use
    auto executor = co_await net::this_coro::executor;
    tcp::acceptor acceptor(executor, endpoint);

    for (;;) {
        tcp::socket socket = co_await acceptor.async_accept(net::redirect_error(net::use_awaitable, ec));
        if (ec)
            co_return;

        net::co_spawn(executor, handle_http_session(std::move(socket), http), net::detached);
    }
}

using data_t = std::array<char, 1024>;
Generator<std::string> getData(std::string filename) {
    std::ifstream file(filename, std::ios::binary);
    std::string data;
    std::string line;
    static const std::string_view DELIM = "<--__-->";

    if (!file.is_open()) {
        std::cout << "Can't open file: " << filename << '\n';
        co_return;
    }
    while (file.is_open() && !file.eof()) {
        while (std::getline(file, line, '`')) {
            //std::cout << line;
            if (!line.ends_with(DELIM)) {
                data += line;
                continue;
            }

            data += line.substr(0, line.size() - DELIM.size());
            co_yield data;
            file.ignore(1);
            data.clear();
        }
    }
}

using namespace std::chrono_literals;
net::awaitable<void> sleep_for(std::chrono::milliseconds duration) {
    auto executor = co_await net::this_coro::executor;
    net::steady_timer timer(executor);
    timer.expires_after(duration);
    co_await timer.async_wait(net::use_awaitable);
}

net::awaitable<void> start_transferring_video(tcp::socket socket, std::string filename) {
    for (auto&& data : getData(filename)) {
        std::cout << data;
        co_await socket.async_write_some(net::buffer(data), net::use_awaitable);
        co_await sleep_for(10ms);
    }
    boost::system::error_code ec;
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

net::awaitable<void> handle_rtsp_session(tcp::socket socket, rtsp_stream_map_SP_t rtsp) {
    for (;;) {
        data_t data;
        boost::system::error_code ec;
        co_await socket.async_read_some(net::buffer(data), net::redirect_error(net::use_awaitable, ec));
        if (ec)
            break;
        PatternSeeker parser{ {data.data(), data.size()}};
        static std::string_view methods[] = { "OPTIONS", "DESCRIBE", "SETUP", "PLAY", "TEARDOWN", "PAUSE" };
        bool res = std::ranges::any_of(methods, [&parser](auto&& method) { return parser.startsWith(method); });
        if (!res)
            continue;
        
        auto method = parser.extract(" ").to_string_view();
        std::cout << "Got " << method << '\n';

        auto url = parser.extract(" ", " RTSP/1.0");
        if (url.isEmpty()) {
            std::cout << "URL is missing\n";
            continue;
        }
        auto streamIt = std::ranges::find_if(*rtsp, [url](auto&& elem) { return url.startsWith(elem.first); });
        if (streamIt == rtsp->end()) {
            std::cout << "can't find this url: " << url.to_string_view() << '\n';
            continue;
        }
        auto& step = streamIt->second.getNextStep();
        if (step.method == method)
            co_await socket.async_write_some(net::buffer(step.response), net::use_awaitable);
        else
            std::cout << "Wrong command, expected: " << step.method << ", actual " << method << '\n';

        if (method == "PLAY") {
            std::string path = replaceSymbols(streamIt->second.m_url) + ".txt";
            net::co_spawn(socket.get_executor(), start_transferring_video(std::move(socket), path), net::detached);
            break;
        }
    }
}

net::awaitable<void> rtsp_listener(tcp::endpoint endpoint, rtsp_stream_map_SP_t rtsp) {
    beast::error_code ec; // Declare error_code before use
    auto executor = co_await net::this_coro::executor;
    tcp::acceptor acceptor(executor, endpoint);

    for (;;) {
        tcp::socket socket = co_await acceptor.async_accept(net::redirect_error(net::use_awaitable, ec));
        if (ec)
            co_return;

        net::co_spawn(executor, handle_rtsp_session(std::move(socket), rtsp), net::detached);
    }
}

int main(int argc, char* argv[]) {
    std::string inputPath = R"(C:\Users\irahm\Documents\PcapParserVcpg\RaysharpLoginVideo.pcapng)";
    auto [httpRequests, rtspStreams] = prepareData(inputPath);

    try {
        net::io_context ioc{ 1 };
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](auto, auto) { ioc.stop(); });
        net::co_spawn(ioc, http_listener({ tcp::v4(), 80 }, httpRequests), net::detached);
        net::co_spawn(ioc, rtsp_listener({ tcp::v4(), 554 }, rtspStreams), net::detached);
        ioc.run();
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
        
    return 0;
}