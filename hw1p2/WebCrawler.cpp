// WebCrawler.cpp
// CSCE 463-500
// Luke Grammer
// 9/10/19

#include "pch.h"

using namespace std;

// basic constructor initializes winsock and opens a TCP socket
WebCrawler::WebCrawler(ParsedURL _url) : remote { nullptr }, server{ NULL }, parser{HTMLParserBase()}
{   
	WSADATA wsa_data;
	WORD w_ver_requested;
	
	url = _url;

	//initialize WinSock
	w_ver_requested = MAKEWORD(2, 2);
	if (WSAStartup(w_ver_requested, &wsa_data) != 0) {
		printf("\tWSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	// open a TCP socket
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		printf("\tsocket() generated error %d\n", WSAGetLastError());
		WSACleanup();
		exit(EXIT_FAILURE);
	}
}

// destructor cleans up winsock and closes socket
WebCrawler::~WebCrawler() 
{   
	closesocket(sock);
	WSACleanup();
}

// basic setter for the url data member
void WebCrawler::SetUrl(ParsedURL _url)
{
	url = _url;
}

// resolves DNS for the host specified by the url member and returns an IP address or -1 for failure
int WebCrawler::ResolveDNS()
{
	char* host;
	DWORD IP;
	struct in_addr addr;

	if (!url.valid)
	{
		printf("supplied url not valid in resolveDNS()\n");
		return -1;
	}
	
	host = (char*) malloc(MAX_HOST_LEN);
	if (host == NULL)
	{
		printf("malloc failure for hostname\n");
		return -1;
	}

	// need to put the hostname in a C string
	strcpy_s(host, MAX_HOST_LEN, url.host.c_str()); 

	start_time = chrono::high_resolution_clock::now();
    
	// first assume that the hostname is an IP address
	IP = inet_addr(host); 
	
	// host is not a valid IP, do a DNS lookup
	if (IP == INADDR_NONE)
	{   
		if ((remote = gethostbyname(host)) == NULL) 
		{
			printf("failed with %d\n", WSAGetLastError());
			free(host);
			return 0;
		}
		// take the first IP address and copy into sin_addr, stop timer and print
		else 
		{   
			stop_time = chrono::high_resolution_clock::now();
			IP = *(u_long*)remote->h_addr;
			addr.s_addr = IP;
			memcpy((char*) &(server.sin_addr), remote->h_addr, remote->h_length);
			printf("done in %" PRIu64 " ms, found %s\n", 
				   chrono::duration_cast<chrono::milliseconds>
				   (stop_time - start_time).count(), inet_ntoa(addr));
		}
	}
	// host is a valid IP, directly drop its binary version into sin_addr, stop timer and print
	else
	{
		stop_time = chrono::high_resolution_clock::now();
		printf("done in %" PRIu64 " ms, found %s\n", 
			   chrono::duration_cast<chrono::milliseconds>
			   (stop_time - start_time).count(), host);
		server.sin_addr.S_un.S_addr = IP;
	}

	free(host);
	return IP;
}

// creates a TCP connection to a server, returns -1 for failure and 0 for success
int WebCrawler::CreateConnection()
{ 
	if (!url.valid)
	{
		printf("supplied url not valid in createConnection()\n");
		return -1;
	}

	// set up the port # and protocol type
	server.sin_family = AF_INET;
	server.sin_port = htons(url.port); 

	// start timer and connect to the server
	start_time = chrono::high_resolution_clock::now();
	if (connect(sock, (struct sockaddr*) &server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		printf("failed with %d\n", WSAGetLastError());
		return -1;
	}
	stop_time = chrono::high_resolution_clock::now();

	printf("done in %" PRIu64 " ms\n", 
		   chrono::duration_cast<chrono::milliseconds>
		   (stop_time - start_time).count());

	return 0;
}

// writes a properly formatted HTTP query to the connected server, returns -1 for failure and 0 for success
int WebCrawler::Write(string request_type, string request)
{
	if (request == "")
		request = url.request;

	string http_request = request_type + " " + request + " HTTP/1.1\r\nUser-agent: " + 
		        AGENT_NAME + "\r\nHost: " + url.host + "\r\nConnection: close\r\n\r\n";

	if (send(sock, http_request.c_str(), (int) strlen(http_request.c_str()), NULL) < 0)
	{
		printf("failed with %d\n", WSAGetLastError());
		return -1;
	}

	return 0;
}

// receives HTTP response from connected server
int WebCrawler::Read(char* &buf, const size_t read_limit, size_t &cur_size, size_t &allocated_size)
{
	// set socket timeout value
	struct timeval timeout; 
	timeout.tv_sec = TIMEOUT_SECONDS; 
	timeout.tv_usec = 0;

	int ret = 0;
	fd_set fd;
	FD_ZERO(&fd);

	cur_size = 0;

	// start connection timer
	start_time = chrono::high_resolution_clock::now();
	stop_time = chrono::high_resolution_clock::now();
	while ((chrono::duration_cast<chrono::milliseconds>(stop_time - start_time).count() / 1000.0) < MAX_CONNECTION_TIME &&
		   cur_size < read_limit)
	{
		FD_SET(sock, &fd);

		// wait to see if socket has any data
		ret = select((int) sock + 1, &fd, NULL, NULL, &timeout);
		if (ret > 0)
		{
			// new data available
			int bytes = recv(sock, buf + cur_size, (int) (allocated_size - cur_size), NULL);
			if (bytes < 0)
			{
				printf("failed with %d on recv\n", WSAGetLastError());
				return -1;
			}

			// advance current position by number of bytes read
			cur_size += bytes; 
			
			// buffer needs to be expanded
			if (allocated_size - cur_size < BUF_SIZE_THRESHOLD)
			{   
				// make sure allocated_size will not overflow
				if (2 * allocated_size < allocated_size) 
				{   
					printf("failed with buffer overflow\n");
					free(buf);
					buf = NULL;
					cur_size = 0;
					allocated_size = 0;
					return -1;
				}

			    // expand memory for buffer, making sure the expansion succeeds
				char* temp = (char*) realloc(buf, 2 * allocated_size); 
				if (temp == NULL)
				{
					printf("realloc failed for buffer\n");
					free(buf);
					buf = NULL;
					cur_size = 0;
					allocated_size = 0;
					return -1;
				}

                // double allocated size with each expansion (higher overhead but faster)
				allocated_size *= 2; 
				buf = temp;
			}
			// connection closed
			if (bytes == 0) 
			{
				if (buf == NULL)
				{
					printf("nothing written to buffer\n");
					cur_size = 0;
					allocated_size = 0;
					return -1;
				}

				char* response_pos = strstr(buf, "HTTP/");
				// response not found, clean up and return
				if (response_pos == NULL)
				{   
					printf("failed with non-HTTP header\n");
					return -1;
				}

				stop_time = chrono::high_resolution_clock::now();
				/* Null-terminate buffer
				 *
				 * Warning C6386 due to indexing by cur_size, but buffer overflow is not possible because 
				 * allocated_size is strictly > cur_size while BUF_SIZE_THRESHOLD > 0 
				 */
				buf[cur_size] = '\0'; 

				printf("done in %" PRIu64 " ms with %zu bytes\n", 
					   chrono::duration_cast<chrono::milliseconds>
					   (stop_time - start_time).count(), cur_size);
				return 0;
			}
		}
		// socket timed out
		else if (ret == 0) 
		{   
			stop_time = chrono::high_resolution_clock::now();
			printf("socket timeout\n");
			return -1;
		}
		else
		{
			printf("failed with %d on select\n", WSAGetLastError());
			return -1;
		}

		stop_time = chrono::high_resolution_clock::now();
	}
	// buf has advanced more than read_limit bytes
	if (cur_size >= read_limit)
	{
		printf("failed with exceeding max\n");
		return -1;
	}
	// connection timer expired
	printf("connection timeout\n");
	return -1;
}

// checks HTTP header in buf and returns true if the response code is between min_response and max_response (inclusive), false otherwise
bool WebCrawler::VerifyHeader(char* buf, int min_response, int max_response)
{
	int response = -1;

	// get response code from HTTP header
	char* response_pos = strstr(buf, "HTTP/");

	// response is found
	if (response_pos != NULL)
	{   
		// response code could not be extracted
		if (sscanf_s(response_pos, "%*s %d", &response) <= 0)
		{   
			printf("failed with non-HTTP header\n");
			return false;
		}
	}
	else
	{   
		printf("failed with non-HTTP header\n");
		return false;
	}

	printf("status code %d\n", response);

	if (response >= min_response && response <= max_response)
	{
		return true;
	}

	return false;
}

// parses HTTP response and finds number of links in HTML buffer
int WebCrawler::Parse(char *buf, size_t size, bool print)
{
	int num_links = -1;
	char *base_url = NULL;   
	string base_url_string = url.scheme + "://" + url.host;

	base_url = (char*)malloc(MAX_URL_LEN); 
	if (base_url == NULL)
	{
		printf("malloc failure for base URL\n");
		return -1;
	}

	// create C-style string with base URL
	strcpy_s(base_url, MAX_URL_LEN, base_url_string.c_str());

	// start timer
	start_time = chrono::high_resolution_clock::now(); 

	// get number of links from response
	char* link_buffer = parser.Parse(buf, (int)size, base_url, (int)strlen(base_url), &num_links);
	if (num_links < 0)
	{
		printf("HTML parsing error\n");
		free(base_url);
		return -1;
	}

	// stop timer and print information
	stop_time = chrono::high_resolution_clock::now();
	printf("done in %" PRIu64 " ms with %d links\n", chrono::duration_cast<chrono::milliseconds>(stop_time - start_time).count(), num_links);
	
	if (print)
	{
		printf("___________________________________________________________________________________\n");

		// extract and print HTTP header
		char* header_end = strstr(buf, "\r\n\r\n");
		if (header_end == NULL)
		{
			printf("unexpected error printing HTTP header\n");
			free(base_url);
			return -1;
		}

		*header_end = '\0';
		printf("%s\r\n", buf);
	}

	// clean up
	free(base_url); 
	return 0;
}

// resets socket connection
int WebCrawler::ResetConnection()
{
	if (closesocket(sock) == SOCKET_ERROR)
	{
		printf("closesocket generated error %d\n", WSAGetLastError());
		return -1;
	}

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		printf("socket() generated error %d\n", WSAGetLastError());
		return -1;
	}

	return 0;
}