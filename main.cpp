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



#pragma warning( push )
#pragma warning( disable : 4996)
#include <Packet.h>
#include <PcapFileDevice.h>
#include <TcpLayer.h>
#include <HttpLayer.h>
#include <IPv4Layer.h>

#pragma warning( pop )


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
    http_requests_vec_t httpRequests;
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

int main(int argc, char* argv[]) {
    std::string inputPath = R"(C:\Users\irahm\Documents\PcapParserVcpg\RaysharpLoginVideo.pcapng)";
    
    auto [httpRequests, rtspStreams] = prepareData(inputPath);


    
    return 0;
}