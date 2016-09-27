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

const int maxNumOfWebsitesToVisit = 75;
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
	cout << "Checking recursion depth at the start" << endl;
	if (recursionDepth >= recursionDepthLimit) {
		thread::id this_id = std::this_thread::get_id();
		printGuard.try_lock();
		cout << "Child thread going to die: " << this_id << endl;
		cout << "Number of websites visited: " << numOfWebsitesVisited << endl;
		printGuard.unlock();
		killAllThreads();
		//return 0;
	} 
	
	int sock;
	struct hostent* host;
	struct sockaddr_in server_addr;
	host = gethostbyname(website.c_str());
	if (host == NULL) {
		websiteQueue.pop();
		string hostname = websiteQueue.front().first;
		string page = websiteQueue.front().second;
		this_thread::sleep_for(chrono::milliseconds(3));
		//connect(hostname, page);
		std::thread t1(connectToURL,hostname,page,recursionDepth);
		t1.join();
		threadVector.push_back(move(t1));
	}
	
	cout << "Try to visit: " << website+path << endl;
	//Creating a socket Structure
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		cout << "Error in socket creation" << endl;
		websiteQueue.pop();
		string hostname = websiteQueue.front().first;
		string page = websiteQueue.front().second;
		this_thread::sleep_for(chrono::milliseconds(3));
		//connect(hostname, page);
		std::thread t2(connectToURL, hostname, page, recursionDepth);
		t2.join();
		threadVector.push_back(move(t2));
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
		//connect(hostname, page);
		std::thread t3(connectToURL, hostname, page, recursionDepth);
		t3.join();
		threadVector.push_back(move(t3));
	}

	cout << "Connected to " << inet_ntoa(server_addr.sin_addr) << " " << ntohs(server_addr.sin_port) << endl;
	
	string requestMessage = generateHttpRequest(website, path);
	cout << "Generated HTTP Request" << endl;
	
	cout<< "Receiving data now" << endl;
	int status = send(sock, requestMessage.c_str(), requestMessage.size(), 0);
	auto start = chrono::high_resolution_clock::now();
	
	char buf[maxMessageReceiveSize];
	
	string messageReceived = "";
	
	if (status !=0) {
		auto end = chrono::high_resolution_clock::now();
		float elapsedTime = chrono::duration_cast<chrono::nanoseconds> (end - start).count();
		//visitedVector.insert(make_pair(website+path, elapsedTime));
		outfile << "Base URL: " << website+path << endl;
		outfile << "Time Taken: " << elapsedTime << " nanoseconds" << endl << endl;
		while (status !=0) {
			if (messageReceived.length() > 32768) {
				break;
			}
			memset(buf, 0, maxMessageReceiveSize);
			status = recv(sock, buf, maxMessageReceiveSize, 0);
			messageReceived.append(buf);
		}
	}
	cout <<"Updating number of websites" << endl;
	updateNumOfWebsitesVisitGuard.try_lock();
	numOfWebsitesVisited++;
	updateNumOfWebsitesVisitGuard.unlock();
	//cout << "Number of websites visited: " << numOfWebsitesVisited << endl;
	
	cout << "Checking if number of website visited is enough" << endl;
	if(numOfWebsitesVisited == maxNumOfWebsitesToVisit) {
		thread::id this_id = std::this_thread::get_id();
		cout << "Thread returning: " << this_id << endl;
		//killAllThreads();
		outfile.close();
		return 0;
	} else {
		cout << "Regex magic" << endl;
		websiteQueue.pop();
		try {
			cout <<"First regex" << endl;
			const boost::regex rmv_all("[\\r|\\n]");
			const string s2 = boost::regex_replace(messageReceived, rmv_all, "");
			const string s = s2;
			cout<<"Second regex" << endl;
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
			cout << "Checking if there's bullet to shoot" << endl;
			if (websiteQueue.empty()) {
				thread::id this_id = std::this_thread::get_id();
				printGuard.try_lock();
				cout << "Child thread going to die: " << this_id << endl;
				cout << "Number of websites visited: " << numOfWebsitesVisited << endl;
				printGuard.unlock();
				return 0;
			}
			cout << "Preparing new child" << endl;
			string hostname = websiteQueue.front().first;
			string page = websiteQueue.front().second;
			this_thread::sleep_for(chrono::milliseconds(3));
			cout << "New child problem" << endl;
			//connect(connectToURL, page);
			std::thread t4(connectToURL, hostname, page,recursionDepth+1);
			t4.join();
			threadVector.push_back(move(t4));
			
		} catch (boost::regex_error &e) {
			cout << "Error: " << e.what() << endl;
		}
		cout << "Checking recursion depth at the end" << endl;
		if (recursionDepth >= recursionDepthLimit) {
			thread::id this_id = std::this_thread::get_id();
			cout << "Thread returning: " << this_id << endl;
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
	//std::thread start(connectToURL, seed, "/");
	thread::id this_id = std::this_thread::get_id();
	cout << "Main thread going to die: " << this_id << endl;
}
