#pragma once

#include "Utility.h"
#include "PatternSeeker.h"

#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

using headers_t = std::unordered_map<std::string, std::string>;

static const std::string_view DELIM = "<--__-->";

// TODO: come up with better name
std::string replaceSymbols(std::string str) {
	for (auto& ch : str) {
		if (ch == ':' || ch == '/')
			ch = '-';
	}
	return str;
}

struct RtspStep
{
	std::string method;
	std::string m_url;
	headers_t headers;
	std::string response;

	friend std::ostream& operator<<(std::ostream& oss, RtspStep& step) {
		oss << step.method << ' ' << step.m_url;
		for (auto&& [header, val] : step.headers) {
			oss << '\n' << header << ": " << val;
		}
		if (!step.response.empty())
			oss << "\n" << step.response << '\n';
		return oss;
	}
};

struct RtspStream
{
	std::vector<RtspStep> m_steps;
	std::string m_uri;
	std::vector<std::string> m_payload;
	uint32_t step = 0;

	const RtspStep& getNextStep() {
		return m_steps[step++ % m_steps.size()];
	}
};

struct PrepareRtspStream
{
private:
	std::vector<RtspStep> m_steps;
	std::string m_uri;
	//std::ofstream m_file;
	//std::ofstream m_testfileOut;
	//std::ofstream m_testfileIn;
	std::vector<std::string> m_payload;

public:
	void parseRstp(std::string data, bool isRequest) {
		if (isRequest) {
			parseRequest(data);
			return;
		}
		parseResponse(data);
	}

	RtspStream getStream() const {
		return RtspStream{ m_steps, m_uri, m_payload };
	}

	friend std::ostream& operator<<(std::ostream& oss, RtspStream& stream) {
		for (auto&& step : stream.m_steps) {
			oss << step << '\n';
		}
		return oss;
	}

private:
	void parseRequest(std::string data) {

		RtspStep step;
		PatternSeeker parser{ data };

		static std::string_view methods[] = { "OPTIONS", "DESCRIBE", "SETUP", "PLAY", "TEARDOWN", "PAUSE" };

		for (auto&& method : methods) {
			if (parser.startsWith(method)) {
				step.method = parser.extract(" ").to_string();
				break;
			}
		}

		if (step.method.empty()) {
			std::cout << "WARNING!!! Can't parse method!\n";
			std::cout << data.size();
			return;
		}

		auto url = parser.extract("rtsp://", " RTSP/1.0", move_after);
		step.m_url = url.to_string();
		url.to("/", move_before);
		m_uri = url.to_string();
		while (parser.isNotEmpty()) {
			parser.skipWhiteSpaces();
			auto name = parser.extract(":", PatterSeekerNS::move_after);
			if (name.isEmpty())
				break;
			auto val = parser.extract("\n", PatterSeekerNS::move_after);
			if (val.isEmpty())
				val = parser;
			step.headers.emplace(name.to_string(), util::trim(val.to_string()));
		}
		m_steps.push_back(step);
	}

	void parseResponse(std::string data) {
		PatternSeeker parser{ data };
		if (parser.expect("RTSP/1.0")) {
			if (m_steps.empty()) {
				std::cout << "WARNING!!! We got response without request";
				return;
			}
			m_steps.back().response = data;
			return;
		}

		m_payload.push_back(data);

		/*if (!m_file.is_open()) {
			m_file.open(replaceSymbols(m_uri) + ".txt", std::ios::out | std::ios::binary);
		}

		if (!m_testfileOut.is_open()) {
			m_testfileOut.open(replaceSymbols(m_uri) + "out.txt", std::ios::out | std::ios::binary);
		}
		if (m_file.is_open()) {
			m_file << data << DELIM << '`';
			m_testfileOut << data;
		}*/
	}
};