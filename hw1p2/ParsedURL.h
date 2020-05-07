// ParsedURL.h
// CSCE 463-500
// Luke Grammer
// 9/10/19

#pragma once

const unsigned MAX_PORT = 65535;
const unsigned MIN_PORT = 1;
const unsigned DEFAULT_PORT = 80;

// Basic struct for containing a URL
struct ParsedURL
{   
	// To determine is a given URL is properly formed
	bool valid; 
	int port;
	std::string scheme, host, query, path, request;

	// Basic default constructor
	ParsedURL() : scheme{ "" }, host{ "" }, port{ DEFAULT_PORT }, query{ "" }, path{ "/" }, request{ "" }, valid{ false } {} 

	/* 
	 * input:
	 *   - url: a string in URL format (<scheme>://<host>[:<port>][/<path>][?<query>][#<fragment>])
	 * 
	 * return: a ParsedURL object with data members initialized to the parsed contents of url, 
	 *         if the URL could not be successfully parsed, returned object's 'valid' member will
	 *         be false.
	 */
	static ParsedURL ParseUrl(std::string url); 
};