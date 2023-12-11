#pragma once

#include "Utility.h"
#include "PatternSeeker.h"

#include <fstream>

using headers_t = std::unordered_map<std::string, std::string>;

static const std::string_view DELIM = "<--__-->";

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
	std::string m_url;
};

struct PrepareRtspStream
{
private:
	std::vector<RtspStep> m_steps;
	std::string m_url;
	std::ofstream m_file;

public:
	void parseRstp(std::string data, bool isRequest) {
		if (isRequest) {
			parseRequest(data);
			return;
		}
		parseResponse(data);
	}

	RtspStream getStream() {
		return RtspStream{ m_steps, m_url };
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
			std::cout << "WARNING!!! Can't parse method!";
			return;
		}

		step.m_url = parser.extract(" ", " RTSP/1.0", move_after).to_string();
		m_url = step.m_url;
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
		m_steps.emplace_back(step);
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

		if (!m_file.is_open()) {
			m_file.open(replaceSymbols(m_url) + ".txt", std::ios::out | std::ios::binary);
		}
		if (m_file.is_open()) {
			m_file << data << DELIM;
		}
	}
};