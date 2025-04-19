#include "Http.h"
#include "Rtsp.h"
#include "Generator.h"
#include "ReassemblyHelper.h"
#include "Raysharp.h"

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
#include <boost/asio/as_tuple.hpp>

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
            if (!line.ends_with(DELIM)) {
                data += line + '`';
                continue;
            }

            data += line.substr(0, line.size() - DELIM.size());
            co_yield data;
            data.clear();
        }
    }
}
Generator<std::string> getData2(std::string filename) {
    std::ifstream file(filename, std::ios::binary);
    std::string data;

    if (!file.is_open()) {
        std::cout << "Can't open file: " << filename << '\n';
        co_return;
    }
    while (std::getline(file, data)) {
        co_yield data;
    }
}

using namespace std::chrono_literals;
net::awaitable<void> sleep_for(std::chrono::milliseconds duration) {
    auto executor = co_await net::this_coro::executor;
    net::steady_timer timer(executor);
    timer.expires_after(duration);
    co_await timer.async_wait(net::use_awaitable);
}

using shared_socket_t = std::shared_ptr<tcp::socket>;
net::awaitable<void> start_transferring_video(shared_socket_t socket, std::string filename) {
    for (auto&& data : getData(filename)) {
        co_await socket->async_write_some(net::buffer(data), net::use_awaitable);
        co_await sleep_for(10ms);
    }
}

using shared_socket_t = std::shared_ptr<tcp::socket>;
net::awaitable<void> start_transferring_video(shared_socket_t socket, std::vector<std::string> payload) {
    for (auto&& data : payload) {
        co_await socket->async_write_some(net::buffer(data), net::use_awaitable);
        co_await sleep_for(5ms);
    }
}

net::awaitable<void> handle_rtsp_session(shared_socket_t socket, rtsp_stream_map_SP_t rtsp) {
    for (;;) {
        data_t data;
        boost::system::error_code ec;
        co_await socket->async_read_some(net::buffer(data), net::redirect_error(net::use_awaitable, ec));
        if (ec)
            break;
        PatternSeeker parser{ {data.data(), data.size()}};
        static std::string_view methods[] = { "OPTIONS", "DESCRIBE", "SETUP", "PLAY", "TEARDOWN", "PAUSE" };
        bool res = std::ranges::any_of(methods, [&parser](auto&& method) { return parser.startsWith(method); });
        if (!res)
            continue;
        
        auto method = parser.extract(" ").to_string_view();
        std::cout << "Got " << method << '\n';

        auto url = parser.extract("rtsp://", " RTSP/1.0");
        url.to("/", move_before);
        auto uri = url.to_string();
        if (uri.empty()) {
            std::cout << "uri is missing\n";
            continue;
        }
        auto streamIt = std::ranges::find_if(*rtsp, [uri](auto&& elem) { return uri.starts_with(elem.first); });
        if (streamIt == rtsp->end()) {
            std::cout << "can't find this uri: " << uri << '\n';
            continue;
        }
        auto& step = streamIt->second.getNextStep();
        if (step.method == method)
            co_await socket->async_write_some(net::buffer(step.response), net::use_awaitable);
        else
            std::cout << "Wrong command, expected: " << step.method << ", actual " << method << '\n';

        if (method == "PLAY") {
            //std::string path = replaceSymbols(uri) + ".txt";
            net::co_spawn(socket->get_executor(), start_transferring_video(socket, streamIt->second.m_payload), net::detached);
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

        auto shared_soket = std::make_shared<tcp::socket>(std::move(socket));

        net::co_spawn(executor, handle_rtsp_session(shared_soket, rtsp), net::detached);
    }
}

struct RTPHeaderExtension
{
    RTPHeaderExtension() :
        definedByProfile(0)
    {}

    uint16_t		definedByProfile;
    std::vector<char>	data;
};

using rtp_header_opt_t = std::optional<RTPHeaderExtension> ;

struct RtpPacketHeader
{
    RtpPacketHeader() :
        isMark(false),
        padding(false),
        sequenceNumber(0),
        payloadType(0),
        rtpTimeStamp(0),
        ssrc(0)
    {}

    bool			isMark;
    bool			padding;
    uint16_t		sequenceNumber;
    uint8_t		    payloadType;
    uint32_t		rtpTimeStamp;
    uint32_t		ssrc;

    rtp_header_opt_t	extension;
};

const int RTP_HEADER_SIZE = 12;

struct RTSPInterleavedHeader
{
    boost::uint8_t				magic;
    boost::uint8_t				channel;
    boost::endian::big_int16_t  length;
};

int main(int argc, char* argv[]) {
    std::string inputPath;
    if (argc < 2) {
        inputPath = R"(C:\Users\irahm\Documents\GitHub\PcapParserVcpg\fd_meta.pcapng)";
    }
    else if (argc == 2) {
        inputPath = argv[1];
    }

    auto [httpRequests, rtspStreams] = prepareData(inputPath);
    auto& stream = rtspStreams->begin()->second;
    std::vector<std::string> payload;
    size_t await_data_size = 0;
    for (auto&& data : stream.m_payload) {
        if (data[0] == '$' && await_data_size == 0) {
            payload.push_back(data);
            auto rtsp_header = reinterpret_cast<const RTSPInterleavedHeader*>(data.data());
            std::cout << rtsp_header->magic << " data.size: " << data.size()  << "\nrtsp packet length: " << rtsp_header->length << '\n';
            await_data_size = rtsp_header->length - (data.size() - sizeof(RTSPInterleavedHeader));
            std::cout << "await data length: " << await_data_size << '\n';
            RtpPacketHeader rtpHeader;
            std::span data_view = std::string_view{data.data() + sizeof(RTSPInterleavedHeader), data.size() - sizeof(RTSPInterleavedHeader)};
            util::BitStream bs{ data_view };
            const uint32_t version = bs.pop(2);
            if (version != 2) {
                std::cout << "version is not 2: " << version << '\n';
                await_data_size = 0;
                continue;
            }
            
            // Padding bit
            rtpHeader.padding = (bs.pop(1) != 0);

            const bool hasExtension = (bs.pop(1) != 0);
            std::cout << "has extension: " << hasExtension << '\n';
            boost::uint32_t csrcCount = bs.pop(4);
            std::cout << "csrc count: " << csrcCount << '\n';

            // Marker bit
            rtpHeader.isMark = (bs.pop(1) != 0);
            std::cout << "marker bit: " << rtpHeader.isMark << '\n';

            //payload type.
            rtpHeader.payloadType = static_cast<boost::uint8_t>(bs.pop(7));
            std::cout << "payload type: " << static_cast<int>(rtpHeader.payloadType) << '\n';

            // Sequence Number.
            rtpHeader.sequenceNumber = static_cast<boost::uint16_t>(bs.pop(16));
            std::cout << "sequence number: " << rtpHeader.sequenceNumber << '\n';

            // Timestamp.
            rtpHeader.rtpTimeStamp = bs.pop(32);
            std::cout << "timestamp: " << rtpHeader.rtpTimeStamp << '\n';

            // SSRC
            rtpHeader.ssrc = bs.pop(32);
            std::cout << "ssrc: " << rtpHeader.ssrc << '\n';

            // CSRC identifiers (optional).
            const int csrcLen = csrcCount * 4;
            std::cout << "csrc len: " << csrcLen << '\n';
            
            bs.skip(csrcLen * 8);

            if (hasExtension)
            {
                rtpHeader.extension = RTPHeaderExtension{};

                // defined by profile.
                rtpHeader.extension.value().definedByProfile = static_cast<uint16_t>(bs.pop(16));

                // Extension length in 8-bits units.
                const boost::uint32_t extension_length = bs.pop(16) * 4;
                std::cout << "extension_length: " << extension_length << '\n';
                if (std::distance(bs.position(), data_view.end()) < static_cast<std::ptrdiff_t>(extension_length))
                {
                    std::cout << "Not enough data\n";
                    continue;
                }
                rtpHeader.extension.value().data.assign(bs.position(), bs.position() + extension_length);

                bs.skip(extension_length * 8);
            }
            
            std::cout << "data_view: " << std::distance(bs.position(), data_view.end())<< '\n';
            if (rtpHeader.payloadType == 108) {
                Raysharp::parsedtPayload(bs);
                return 0;
            }
        }
        else {
            payload.back().append(data.substr(0, await_data_size));
            await_data_size = 0;
            std::cout << "no " << data.size() << "\n";
        }
    }

    stream.m_payload = payload;

    /*std::ofstream file(R"(C:\Users\irahm\Desktop\output_as_is.txt)", std::ios::out | std::ios::binary);
    for (pcpp::Packet packet : generatePackets(inputPath)) {
        if (!packet.isPacketOfType(pcpp::IPv4)) {
            continue;
        }
        pcpp::TcpLayer* tcpLayer = packet.getLayerOfType<pcpp::TcpLayer>();
        if (!tcpLayer)
            continue;

        uint16_t destPort = tcpLayer->getDstPort();
        uint16_t sourcePort = tcpLayer->getSrcPort();

        if (destPort == 554 || sourcePort == 554) {
            uint8_t* data = tcpLayer->getLayerPayload();
            size_t dataLen = tcpLayer->getLayerPayloadSize();

            file.write(reinterpret_cast<const char*>(data), dataLen);
        }
    }*/

    // try {
    //     net::io_context ioc{ 1 };
    //     net::signal_set signals(ioc, SIGINT, SIGTERM);
    //     signals.async_wait([&ioc](auto, auto) { ioc.stop(); });
        
    //     net::co_spawn(ioc, http_listener({ tcp::v4(), 80 }, httpRequests), net::detached);
    //     net::co_spawn(ioc, rtsp_listener({ tcp::v4(), 554 }, rtspStreams), net::detached);
    //     ioc.run();
    // }
    // catch (std::exception& e) {
    //     std::cerr << "Error: " << e.what() << std::endl;
    //     return EXIT_FAILURE;
    // }
        
    return 0;
}