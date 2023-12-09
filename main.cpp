#include "Http.h"
#include "Rtsp.h"
#include "Generator.h"

#include <fstream>
#include <ranges>
#include <chrono>
#include <variant>
#include <algorithm>
#include <unordered_set>
#include <coroutine>
#include <functional>

#include <Packet.h>
#include <PcapFileDevice.h>
#include <TcpLayer.h>
#include <HttpLayer.h>
#include <IPv4Layer.h>


void analyzePacket(pcpp::RawPacket& rawPacket, std::ofstream& outputFile) {
    pcpp::Packet parsedPacket(&rawPacket);

    if (parsedPacket.isPacketOfType(pcpp::TCP)) {
        pcpp::TcpLayer* tcpLayer = parsedPacket.getLayerOfType<pcpp::TcpLayer>();

        if (tcpLayer->getTcpHeader()->portDst == htons(554) || tcpLayer->getTcpHeader()->portSrc == htons(554)) {
            uint8_t* data = tcpLayer->getLayerPayload();
            size_t dataLen = tcpLayer->getLayerPayloadSize();

            outputFile.write(reinterpret_cast<const char*>(data), dataLen);
        }
    }
}

void analyzePackets(std::string inputPath, std::ofstream& outputFile) {
    auto deleter = [](auto* reader) {
        reader->close();
        delete reader;
    };

    std::unique_ptr<pcpp::IFileReaderDevice, decltype(deleter)> reader{ pcpp::IFileReaderDevice::getReader(inputPath), deleter };

    if (!reader->open()) {
        std::cerr << "Error opening the pcap file!" << std::endl;
        return;
    }

    pcpp::RawPacket rawPacket;
    while (reader->getNextPacket(rawPacket)) {
        analyzePacket(rawPacket, outputFile);
    }
}

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



std::string printTcpFlags(pcpp::TcpLayer* tcpLayer)
{
    std::string result = "";
    if (tcpLayer->getTcpHeader()->synFlag == 1)
        result += "SYN ";
    if (tcpLayer->getTcpHeader()->ackFlag == 1)
        result += "ACK ";
    if (tcpLayer->getTcpHeader()->pshFlag == 1)
        result += "PSH ";
    if (tcpLayer->getTcpHeader()->cwrFlag == 1)
        result += "CWR ";
    if (tcpLayer->getTcpHeader()->urgFlag == 1)
        result += "URG ";
    if (tcpLayer->getTcpHeader()->eceFlag == 1)
        result += "ECE ";
    if (tcpLayer->getTcpHeader()->rstFlag == 1)
        result += "RST ";
    if (tcpLayer->getTcpHeader()->finFlag == 1)
        result += "FIN ";

    return result;
}

std::string printTcpOptionType(pcpp::TcpOptionType optionType)
{
    switch (optionType)
    {
    case pcpp::PCPP_TCPOPT_NOP:
        return "NOP";
    case pcpp::PCPP_TCPOPT_TIMESTAMP:
        return "Timestamp";
    default:
        return "Other";
    }
}

int main(int argc, char* argv[]) {
    std::string inputPath = R"(C:\Users\irahm\Documents\PcapParserVcpg\RaysharpLoginVideo.pcapng)";
    // std::string outputPath = R"(C:\Users\irahm\Documents\output.txt)";

    /* std::ofstream outputFile(outputPath);
    if (!outputFile.is_open())
    {
        std::cerr << "Error opening the output file!" << std::endl;
        return 0;
    }*/


    std::unordered_set<ConnInfo, ConnInfoHash> connections;

    for (pcpp::Packet packet : generatePackets(inputPath)) {

        if (!packet.isPacketOfType(pcpp::IPv4))
            continue;

        pcpp::IPv4Address srcIP = packet.getLayerOfType<pcpp::IPv4Layer>()->getSrcIPv4Address();
        pcpp::IPv4Address destIP = packet.getLayerOfType<pcpp::IPv4Layer>()->getDstIPv4Address();

        pcpp::TcpLayer* tcpLayer = packet.getLayerOfType<pcpp::TcpLayer>();
        if (!tcpLayer)
            continue;

        uint16_t destPort = tcpLayer->getDstPort();
        uint16_t sourcePort = tcpLayer->getSrcPort();
        
        auto timestamp = convertToTimestamp(packet.getRawPacket()->getPacketTimeStamp());
        ConnInfo info{
            .source_ip = srcIP.toString(),
            .dest_ip = destIP.toString(),
            .source_port = sourcePort,
            .dest_port = destPort,
        };

        pcpp::HttpRequestLayer* httpRequestLayer = packet.getLayerOfType<pcpp::HttpRequestLayer>();
        if (!httpRequestLayer)
            continue;
        std::string data{ (const char*)httpRequestLayer->getData(), httpRequestLayer->getDataLen() };
        std::cout << data << "\n\n---\n";

        http_method_t method = httpRequestLayer->getFirstLine()->getMethod();
        auto uri = httpRequestLayer->getFirstLine()->getUri();
        std::string body{ (const char*)httpRequestLayer->getLayerPayload(), httpRequestLayer->getLayerPayloadSize() };

        HttpRequest request{
            .time = timestamp,
            .conninfo = info,
            .method = method,
            .uri = uri,
            .body = body,
        };

//std::cout << request << '\n';
    }
   

    return 0;
}