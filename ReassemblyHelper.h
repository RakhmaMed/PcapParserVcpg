#pragma once

#include <TcpReassembly.h>

#include "Http.h"

// flowkey, request
using http_requests_t = std::map<uint32_t, RequestResponse>;
using rtsp_stream_t = std::map<uint32_t, PrepareRtspStream>;

using http_requests_vec_t = std::vector<RequestResponse>;
using rtsp_stream_vec_t = std::vector<RtspStream>;

class ReassemblyHelper
{
    http_requests_t http_requests;
    rtsp_stream_t rtspStreams;

public:
    http_requests_vec_t getHttpRequests() {
        http_requests_vec_t reqs;
        for (auto&& req : http_requests) {
            reqs.push_back(req.second);
        }

        return reqs;
    }

    rtsp_stream_vec_t getRtspStreams() {
        rtsp_stream_vec_t streams;
        for (auto&& req : rtspStreams) {
            streams.push_back(req.second.getStream());
        }

        return streams;
    }

    void onTcpMessageReady(int8_t side, const pcpp::TcpStreamData& tcpData) {
        auto&& connData = tcpData.getConnectionData();
        if (util::isHttpPort(connData)) {
            parseHttp(side, tcpData);
        }
        else if (util::isRtspPort(connData)) {
            parseRtsp(side, tcpData);
        }
        else {

        }
    }

    void onTcpConnectionStart(const pcpp::ConnectionData& connData) {
        if (util::isHttpPort(connData)) {
            http_requests.emplace(connData.flowKey, RequestResponse{});
        }
        else if (util::isRtspPort(connData)) {
            rtspStreams.emplace(connData.flowKey, PrepareRtspStream{});
        }
        else {
            // Determine who opened connection
        }
    }

    void onTcpConnectionEnd(const pcpp::ConnectionData& connData, pcpp::TcpReassembly::ConnectionEndReason reason) {
        if (util::isHttpPort(connData)) {
            auto& reqresp = http_requests[connData.flowKey];
            if (reqresp.isEmpty()) {
                http_requests.erase(connData.flowKey);
                return;
            }
            reqresp.parse();
            //std::cout << reqresp;
        }
        else if (util::isRtspPort(connData)) {
            auto& rtspStream = rtspStreams[connData.flowKey];
        }
        else {
            // Determine who closed connection
        }
    }

private:
   
    util::ConnInfo getConnInfo(const pcpp::ConnectionData& connData) {
        return util::ConnInfo{
            .source_ip = connData.srcIP.toString(),
            .dest_ip = connData.dstIP.toString(),
            .source_port = connData.srcPort,
            .dest_port = connData.dstPort,
        };
    }

	void parseHttp(int8_t side, const pcpp::TcpStreamData& tcpData) {
        auto& reqresp = http_requests[tcpData.getConnectionData().flowKey];
        bool isRequest = side == 0;
        std::string_view data{reinterpret_cast<const char*>(tcpData.getData()), tcpData.getDataLength()};
        if (isRequest) {
            reqresp.request.setConnInfo(getConnInfo(tcpData.getConnectionData()));
            reqresp.request.append(data);
            reqresp.request.setTime(util::convertToTimestamp(tcpData.getTimeStamp()));
        }
        else {
            reqresp.response.setConnInfo(getConnInfo(tcpData.getConnectionData()));
            reqresp.response.append(data);
            reqresp.response.setTime(util::convertToTimestamp(tcpData.getTimeStamp()));
        }
	}

    void parseRtsp(int8_t side, const pcpp::TcpStreamData& tcpData) {
        auto& rtspStream = rtspStreams[tcpData.getConnectionData().flowKey];
        std::string data{ reinterpret_cast<const char*>(tcpData.getData()), tcpData.getDataLength() };
        bool isRequest = side == 0;
        rtspStream.parseRstp(data, isRequest);
    }
};