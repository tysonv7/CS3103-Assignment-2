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
#include <thread>
#include <mutex>

using namespace std;

static mutex updateNumOfWebsitesVisitGuard;
static mutex checkNumOfThreadsGuard;
static mutex updateVisitedURLGuard;
static mutex printGuard;

const int maxMessageReceiveSize = 1024;
set<string> visitedURL;
set<pair<string, float>>visitedVector;
vector<thread> threadVector;

const int maxNumOfWebsitesToVisit = 50;
const int recursionDepthLimit = 3;
const int maxNumOfThreads = 5;

int numOfWebsitesVisited = 0;

queue<pair<string, string>> websiteQueue;

ofstream outfile("output.txt");

void killAllThreads(){
	for (auto &x : threadVector) {
		x.join();
	}
}

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
				updateVisitedURLGuard.try_lock();
				if(visitedURL.find(url) == visitedURL.end()) {
					websiteQueue.push(make_pair(hostname, page));
					visitedURL.insert(url);
					
					updateVisitedURLGuard.unlock();
				}
			}
		} else {
			string hostname = originalHost;
			size_t found = hostname.find_first_of("/");
			if (found == string::npos) {
				url = hostname;
				updateVisitedURLGuard.try_lock();
				if(visitedURL.find(url) == visitedURL.end()) {
					websiteQueue.push(make_pair(hostname, ""));
					visitedURL.insert(url);
					updateVisitedURLGuard.unlock();
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
			updateVisitedURLGuard.try_lock();
			if (found == string::npos) {
				url = host + "/";
				if(visitedURL.find(url) == visitedURL.end()) {
					websiteQueue.push(make_pair(host,"/"));
					visitedURL.insert(url);
					updateVisitedURLGuard.unlock();
				}
				
			}
		} else {
			size_t found = host.find_first_of("/");
			updateVisitedURLGuard.try_lock();
			if (found == string::npos) {
				url = host+href;
				if(visitedURL.find(url) == visitedURL.end()) {
					websiteQueue.push(make_pair(host,href));
					visitedURL.insert(url);
					updateVisitedURLGuard.unlock();
				}
			}
		}
	}
}

//Generate the HTTP requests
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

int connectToURL(string website, string path, int recursionDepth) {
	//When the current thread exceeds it's the recursion limits, it terminates
	if (recursionDepth >= recursionDepthLimit) {
		killAllThreads();
		//return 0;
	} 
	
	int sock;
	struct hostent* host;
	struct sockaddr_in server_addr;
	host = gethostbyname(website.c_str());
	if (host == NULL) {
		//Expelling the problematic website
		websiteQueue.pop();
		
		//Fetching the inputs for the next website to crawl
		string hostname = websiteQueue.front().first;
		string page = websiteQueue.front().second;
		
		//Pause for 3 milliseconds before making a new thread
		this_thread::sleep_for(chrono::milliseconds(3));
		
		//Making a new thread to crawl
		std::thread t1(connectToURL,hostname,page,recursionDepth);
		t1.join();
		threadVector.push_back(move(t1));
	}
	
	cout << "Try to visit: " << website+path << endl;
	//Creating a socket Structure
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		cout << "Error in socket creation" << endl;
		
		//Expelling the problematic website
		websiteQueue.pop();
		
		//Fetching the inputs for the next website to crawl
		string hostname = websiteQueue.front().first;
		string page = websiteQueue.front().second;
		
		//Pause for 3 milliseconds before making a new thread
		this_thread::sleep_for(chrono::milliseconds(3));
		
		//Making a new thread to crawl
		std::thread t2(connectToURL, hostname, page, recursionDepth);
		t2.join();
		threadVector.push_back(move(t2));
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(80);
	server_addr.sin_addr = *((struct in_addr *)host -> h_addr);
	bzero(&(server_addr.sin_zero), 8);
	
	//Connect to HTTP Server Port 80
	if (connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		cout << "Error in connecting to HTTP Port 80" << endl;
		
		//Expelling the problematic website
		websiteQueue.pop();
		
		//Fetching the inputs for the next website to crawl
		string hostname = websiteQueue.front().first;
		string page = websiteQueue.front().second;
		
		//Pause for 3 milliseconds before making a new thread
		this_thread::sleep_for(chrono::milliseconds(3));
		
		std::thread t3(connectToURL, hostname, page, recursionDepth);
		t3.join();
		threadVector.push_back(move(t3));
	}

	cout << "Connected to " << inet_ntoa(server_addr.sin_addr) << " " << ntohs(server_addr.sin_port) << endl;
	
	//Generates the HTTP Request message
	string requestMessage = generateHttpRequest(website, path);
	
	//Start the timer of how long the page take to respond
	int status = send(sock, requestMessage.c_str(), requestMessage.size(), 0);
	auto start = chrono::high_resolution_clock::now();
	
	char buf[maxMessageReceiveSize];
	
	string messageReceived = "";
	
	if (status !=0) {
		//Stop the timer
		auto end = chrono::high_resolution_clock::now();
		float elapsedTime = chrono::duration_cast<chrono::nanoseconds> (end - start).count();
		
		outfile << "Base URL: " << website+path << endl;
		outfile << "Time Taken: " << elapsedTime << " nanoseconds" << endl << endl;
		//Begin to download the data from the webpage
		while (status !=0) {
			
			//Sets a limit of how much to download from a page. 
			if (messageReceived.length() > 65536) {
				break;
			}
			
			memset(buf, 0, maxMessageReceiveSize);
			status = recv(sock, buf, maxMessageReceiveSize, 0);
			messageReceived.append(buf);
		}
	}
	
	//Update the number of websites count
	updateNumOfWebsitesVisitGuard.try_lock();
	numOfWebsitesVisited++;
	updateNumOfWebsitesVisitGuard.unlock();
	
	//Terminate threads if visited the maximum number of websites
	if(numOfWebsitesVisited == maxNumOfWebsitesToVisit) {
		//killAllThreads();
		outfile.close();
		return 0;
	} else {
		//Expelled the crawled website
		websiteQueue.pop();
		
		//Trying to parse the current website crawled
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
			
			//If no more website to clear, terminate the thread
			if (websiteQueue.empty()) {
				return 0;
			}
			
			//Fetching the inputs for the next website to crawl
			string hostname = websiteQueue.front().first;
			string page = websiteQueue.front().second;
			
			//Pause for 3 milliseconds before making a new thread
			this_thread::sleep_for(chrono::milliseconds(3));
			
			//Making a new thread to crawl
			std::thread t4(connectToURL, hostname, page,recursionDepth+1);
			t4.join();
			threadVector.push_back(move(t4));
			
		} catch (boost::regex_error &e) {
			cout << "Error: " << e.what() << endl;
		}
		//When the current thread exceeds it's the recursion limits, it terminates
		if (recursionDepth >= recursionDepthLimit) {
			return 0;
		} else {
			//killAllThreads();
		}
	}
}

int main(int argc, char* argv[]) {
	string seed(argv[1]);
	websiteQueue.push(make_pair(seed,"/"));
	connectToURL(seed, "/", 0);
	cout << "Crawling done. Results printed" << endl;
	exit(0);
}
