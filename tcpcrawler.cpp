#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <chrono> //For timing purposes
#include <vector>
#include <fstream>
#include <queue>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <utility>
#include <cstddef>
#include <thread>
#include <ctime>
#include <time.h>
#include <pthread.h>

using namespace std;

const int maxMessageReceiveSize = 140 * 1024;
set<string> visitedURL;
set<pair<string, float>>visitedVector;

const int maxNumOfWebsitesToVisit = 50;
const int recursionDepthLimit = 3;

int numOfWebsitesVisited = 0;

queue<pair<string, string>> websiteQueue;

string parseHttp(string str) {
	const boost::regex re("(?i)http://(.*)/?(.*)");
    boost::smatch what;
    if (boost::regex_match(str, what, re)) {
        std::string hst = what[1];
        boost::algorithm::to_lower(hst);
        return hst;
    }
    return "";
}

void parseHref(string originalHost, string str) {
	const boost::regex re("(?i)http://(.*)/(.*)");
        boost::smatch what;
        string url;
        if (boost::regex_match(str, what, re)) {
            // We found a full URL, parse out the 'hostname'
            // Then parse out the page
            string hostname = what[1];
            boost::algorithm::to_lower(hostname);
            string page = what[2];
			size_t found = hostname.find_first_of("/");
			if (found == string::npos) {
				url = hostname+page;
				if(visitedURL.find(url) == visitedURL.end()) {
					websiteQueue.push(make_pair(hostname, page));
					visitedURL.insert(url);
				}
			}
		} else {
			string hostname = originalHost;
			size_t found = hostname.find_first_of("/");
			if (found == string::npos) {
				url = hostname;
				if(visitedURL.find(url) == visitedURL.end()) {
					websiteQueue.push(make_pair(hostname, ""));
					visitedURL.insert(url);
				}
				
			}
		}
}

void parse(string host, string href) {
	string hst = parseHttp(href);
	string url;
	if (!hst.empty()) {
		//If we have a HTTP prefix, we could end up with a 'hostname' and page
		parseHref(hst, href);
	} else {
		if(href =="") {
			
			size_t found = host.find_first_of("/");
			if (found == string::npos) {
				url = host + "/";
				if(visitedURL.find(url) == visitedURL.end()) {
					websiteQueue.push(make_pair(host,"/"));
					visitedURL.insert(url);
				}
				
			}
		} else {
			size_t found = host.find_first_of("/");
			if (found == string::npos) {
				url = host+href;
				if(visitedURL.find(url) == visitedURL.end()) {
					websiteQueue.push(make_pair(host,href));
					visitedURL.insert(url);
				}
			}
		}
	}
}

void writeResults() {
	ofstream outfile("output.txt");
	//To check the visited array
	for (auto &x:visitedVector) {
		outfile << "Base URL: " << x.first << endl;
		outfile << "Time Taken: " << x.second << " nanoseconds" << endl << endl;
	}
	outfile.close();
}

string generateHttpRequest(string host, string path) {
	string requestMessage = "GET ";
	requestMessage.append(path);
	requestMessage.append(" HTTP/1.1\r\n");
	requestMessage.append("Host: ");
	requestMessage.append(host);
	requestMessage.append("\r\n");
	requestMessage.append("Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.81\r\n");
	requestMessage.append("User-Agent: Mozilla/5.0 (compatible; tysonv7bot/1.0; https://github.com/tysonv7/CS3103-Assignment-2/)\r\n");
	requestMessage.append("Connection: close\r\n\r\n");
	return requestMessage;
}

int connect(string website, string path) {
	int sock;
	struct hostent* host;
	struct sockaddr_in server_addr;
	host = gethostbyname(website.c_str());
	if (host == NULL) {
		websiteQueue.pop();
		string hostname = websiteQueue.front().first;
		string page = websiteQueue.front().second;
		this_thread::sleep_for(chrono::milliseconds(3));
		connect(hostname, page);
	}
	
	cout << "Try to visit: " << website << endl;
	//Creating a socket Structure
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		cout << "Error in socket creation" << endl;
		websiteQueue.pop();
		string hostname = websiteQueue.front().first;
		string page = websiteQueue.front().second;
		this_thread::sleep_for(chrono::milliseconds(3));
		connect(hostname, page);
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(80);
	server_addr.sin_addr = *((struct in_addr *)host -> h_addr);
	bzero(&(server_addr.sin_zero), 8);
	cout << "Trying to establish connection to port 80" << endl;
	//Connect to HTTP Server Port 80
	if (connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		cout << "Error in connecting to HTTP Port 80" << endl;
		websiteQueue.pop();
		string hostname = websiteQueue.front().first;
		string page = websiteQueue.front().second;
		this_thread::sleep_for(chrono::milliseconds(3));
		connect(hostname, page);
	}

	cout << "Connected to " << inet_ntoa(server_addr.sin_addr) << " " << ntohs(server_addr.sin_port) << endl;
	
	string requestMessage = generateHttpRequest(website, path);
	
	int status = send(sock, requestMessage.c_str(), requestMessage.size(), 0);
	auto start = chrono::high_resolution_clock::now();
	
	char buf[maxMessageReceiveSize];
	
	string messageReceived = "";
	
	if (status !=0) {
		auto end = chrono::high_resolution_clock::now();
		float elapsedTime = chrono::duration_cast<chrono::nanoseconds> (end - start).count();
		visitedVector.insert(make_pair(website, elapsedTime));
		while (status !=0) {
			memset(buf, 0, maxMessageReceiveSize);
			status = recv(sock, buf, maxMessageReceiveSize, 0);
			messageReceived.append(buf);
		}
	}
	numOfWebsitesVisited++;
	cout << "Number of websites visited: " << numOfWebsitesVisited << endl;
	
	if(numOfWebsitesVisited == maxNumOfWebsitesToVisit) {
		return 0;
	} else {
		websiteQueue.pop();
		try {
			const boost::regex rmv_all("[\\r|\\n]");
			const string s2 = boost::regex_replace(messageReceived, rmv_all, "");
			const string s = s2;
			
			//Use this regex expression to find for anchor tag but not '>' where (.+? match anything)
			const boost::regex re("<a\\s+href\\s*=\\s*(\"([^\"]*)\")|('([^']*)')\\s*>");
			boost::cmatch matches;
			// Using token iterator with sub-matches
			const int subs[] = {2,4};
			boost::sregex_token_iterator i(s.begin(), s.end(), re, subs);
			boost::sregex_token_iterator j;
			for(; i!=j; i++) {
				//Iterate through the listed Hrefs and move to the next request
				const string href = *i;
				if (href.length() != 0) {
					parse(website,href);
				}
			}
			string hostname = websiteQueue.front().first;
			string page = websiteQueue.front().second;
			this_thread::sleep_for(chrono::milliseconds(3));
			connect(hostname, page);
		} catch (boost::regex_error &e) {
			cout << "Error: " << e.what() << endl;
		}
		return 1;
	}
}

int main(int argc, char* argv[]) {
	string seed(argv[1]);
	websiteQueue.push(make_pair(seed,"/"));
	connect(seed, "/");
	writeResults();
}
