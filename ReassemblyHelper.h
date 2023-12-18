#pragma once

#include <TcpReassembly.h>

#include "Http.h"
#include "Generator.h"
#include <functional>

// flowkey, request
using http_requests_t = std::map<uint32_t, RequestResponse>;
using rtsp_stream_t = std::map<uint32_t, PrepareRtspStream>;

using http_requests_vec_t = std::vector<RequestResponse>;

using http_requests_map_t = std::map<std::string_view, http_requests_vec_t>;
using rtsp_stream_map_t = std::map<std::string, RtspStream>;
using rtsp_stream_map_SP_t = std::shared_ptr<rtsp_stream_map_t>;


struct ReqResHolder
{
    ReqResHolder(http_requests_map_t map) {
        for (auto&& [uri, http] : map) {
            m_uris[uri] = helper{ http };
        }
    }
    struct helper {
        std::vector<RequestResponse> vec{};
        uint32_t i = 0;

        const RequestResponse& getNextReqRes() {
            return vec[i++ % vec.size()];
        }
    };

    const RequestResponse& operator[](std::string_view str) {
        return m_uris[str].getNextReqRes();
    }

    std::map<std::string_view, helper> m_uris;
};
using req_res_holder_SP_t = std::shared_ptr<ReqResHolder>;

class ReassemblyHelper
{
    http_requests_t http_requests;
    rtsp_stream_t rtspStreams;

public:
    req_res_holder_SP_t getHttpRequests() {
        http_requests_map_t reqs;
        for (auto&& http : http_requests) {
            auto& req = reqs[http.second.request.uri()];
            req.push_back(http.second);
        }

        return std::make_shared<ReqResHolder>(reqs);
    }

    rtsp_stream_map_SP_t getRtspStreams() {
        auto streams = std::make_shared<rtsp_stream_map_t>();
        for (auto&& [flowKey, stream] : rtspStreams) {
            auto uri = stream.getStream().m_uri;
            if (uri.empty())
                continue;
            (*streams)[uri] = stream.getStream();
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
            //auto& rtspStream = rtspStreams[connData.flowKey];
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
            reqresp.request.append(data);
        }
        else {
            reqresp.response.append(data);
        }
	}
    void parseRtsp(int8_t side, const pcpp::TcpStreamData& tcpData) {
        auto& rtspStream = rtspStreams[tcpData.getConnectionData().flowKey];
        std::string data{ reinterpret_cast<const char*>(tcpData.getData()), tcpData.getDataLength() };
        bool isRequest = side == 0;
        rtspStream.parseRstp(data, isRequest);
    }
};