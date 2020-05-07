// WebCrawler.h
// CSCE 463-500
// Luke Grammer
// 9/10/19

#pragma once

const std::string AGENT_NAME = "CPPWebCrawler/1.2";

// initial buffer size is 8KB
const UINT INITIAL_BUF_SIZE = 8 * 1024;

// if buffer size is greater than 32KB, reset buffer
const UINT BUF_RESET_THRESHOLD = 32 * 1024;

// size threshold for increasing buffer capacity
const UINT BUF_SIZE_THRESHOLD = 1024; 

// max time to download before connection times out
const UINT MAX_CONNECTION_TIME = 10;

// max time without read response before socket times out
const UINT TIMEOUT_SECONDS = 10;

class WebCrawler
{
	HTMLParserBase parser;
	ParsedURL url;
	SOCKET sock;

	struct hostent* remote;    // structure used in DNS lookups
	struct sockaddr_in server; // structure for connecting to server

	// Beginning and end time points for timer implementation
	std::chrono::time_point<std::chrono::high_resolution_clock> start_time, stop_time; 

public:
	// basic constructor initializes winsock and opens a TCP socket
	WebCrawler(ParsedURL _url = ParsedURL()); 

	// destructor cleans up winsock and closes socket
	~WebCrawler(); 

	// basic setter for the url data member
	void SetUrl(ParsedURL _url);

	// resolves DNS for the host specified by the url member and returns an IP address or -1 for failure
	int ResolveDNS();

	// creates a TCP connection to a server, returns -1 for failure and 0 for success
	int CreateConnection(); 

	// writes a properly formatted HTTP query to the connected server, returns -1 for failure and 0 for success
	int Write(std::string request_type, std::string request = ""); 

	// checks HTTP header in buf and returns true if the response code is between min_response and max_response (inclusive), false otherwise
	bool VerifyHeader(char *buf, int min_response, int max_response);

	// receives HTTP response from connected server
	int Read(char* &buf, size_t read_limit, size_t &cur_size, size_t &allocated_size); 

	// parses HTTP response and finds number of links in HTML buffer
	int Parse(char* buf, size_t size, bool print); 

	// resets socket connection
	int ResetConnection();
};

