// hw1p2.cpp
// CSCE 463-500
// Luke Grammer
// 9/10/19

#include "pch.h"

#define _CRTDBG_MAP_ALLOC  
#include <stdlib.h>  
#include <crtdbg.h> // libraries to check for memory leaks

#pragma comment(lib, "ws2_32.lib")

// valid number of command line args 
const unsigned NUM_ARGS = 3; 

// smallest valid number of threads
const unsigned MIN_NUM_THREADS = 1; 

// largest valid number of threads
const unsigned MAX_NUM_THREADS = 1; 

// max size of robots.txt to download (16KB)
const size_t   MAX_ROBOTS_SIZE = 16 * 1024; 

// max size of page to download (2MB)
const size_t   MAX_PAGE_SIZE = 2 * 1024 * 1024; 

using namespace std;

/*
 * Function: CrawlUrls
 * ------------------
 * Crawls through a list of URLs in a given input file, initiating connection
 * if the crawler has not attempted to connect to the host before and 
 * first requesting the HTTP header for /robots.txt. If robots.txt is not found
 * (4XX) response code then the page specified by the URL is requested and 
 * parsed to find the number of links on the page.
 *
 * input:
 *   - seen_ips: data structure to hold the list of IP addresses visited by the crawler
 *   - seen_hosts: data structure to hold the list of hosts visited by the crawler
 *   - file: an open input file containing the URLs that should be crawled
 *
 * return: a status code that will be -1 in the case that an error is encountered,
 *         or 0 for successful execution
 */
int CrawlUrls(unordered_set<DWORD> &seen_ips, unordered_set<string> &seen_hosts, FILE* file)
{
	if (!file)
	{
		printf("error: input file has not yet been opened for reading\n");
		return -1;
	}
	
	char *url = (char*) malloc(MAX_URL_LEN);
	if (url == NULL)
	{
		printf("error: malloc failed for url");
		return -1;
	}

	char* buffer = NULL;

	ParsedURL parsed_url;
	WebCrawler crawler;
	size_t prev_size = 0;
	size_t cur_buf_size = 0;
	size_t allocated_size = 0;

	while (fgets(url, MAX_URL_LEN, file) != NULL)
	{
		printf("\n");
		crawler.ResetConnection();
		
		// buffer should be de-allocated if it is too large
		if (allocated_size > BUF_RESET_THRESHOLD)
		{
			free(buffer);
			buffer = NULL;
			cur_buf_size = 0;
			allocated_size = 0;
		}

		// buffer should be allocated before it is used
		if (buffer == NULL)
		{
			buffer = (char*) malloc(INITIAL_BUF_SIZE);
			if (buffer == NULL)
			{
				printf("malloc failed for buffer");
				return -1;
			}
			allocated_size = INITIAL_BUF_SIZE;
			cur_buf_size = 0;
		}

		// parse the URL read from the file
		// --------------------------------------------------------------------
		string url_string(url);

		// remove carriage return and newline since they are not valid URI characters
		url_string.erase(remove(url_string.begin(), url_string.end(), '\r'), url_string.end());
		url_string.erase(remove(url_string.begin(), url_string.end(), '\n'), url_string.end());

		printf("URL: %s\n", url_string.c_str());
		printf("\tParsing URL... ");
		parsed_url = ParsedURL::ParseUrl(url_string);

		if (!parsed_url.valid)
			continue;

		// link the parsed URL with the crawler
		crawler.SetUrl(parsed_url);

		// check uniqueness of hostname and IP address
		// --------------------------------------------------------------------
		printf("\tChecking host uniqueness... ");
		prev_size = seen_hosts.size();
		seen_hosts.insert(parsed_url.host);
		if (seen_hosts.size() <= prev_size)
		{
			printf("failed\n");
			continue;
		}
		printf("passed\n");

		printf("\tDoing DNS... ");
		DWORD IP = crawler.ResolveDNS();
		if (IP == NULL)
			continue;

		printf("\tChecking IP uniqueness... ");
		prev_size = seen_ips.size();
		seen_ips.insert(IP);
		if (seen_ips.size() <= prev_size)
		{
			printf("failed\n");
			continue;
		}
		printf("passed\n");

		// check /robots.txt
		// --------------------------------------------------------------------------
		printf("\tConnecting on robots... ");
		if (crawler.CreateConnection() < 0)
			continue;

		if (crawler.Write("HEAD", "/robots.txt") < 0)
			continue;

		printf("\tLoading... ");
		if (crawler.Read(buffer, MAX_ROBOTS_SIZE, cur_buf_size, allocated_size) < 0)
			continue;

		printf("\tVerifying Header... ");
		if (!crawler.VerifyHeader(buffer, 400, 499))
			continue;

		// connect to page
		// --------------------------------------------------------------------
		crawler.ResetConnection();

		printf("      * Connecting to page... ");
		if (crawler.CreateConnection() < 0)
			continue;

		if (crawler.Write("GET") < 0)
			continue;

		printf("\tLoading... ");
		if (crawler.Read(buffer, MAX_PAGE_SIZE, cur_buf_size, allocated_size) < 0)
			continue;

		printf("\tVerifying header... ");
		if (!crawler.VerifyHeader(buffer, 200, 299)) 
			continue;

		printf("      + Parsing page... ");
		crawler.Parse(buffer, cur_buf_size, false);
	}

	if (buffer != NULL)
		free(buffer);

	free(url);
	return(EXIT_SUCCESS);
}

/*
 * Function: main
 * ------------------
 * Simple driver function to validate command line arguments and call CrawlUrls
 * expects two command line arguments: number of threads and input file
 *
 * input:
 *   - argc: count of command line arguments
 *   - argv: array of strings with three elements ["hw1p2.exe", "<number of threads>", "<input file>"]
 *
 * return: an status code that will be 1 in the case that an error is encountered,
 *         or 0 for successful execution
 */
int main(int argc, char** argv)
{
	// debug flag to check for memory leaks
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); 
	
	FILE* file = NULL;
	int num_threads = 0;
	unordered_set<DWORD> seen_ips;
	unordered_set<string> seen_hosts;
	
	// make sure command line arguments are valid
	if (argc < NUM_ARGS || argc > NUM_ARGS)
	{  
		(argc < NUM_ARGS) ? printf("too few arguments") : printf("too many arguments");
		printf("\nusage: hw1p2.exe 1 <filename>\n");
		return(EXIT_FAILURE);
	}
	
	// convert the number of threads to an int and validate its range
	try
	{
		num_threads = stoi(argv[1]);
		if (num_threads < MIN_NUM_THREADS || num_threads > MAX_NUM_THREADS)
			throw out_of_range("");
	}
	catch (...)
	{ 
		printf("supplied argument for thread count invalid: %s\n", argv[1]);
		return(EXIT_FAILURE);
	}

	// open input file for reading
	if(fopen_s(&file, argv[2], "rb") != 0)
	{
		printf("%s could not be opened for reading\n", argv[2]);
		return(EXIT_FAILURE);
	}

	if (file != NULL)
	{
		fseek(file, NULL, SEEK_END);
		printf("Opened %s with size %u", argv[2], ftell(file));
		rewind(file);
	}

	// start crawling URLs
	int ret = CrawlUrls(seen_ips, seen_hosts, file);
	if (ret < 0)
	{
		return(EXIT_FAILURE);
	}

	// clean up by closing file
	if (file)
	{
		if (fclose(file) != 0)
		{
			printf("file could not be closed\n");
			return(EXIT_FAILURE);
		}
	}

	return 0;
}
