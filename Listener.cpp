/*
 * Listener.cpp
 *
 *  Created on: Nov 2, 2015
 *      Author: lizhen
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <stdexcept>
#include <typeinfo>
#include <string>
#include <algorithm>
#include <iostream>

#include "Util.h"
#include "Packet.h"
#include "G04.h"
#include "Listener.h"

#define	SA	struct sockaddr

std::string SeedInfo::toString() {
	char tmp[MAXLINE];
	sprintf(tmp, "(0x%x) %s:%d", ip, Util::itoaddress(ip).c_str(), port);
	return tmp;
}

std::string PeerInfo::toString() {
	char tmp[MAXLINE];
	sprintf(tmp, "(0x%x) %s:%d, sockfd=%d", ip, Util::itoaddress(ip).c_str(),
			port, sockfd);
	return tmp;
}
std::string PeerInfo::address() {
	return Util::itoaddress(ip);
}

void FileInfo::addKeyword(std::string kw) {
	keywords.push_back(kw);
}
bool FileInfo::match(std::string kw) {
	std::string s = Util::toUpper(kw);
	for (size_t i = 0; i < keywords.size(); i++) {
		std::string thiskw = Util::toUpper(keywords[i]);
		if (thiskw.find(s) != std::string::npos) {
			return true;
		}
	}
	return false;
}
std::string FileInfo::toString() {
	char tmpstr[MAXLINE];
	sprintf(tmpstr, "%d %s (%lu bytes): ", this->file_index, file_name.c_str(),
			size);
	std::string ret = tmpstr;
	for (size_t i = 0; i < keywords.size(); i++) {
		ret += keywords[i] + "|";
	}
	return ret;
}

std::string QueryResultInfo::toString() {
	char tmpstr[MAXLINE];
	sprintf(tmpstr, "%x %s %d %d %s %d", ip, Util::itoaddress(ip).c_str(), port,
			qr.file_index, qr.file_name.c_str(), qr.file_size);
	return tmpstr;
}

DownloadRequest::DownloadRequest() {
	bzero(this, sizeof(DownloadRequest));
	fileindex = 0;
	strcpy(httpversion, "1.0");
	strcpy(agentname, "Gnutella");
	strcpy(agentversion, "0.4");
}

std::string DownloadRequest::toString() {
	std::string ret = "";
	char tmpstr[MAXLINE];
	bzero(tmpstr, MAXLINE);
	sprintf(tmpstr, "GET /get/%d/%s/ %s/%s\r\nUser-Agent: %s/%s\r\n",
			this->fileindex, this->filename, "HTTP", httpversion, agentname,
			agentversion);
	ret += tmpstr;
	if (strlen(range) > 0) {
		ret = ret + "Range: " + range + "\r\n";
	}
	if (strlen(connection) > 0) {
		ret = ret + "Connection: " + connection + "\r\n";
	}
	ret += "\r\n";
	return ret;
}

int DownloadRequest::parseFromString(const std::string& s) {
	std::vector<std::string> vec;
	size_t start_pos = 0;
	for (size_t i = 0; i < s.length(); i++) {
		if (s[i] == '\n' && i > 0 && s[i - 1] == '\r') {
			std::string substr = s.substr(start_pos, i - 1 - start_pos);
			start_pos = i + 1;
			vec.push_back(substr);
		}
	}
	if (vec.size() < 3 || (vec.size() > 0 && !vec[vec.size() - 1].empty())) {
		Util::log_error("Parse message failed (split=%ld):%s\n", vec.size(),
				s.c_str());
		return -1;
	}

	bzero(this, sizeof(DownloadRequest));
	int ncount = 0;
	if ((ncount = sscanf(vec[0].c_str(), "GET /get/%d/%s HTTP/%s", &fileindex,
			filename, httpversion)) != 3
			|| filename[strlen(filename) - 1] != '/') {
		Util::log_error("Parse line1 failed(%d):%s\n", ncount, vec[0].c_str());
		return -1;
	}
	filename[strlen(filename) - 1] = 0x00;
	std::string line2 = vec[1];
	std::replace(line2.begin(), line2.end(), '/', ' ');
	if ((ncount = sscanf(line2.c_str(), "User-Agent: %s %s", agentname,
			agentversion)) != 2) {
		Util::log_error("Parse line2 failed(%d):%s\n", ncount, vec[1].c_str());
		return -1;
	}
	return 0;
}

DownloadResponse::DownloadResponse() {
	bzero(this, sizeof(DownloadResponse));
	contentlength = 0;
	part_binary_len = 0;
	strcpy(httpversion, "1.0");
	strcpy(retcode, "200");
	strcpy(retdesc, "OK");
	strcpy(servername, "Gnutella/0.4");
	strcpy(contenttype, "application/binary");
}

std::string DownloadResponse::toString() {
	std::string ret = "";
	char tmpstr[MAXLINE];

	bzero(tmpstr, MAXLINE);
	sprintf(tmpstr, "HTTP/%s %s %s\r\n", httpversion, retcode, retdesc);
	ret += tmpstr;

	bzero(tmpstr, MAXLINE);
	sprintf(tmpstr, "Server: %s\r\n", servername);
	ret += tmpstr;

	bzero(tmpstr, MAXLINE);
	sprintf(tmpstr, "Content-Type: %s\r\n", contenttype);
	ret += tmpstr;

	bzero(tmpstr, MAXLINE);
	sprintf(tmpstr, "Content-Length: %lu\r\n", contentlength);
	ret += tmpstr;

	ret += "\r\n";
	for (size_t i = 0; i < part_binary_len; i++) {
		ret.push_back(part_binary[i]);
	}
	return ret;
}

int DownloadResponse::parseFromString(const std::string& fulls) {
	size_t head_end_pos = -1;
	for (size_t i = 0; i < fulls.length(); i++) {
		if (i > 3 && fulls[i] == '\n' && fulls[i - 1] == '\r'
				&& fulls[i - 2] == '\n' && fulls[i - 3] == '\r') {
			head_end_pos = i;
			break;
		}
	}
	if (head_end_pos < 0) {
		Util::log_error("Parse message failed, \\r\\n\\r\\n not found.%s\n",
				fulls.c_str());
		return -1;
	}

	bzero(this, sizeof(DownloadResponse));
	this->part_binary_len = 0;
	for (size_t i = head_end_pos + 1; i < fulls.length(); i++) {
		this->part_binary[i - head_end_pos - 1] = fulls[i];
		this->part_binary_len++;
	}

	std::string s = fulls.substr(0, head_end_pos + 1);
	std::vector<std::string> vec;
	size_t start_pos = 0;
	for (size_t i = 0; i < s.length(); i++) {
		if (s[i] == '\n' && i > 0 && s[i - 1] == '\r') {
			std::string substr = s.substr(start_pos, i - 1 - start_pos);
			start_pos = i + 1;
			vec.push_back(substr);
		}
	}
	if (vec.size() != 5 || (vec.size() > 0 && !vec[vec.size() - 1].empty())) {
		Util::log_error("Parse message failed (split=%ld):%s\n", vec.size(),
				s.c_str());
		return -1;
	}

	int ncount = 0;
	if ((ncount = sscanf(vec[0].c_str(), "HTTP/%s %s %[0-9a-zA-Z ]",
			httpversion, retcode, retdesc)) != 3) {
		Util::log_error("Parse line1 failed(%d):%s\n", ncount, vec[0].c_str());
		return -1;
	}

	if ((ncount = sscanf(vec[1].c_str(), "Server: %s", servername)) != 1) {
		Util::log_error("Parse line2 failed(%d):%s\n", ncount, vec[1].c_str());
		return -1;
	}

	if ((ncount = sscanf(vec[2].c_str(), "Content-Type: %s", contenttype))
			!= 1) {
		Util::log_error("Parse line3 failed(%d):%s\n", ncount, vec[2].c_str());
		return -1;
	}

	if ((ncount = sscanf(vec[3].c_str(), "Content-Length: %lu", &contentlength))
			!= 1) {
		Util::log_error("Parse line4 failed(%d):%s\n", ncount, vec[3].c_str());
		return -1;
	}

	return 0;
}

Listener::Listener(G04* g04) :
		neighborfd(-1), filefd(-1), bStop(false), hasStoped(false), maxfd(0), hostip(
				0) {
	this->conf = g04->conf;

}

Listener::~Listener() {
	if (neighborfd > 0)
		close(neighborfd);
	if (filefd > 0)
		close(filefd);
}

bool Listener::has_stopped() {
	return hasStoped;
}

bool Listener::going_stop() {
	return bStop;
}

void Listener::going_stop(bool b) {
	bStop = b;
}

int Listener::startlisten() {
	while (!this->going_stop()) {
		fd_set rset;
		rset = this->fdset;
		/* Wait up to 2 seconds. */
		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		int nready = select(this->maxfd + 1, &rset, NULL,
		NULL, &tv);
		if (nready == -1) {
			Util::log_error("select returns error %d\n", nready);
		} else if (nready) {

			if (FD_ISSET(neighborfd, &rset)) { /* new client connection */
				this->handle_new_peer();
				nready--;
			}

			if (FD_ISSET(filefd, &rset)) { /* request for file */
				this->handle_file_download_request();
				nready--;
			}

			if (nready <= 0) {
				continue;
			}

			this->handle_peer_select(rset, nready);

		} else { //timeout
			//Util::log_debug("no message\n");
			handle_timer_task();
		}
	}
	close(neighborfd);
	close(filefd);
	for (std::map<int, PeerInfo>::iterator i = peers.begin(); i != peers.end();
			i++) {
		close(i->first);
	}
	hasStoped = true;
	return 0;
}

void* peer_download_thread_handler(void* arg) {
	DownloadRequestArgs *dra = (DownloadRequestArgs*) arg;
	dra->listener->handle_file_download(dra->sockfd, dra->dr);
	Util::log_debug("Thread finished to handle peer download request.\n");
	pthread_exit(NULL);
}

int Listener::handle_file_download_by_thread(int sockfd, DownloadRequest dr) {
	Util::log_debug("Starting thread to handle peer download request.\n");
	pthread_t thread;
	pthread_attr_t attr;
	/* For portability, explicitly create threads in a joinable state */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	DownloadRequestArgs *dra = new DownloadRequestArgs();
	dra->sockfd = sockfd;
	dra->dr = dr;
	dra->listener = this;
	pthread_create(&thread, &attr, peer_download_thread_handler, (void *) dra);
	pthread_attr_destroy(&attr);
	return 0;
}

int Listener::handle_file_download_request() {
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	int connfd = accept(filefd, (SA *) &cliaddr, &clilen);

	char str[INET6_ADDRSTRLEN];
	Util::log_debug("Download from peer: %s, port %d\n",
			inet_ntop(AF_INET, &cliaddr.sin_addr, str, INET6_ADDRSTRLEN),
			ntohs(cliaddr.sin_port));

	std::string buf;
	size_t nread = Util::readed(connfd, buf);
	if (nread < 0) {
		Util::log_error("Error to read from peer.\n");
		close(connfd);
		return -1; //ignore the request
	}

	DownloadRequest dr;
	if (dr.parseFromString(buf) < 0) {
		Util::log_error("Parse download request failed:%s.\n", buf.c_str());
		close(connfd);
		return -1; //ignore the request
	}

	Util::log_debug("Peer requested to download file %d: %s\n", dr.fileindex,
			dr.filename);

	this->handle_file_download_by_thread(dup(connfd), dr);

	close(connfd);
	Util::log_debug("Finished peer download request.\n");
	return 0;
}

int Listener::handle_file_download(int sockfd, DownloadRequest dr) {
	DownloadResponse resp;
	FileInfo fi = localFiles[dr.fileindex];
	if (fi.file_name != dr.filename) {
		strcpy(resp.retcode, "410");
		strcpy(resp.retdesc, "Gone");
		ssize_t nwriten = Util::writen(sockfd, resp.toString());
		if (nwriten < 0) {
			Util::log_error("reply download message failed\n");
		}
		close(sockfd);
		return -1;

	}
	std::string filepath = conf.localFilesDirectory + "/" + dr.filename;
	FILE* localfileptr = fopen(filepath.c_str(), "rb");

	if (!localfileptr) {
		strcpy(resp.retcode, "404");
		strcpy(resp.retdesc, "Not Found");
		ssize_t nwriten = Util::writen(sockfd, resp.toString());
		if (nwriten < 0) {
			Util::log_error("reply download message failed\n");
		}
		close(sockfd);
		fclose(localfileptr);
		return -1;
	}

	strcpy(resp.retcode, "200");
	strcpy(resp.retdesc, "OK");
	resp.contentlength = fi.size;
	ssize_t nwriten = Util::writen(sockfd, resp.toString());
	if (nwriten < 0) {
		Util::log_error("reply download message failed\n");
		close(sockfd);
		fclose(localfileptr);
		return -1;
	}

	size_t nwrite = -1;
	char tmpbuf[MAXLINE];
	while (!feof(localfileptr)) {
		size_t nread = fread(tmpbuf, sizeof(char), MAXLINE, localfileptr);
		if (nread != MAXLINE && ferror(localfileptr)) {
			Util::log_error("Read file content from disk failed\n");
			close(sockfd);
			fclose(localfileptr);
			return -1;
		}
		nwrite = Util::writen(sockfd, tmpbuf, nread);
		if (nwrite < 0) {
			Util::log_error("Write file content to socket failed\n");
			close(sockfd);
			fclose(localfileptr);
			return -1;

		}
	}

	close(sockfd);
	fclose(localfileptr);
	Util::log_debug("Finished peer download request.\n");
	return 0;
}

size_t Listener::read_one_packet(int sockfd, std::string& buf) {
	size_t n;
	std::string buf1;
	if ((n = Util::read_once(sockfd, buf1, Packet::HEADSIZE))
			!= Packet::HEADSIZE) {
		if (n == 0) {
			this->erasePeer(sockfd);
			return 0;
		} else {
			Util::log_error("Read packet head message failed for %d: %s\n",
					sockfd, buf.c_str());
		}
		return -1;
	}
	size_t pl = Packet::string2uint(buf1, 19);

	std::string buf2;
	if ((n = Util::read_once(sockfd, buf2, pl)) != pl) {
		if (n == 0) {
			this->erasePeer(sockfd);
			return 0;
		} else {
			Util::log_error("Read packet head message failed for %d: %s\n",
					sockfd, buf.c_str());
		}

		return -1;
	}
	buf = buf1 + buf2;
	return buf.size();
}

int Listener::cleanHist() {
	static int STALE_TIME = 60;
	time_t now = time(NULL);
	std::map<std::string, MsgInfo>::iterator iter;
	std::map<std::string, MsgInfo>* array[] = { &myPingHist, &pingHist,
			&pongHist, &queryHist, &queryhitsHist, NULL };

	std::map<std::string, MsgInfo>* p;
	int i = 0;
	while ((p = array[i++])) {
		std::map<std::string, MsgInfo> &mymap = *p;
		for (iter = mymap.begin(); iter != mymap.end();) {
			if (iter->second.ts + STALE_TIME < now) {
				iter = mymap.erase(iter);
			} else {
				iter++;
			}
		}
	}
	return 0;
}

int Listener::handle_timer_task() {
	time_t now = time(0);
	for (size_t i = 0; i < this->timeTaskQueue.size(); i++) {
		TimerTask& tt = timeTaskQueue[i];
		if (tt.last_exec + tt.inteveral < now) {
			if (tt.name == "ping") {
				//Util::log_debug("handle ping timer task\n");
				if (this->peers.empty() || conf.isSeedNode) {
					this->connect_seeds();
				} else if (!this->peerFull()) {
					if (!conf.isSeedNode) { //only non seed node actively ping peers
						this->ping_peers();
					}
				} else if (this->peerFull()) {
					if (!conf.isSeedNode) {
						//this->try_remove_one_seed();
					}
				}
			} else if (tt.name == "clean_hist") {
				//Util::log_debug(
				//		"handle cleaning historical records timer task\n");
				this->cleanHist();
			}
			tt.last_exec = now;
		}
	}
	return 0;
}

int Listener::handle_peer_select(fd_set& rset, int msgcnt) {
	std::map<int, PeerInfo>::iterator iter;
	int nready = msgcnt;
	std::map<int, PeerInfo> tmpfds(peers);
	for (iter = tmpfds.begin(); iter != tmpfds.end(); iter++) {
		if (nready <= 0) {
			break;
		}
		int sockfd = iter->first;
		if (FD_ISSET(sockfd, &rset)) {
			nready--;
			std::string buf;
			size_t n = read_one_packet(sockfd, buf);
			if (n < 0) {
				Util::log_error("Read message failed for %s: %s\n", buf.c_str(),
						iter->second.toString().c_str());
			} else if (n == 0) {
				//peer closed
			} else {
				this->handle_peer_msg(sockfd, buf);
			}
		}
	}
	return nready;
}
int Listener::reply_ack(int sockfd) {
	return Util::writen(sockfd, "ACK");
}

int Listener::handle_peer_msg(int sockfd, const std::string& msg) {
	if (msg.length() < Packet::HEADSIZE) {
		Util::log_error("Unknown Message:%s\n", msg.c_str());
		return -1;
	}
	unsigned char pd = (unsigned char) msg[16];

	try {
		switch (pd) {
		case (unsigned char) 0x00: {
			Ping obj;
			obj.parseString(msg);
			Util::log_debug("Recv message type=0x%x, id=%s,ttl=%d, hops=%d.\n",
					pd, obj.hexid().c_str(), obj.ttl, obj.hops);
			this->handle_ping(sockfd, obj);
			break;
		}
		case (unsigned char) 0x01: {
			Pong obj;
			obj.parseString(msg);
			Util::log_debug("Recv message type=0x%x, id=%s,ttl=%d, hops=%d.\n",
					pd, obj.hexid().c_str(), obj.ttl, obj.hops);
			this->handle_pong(sockfd, obj);
			break;
		}
		case (unsigned char) 0x80: {
			Query obj;
			obj.parseString(msg);
			Util::log_debug("Recv message type=0x%x, id=%s,ttl=%d, hops=%d.\n",
					pd, obj.hexid().c_str(), obj.ttl, obj.hops);
			this->handle_Query(sockfd, obj);
			break;
		}
		case (unsigned char) 0x81: {
			QueryHits obj;
			obj.parseString(msg);
			Util::log_debug("Recv message type=0x%x, id=%s,ttl=%d, hops=%d.\n",
					pd, obj.hexid().c_str(), obj.ttl, obj.hops);
			this->handle_QueryHits(sockfd, obj);
			break;
		}
		default:
			Util::log_error("Unknown Message type:%x\n", pd);
			return -1;
			break;
		}
	} catch (const std::runtime_error& e) {
		Util::log_error("Error! %s", e.what());
		return -1;
	}

	return 0;
}

size_t Listener::maxPeerSize() {
	unsigned int maxsize = (FD_SETSIZE - 10) / 100 * 100;
	if (!conf.isSeedNode) {
		maxsize = std::min(conf.NumberOfPeers, maxsize);
	}
	return maxsize;
}

bool Listener::peerFull() {
	return peers.size() >= maxPeerSize();
}

void Listener::erasePeer(int sockfd) {
	PeerInfo &pi = this->peers[sockfd];
	Util::log_debug("Remove peer:%s\n", pi.toString().c_str());
	FD_CLR(sockfd, &this->fdset);
	peers.erase(sockfd);
	close(sockfd);
}

bool Listener::isStaleMessage(Packet& msg) {
	if (typeid(msg) == typeid(Ping&)) {
		return pingHist.find(msg.key()) != pingHist.end();
	} else if (typeid(msg) == typeid(Pong&)) {
		return pongHist.find(msg.key()) != pongHist.end();
	} else if (typeid(msg) == typeid(Query&)) {
		return queryHist.find(msg.key()) != queryHist.end();
	} else if (typeid(msg) == typeid(QueryHits&)) {
		return queryhitsHist.find(msg.key()) != queryhitsHist.end();
	} else {
		Util::log_error("Unknown msg type:%s", typeid(msg).name());
	}
	return false;
}

void Listener::saveMsgHist(int sockfd, Packet& msg) {
	std::string key = msg.hexid();
	time_t now = time(0);
	if (typeid(msg) == typeid(Ping&)) {
		pingHist[key] = MsgInfo(msg.key(), sockfd, now);
	} else if (typeid(msg) == typeid(Pong&)) {
		pongHist[key] = MsgInfo(msg.key(), sockfd, now);
	} else if (typeid(msg) == typeid(Query&)) {
		queryHist[key] = MsgInfo(msg.key(), sockfd, now);
	} else if (typeid(msg) == typeid(QueryHits&)) {
		queryhitsHist[key] = MsgInfo(msg.key(), sockfd, now);
	} else {
		Util::log_error("Unknown msg type:%s", typeid(msg).name());
	}
}

int Listener::forward_message_single(Packet& msg, int sockfd) {
	ssize_t nwrite = Util::writen(sockfd, msg.toString());
	if (nwrite < 0) {
		Util::log_error("forward failed for socked:%d\n", sockfd);
	}
	std::string buf;
	if (0) {
		ssize_t nread = Util::readed(sockfd, buf);
		if (nread < 1 || buf != "ACK") {
			Util::log_error("Failed to get acknowledgement from socked:%d\n",
					sockfd);
			this->erasePeer(sockfd);
		}
	}
	return nwrite;
}

int Listener::forward_message(Packet& msg, int src_sockfd) {
	std::map<int, PeerInfo>::iterator iter;
	for (iter = peers.begin(); iter != peers.end(); iter++) {
		int sockfd = iter->first;
		if (sockfd == src_sockfd)
			continue;
		(void) forward_message_single(msg, sockfd);
	}
	return 0;
}

int Listener::reply_ping(int sockfd, Ping& ping) {
	Pong msg;
	msg.setid(ping.id);
	msg.ttl = conf.TTL;
	msg.ip = hostip;
	msg.port = conf.neighborPort;
	ssize_t nwrite = Util::writen(sockfd, msg.toString());
	if (nwrite < 0) {
		Util::log_error("Reply pong failed for sock:%d\n", sockfd);
	}
	return nwrite;
}

void Listener::query(std::string keyword, QueryHits& msg) {
	std::vector<QueryResult> rs;
	for (size_t i = 0; i < localFiles.size(); i++) {
		FileInfo& fi = localFiles[i];
		if (fi.match(keyword)) {
			QueryResult r;
			r.file_index = fi.file_index;
			r.file_name = fi.file_name;
			r.file_size = fi.size;
			rs.push_back(r);
		}
	}
	msg.ttl = conf.TTL;
	msg.ip = hostip;
	msg.port = conf.filePort;
	msg.resultSet = rs;
	msg.num_hits = rs.size();
}

int Listener::reply_query(int sockfd, Query& query) {
	QueryHits msg;
	this->query(query.criteria, msg);
	if (msg.resultSet.empty()) {
		Util::log_debug("No result found for %s\n", query.criteria.c_str());
		return 0;
	}
	msg.setid(query.id);
	ssize_t nwrite = Util::writen(sockfd, msg.toString());
	if (nwrite < 0) {
		Util::log_error("Reply QueryHits failed for sock:%d\n", sockfd);
	}
	return nwrite;
}

bool Listener::isMyPing(Ping& msg) {
	return myPingHist.find(msg.hexid()) != myPingHist.end();
}

bool Listener::isMyPing(Pong& msg) {
	return myPingHist.find(msg.hexid()) != myPingHist.end();
}

bool Listener::isMyQuery(Query& msg) {
	return myQueryHexid == msg.hexid();
}

bool Listener::isMyQuery(QueryHits& msg) {
	return myQueryHexid == msg.hexid();
}

int Listener::handle_ping(int sockfd, Ping msg) {

	if (isMyPing(msg)) {
		return 0;
	}

	if (isStaleMessage(msg)) {
		Util::log_debug("Discard message:%s\n", msg.hexid().c_str());
		return 0;
	}
	msg.ttl = std::max(0, msg.ttl - 1);
	msg.hops = msg.hops + 1;
	saveMsgHist(sockfd, msg);

	if (!conf.isSeedNode && !peerFull()) { //seed node does not actively allowed connected
		reply_ping(sockfd, msg);
	}

	if (msg.ttl > 0)
		forward_message(msg, sockfd);

	return 0;
}

int Listener::handle_pong(int sockfd, Pong msg) {

	if (isMyPing(msg)) {
		if (!has_peer(msg.ip, msg.port)) {
			if (peerFull()) {
				Util::log_debug(
						"Enough peers. Save Pong message from %s:%d for later connection.\n",
						Util::itoaddress(msg.ip).c_str(), msg.port);
				//TODO: save pong message
			} else {
				this->connect_peer(msg.ip, msg.port);
			}
		} else {
			Util::log_debug(
					"Discard Pong message since %s:%d is already a peer.\n",
					Util::itoaddress(msg.ip).c_str(), msg.port);
			//discard the message;
		}
		return 0;
	} else {
//		if (isStaleMessage(msg)) {
//			Util::log_debug("Discard message:%s\n", msg.hexid().c_str());
//			return 0;
//		}
//		msg.ttl = std::max(0, msg.ttl - 1);
//		msg.hops = msg.hops + 1;
//		saveMsgHist(sockfd, msg);

		if (pingHist.find(msg.hexid()) != pingHist.end()) {
			MsgInfo info = pingHist[msg.hexid()];
			this->forward_message_single(msg, info.sockfd);
		} else {
			Util::log_debug("Discard Pong message:%s\n", msg.hexid().c_str());
			return 0;
		}
	}
	return 0;
}

int Listener::handle_Query(int sockfd, Query msg) {
	if (isMyQuery(msg)) {
		return 0;
	}

	if (isStaleMessage(msg)) {
		Util::log_debug("Discard message:%s\n", msg.hexid().c_str());
		return 0;
	}
	msg.ttl = std::max(0, msg.ttl - 1);
	msg.hops = msg.hops + 1;
	saveMsgHist(sockfd, msg);

	reply_query(sockfd, msg);

	if (msg.ttl > 0)
		forward_message(msg, sockfd);

	return 0;
}

int Listener::handle_QueryHits(int sockfd, QueryHits msg) {

	if (isMyQuery(msg)) {
		this->save_and_display_query_result(msg);
		return 0;
	} else {
//		if (isStaleMessage(msg)) {
//			Util::log_debug("Discard message:%s\n", msg.hexid().c_str());
//			return 0;
//		}
//		msg.ttl = std::max(0, msg.ttl - 1);
//		msg.hops = msg.hops + 1;
//		saveMsgHist(sockfd, msg);

		if (queryHist.find(msg.hexid()) != queryHist.end()) {
			MsgInfo info = queryHist[msg.hexid()];
			this->forward_message_single(msg, info.sockfd);
		} else {
			Util::log_error("Discard QueryHits message:%s\n",
					msg.hexid().c_str());
			return 0;
		}
	}
	return 0;
}

int Listener::display(int localidx, QueryResultInfo rs) {
	std::string ip =
			rs.ip == this->hostip ? "localhost" : Util::itoaddress(rs.ip);
	fprintf(stdout, "%4d %4d %10d %20s %12s:%d\n", localidx, rs.qr.file_index,
			rs.qr.file_size, rs.qr.file_name.c_str(), ip.c_str(), rs.port);
	return 0;
}
int Listener::save_and_display_query_result(QueryHits qh) {
	size_t idx = queryResultSet.size();
	for (size_t i = 0; i < qh.resultSet.size(); i++) {
		QueryResult r = qh.resultSet[i];
		QueryResultInfo qri(qh.ip, qh.port, r);
		this->queryResultSet.push_back(qri);
		this->display(idx, qri);
		idx++;
	}
	fprintf(stdout, "\n");
	return 0;
}

int Listener::ping_peer(int sockfd) {
	Ping ping;
	return ping_peer(sockfd, ping.id);
}

int Listener::ping_peer(int sockfd, PACKET_ID id) {
	Ping ping;
	ping.ttl = conf.TTL;
	ping.setid(id);
	MsgInfo mi(ping.hexid(), sockfd, time(0));
	myPingHist[ping.hexid()] = mi;
	if (Util::writen(sockfd, ping.toString()) < 0) {
		Util::log_error("Send Ping to peer failed: %d\n", sockfd);
		this->erasePeer(sockfd);
		close(sockfd);
		return -1;
	}
	return 0;
}

int Listener::connect_seeds() {
	for (size_t i = 0; i < seedNodes.size(); i++) {
		const SeedInfo &si = seedNodes[i];
		if (!this->has_peer(si.ip, si.port)) {
			this->connect_peer(si.ip, si.port);
		}
	}
	return 0;
}

int Listener::try_remove_one_seed() {
	for (size_t i = 0; i < seedNodes.size(); i++) {
		const SeedInfo &si = seedNodes[i];
		int fd = this->get_peer_fd(si.ip, si.port);
		if (fd > 0) {
			this->erasePeer(fd);
			break;
		}
	}
	return 0;
}

int Listener::ping_peers() {
	Ping ping;
	std::map<int, PeerInfo> tmpfds(peers);
	std::map<int, PeerInfo>::iterator iter;
	for (iter = tmpfds.begin(); iter != tmpfds.end(); iter++) {
		ping_peer(iter->first, ping.id);
	}
	return 0;
}

int Listener::query_peer(int sockfd, std::string keyword) {
	Query query;
	return query_peer(sockfd, keyword, query.id);
}

int Listener::query_peer(int sockfd, std::string keyword, PACKET_ID id) {
	Query query;
	query.ttl = conf.TTL;
	query.criteria = keyword;
	query.setid(id);
	myQueryHexid = query.hexid();
	if (Util::writen(sockfd, query.toString()) < 0) {
		Util::log_error("Send Query to peer failed: %d\n", sockfd);
		this->erasePeer(sockfd);
		close(sockfd);
		return -1;
	}
	return 0;
}

int Listener::query_peers(std::string keyword) {
	std::map<int, PeerInfo> tmpfds(peers);
	std::map<int, PeerInfo>::iterator iter;
	Packet p;
	for (iter = tmpfds.begin(); iter != tmpfds.end(); iter++) {
		query_peer(iter->first, keyword, p.id);
	}
	return 0;
}

int Listener::query_local(std::string keyword) {
	QueryHits msg;
	this->query(keyword, msg);
	this->save_and_display_query_result(msg);
	return msg.num_hits;
}

int Listener::connect_peer(int ip, unsigned short port) {
	int sockfd;
	struct sockaddr_in servaddr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		Util::log_error("socket() failed\n");
		return -1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(ip);

	if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
		Util::log_error("Connect to %s:%d failed\n",
				Util::itoaddress(ip).c_str(), port);
		close(sockfd);
		return -1;
	} else
		Util::log_debug("Connected to %s:%d\n", Util::itoaddress(ip).c_str(),
				port);

	const char* buf = "GNUTELLA CONNECT/0.4\r\n";
	if (Util::writen(sockfd, buf) < 0) {
		Util::log_error("Send CONNECT msg failed.\n");
		close(sockfd);
		return -1;
	} else {
		Util::log_debug("Send CONNECT msg to %s:%d\n",
				Util::itoaddress(ip).c_str(), port);
	}
	std::string reply;
	if (Util::readed(sockfd, reply) <= 0) {
		Util::log_debug("Recv CONNECT reply msg failed.\n");
		close(sockfd);
		return -1;
	} else
		Util::log_debug("Get reply from  %s:%d:%s\n",
				Util::itoaddress(ip).c_str(), port, reply.c_str());

	if (reply != "GNUTELLA OK\n\n" && reply != "GNUTELLA OK\r\n") {
		Util::log_debug("Recv unknown CONNECT reply msg %s.\n", reply.c_str());
		close(sockfd);
		return -1;
	}

	PeerInfo pinfo;
	pinfo.sockfd = sockfd;
	pinfo.ip = ip;
	pinfo.port = port;
	pinfo.ip = ntohl(servaddr.sin_addr.s_addr);
	pinfo.port = ntohs(servaddr.sin_port);
	peers[sockfd] = pinfo;
	maxfd = std::max(maxfd, sockfd);
	FD_SET(sockfd, &this->fdset);
	Util::log_debug("Connect to peer successfully.\n");
	return 0;

}

bool Listener::has_peer(int ip, unsigned short port) {
	return get_peer_fd(ip, port) > 0;
}

int Listener::get_peer_fd(int ip, unsigned short port) {
	std::map<int, PeerInfo>::iterator iter;
	for (iter = peers.begin(); iter != peers.end(); iter++) {
		if (iter->second.ip == ip && iter->second.port == port) {
			return iter->first;
		}
	}
	return -1;
}

int Listener::handle_new_peer() {
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	int connfd = accept(neighborfd, (SA *) &cliaddr, &clilen);

	char str[INET6_ADDRSTRLEN];
	Util::log_debug("new client: %s, port %d\n",
			inet_ntop(AF_INET, &cliaddr.sin_addr, str, INET6_ADDRSTRLEN),
			ntohs(cliaddr.sin_port));

	if (this->peers.size() >= maxPeerSize()) {
		Util::log_debug("too many peers, exceeds limits: %u\n", maxPeerSize());
		close(connfd);
		return -1; //ignore the request
	}

	std::string buf;
	int nread = Util::readed(connfd, buf);
	if (nread < 0) {
		Util::log_error("Error to read from peer.\n");
		close(connfd);
		return -1; //ignore the request
	}

	if (buf == "GNUTELLA CONNECT/0.4\n\n"
			|| buf == "GNUTELLA CONNECT/0.4\r\n") {
		ssize_t nwriten = Util::writen(connfd, "GNUTELLA OK\r\n");
		if (nwriten > 0) {
			PeerInfo pinfo;
			pinfo.ip = ntohl(cliaddr.sin_addr.s_addr);
			pinfo.port = ntohs(cliaddr.sin_port);
			pinfo.sockfd = connfd;
			peers[connfd] = pinfo;
			FD_SET(connfd, &fdset); /* add new descriptor to set */
			maxfd = std::max(maxfd, connfd);
		} else {
			Util::log_error("reply GNUTELLA OK to peer failed\n");
			close(connfd);
			return -1;
		}

	} else {
		Util::log_error("Unknown request from peer: %s\n", buf.c_str());
		close(connfd);
		return -1;
	}

	return 0;
}

int Listener::initialize() {
	if (readLocalFileConf() < 0) {
		fprintf(stderr, "Error! Load local files failed.\n");
	}

	if (readLocalSeedConf() < 0) {
		fprintf(stderr, "Error! Load local files failed.\n");
	}

	{
		int listenfd;
		struct sockaddr_in servaddr;
		listenfd = socket(AF_INET, SOCK_STREAM, 0);
		int enable = 1;
		setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		int port = conf.neighborPort;
		servaddr.sin_port = htons(port);

		if (bind(listenfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
			Util::log_info("Bind neighbor socket failed at port %d, errno=%d\n",
					conf.neighborPort, errno);
			return -1;
		}

		if (listen(listenfd, LISTENQ) < 0) {
			Util::log_info(
					"Listen neighbor socket failed at port %d, errno=%d\n",
					conf.neighborPort, errno);
			return -1;
		}

		this->neighborfd = listenfd;
		Util::log_info("Bind neighbor socket at port %d\n", conf.neighborPort);
	}

	{
		int listenfd;
		struct sockaddr_in servaddr;
		listenfd = socket(AF_INET, SOCK_STREAM, 0);
		int enable = 1;
		setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		int port = conf.filePort;
		servaddr.sin_port = htons(port);

		if (bind(listenfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
			Util::log_info("Listen file socket failed at port %d, errno=%d\n",
					conf.neighborPort, errno);
			return -1;
		}

		if (listen(listenfd, LISTENQ) < 0) {
			Util::log_info("Listen file socket failed at port %d, errno=%d\n",
					conf.neighborPort, errno);
			return -1;
		}

		this->filefd = listenfd;
		Util::log_info("Bind file socket at port %d\n", conf.filePort);
	}

	this->hostip = Util::getLocalIP();
	Util::log_info("Using IP %s for peers\n", Util::itoaddress(hostip).c_str());

	this->maxfd = std::max(neighborfd, filefd);
	FD_ZERO(&fdset);
	FD_SET(this->neighborfd, &fdset);
	FD_SET(this->filefd, &fdset);

	TimerTask tt;
	tt.name = "ping";
	tt.last_exec = time(NULL);
	tt.inteveral = 10;
	timeTaskQueue.push_back(tt);

	tt.name = "clean_hist";
	tt.last_exec = time(NULL);
	tt.inteveral = 60;
	timeTaskQueue.push_back(tt);

	return 0;
}

int Listener::com_connect(int ip, int port) {
	return this->connect_peer(ip, port);
}

int Listener::com_connect(std::string host, int port) {
	int ip = Util::host2ip(host);
	if (has_peer(ip, port)) {
		fprintf(stderr, "%s has been already a peer.\n",
				Util::itoaddress(ip).c_str());
		return 0;
	}

	int code = this->com_connect(ip, port);
	if (code < 0) {
		fprintf(stderr, "connect to %s failed.\n",
				Util::itoaddress(ip).c_str());
	}
	return code;
}

int Listener::com_ping() {
	if (peers.empty()) {
		fprintf(stdout, "no peer has been connected to!\n");
		return 0;
	}
	if (this->peerFull()) {
		fprintf(stdout, "Enough peers have been connected to (size=%ld).\n",
				peers.size());
		return 0;
	}
	return this->ping_peers();
}

int Listener::com_peers() {
	fprintf(stdout, "Peers(%ld/%ld):\n", peers.size(), maxPeerSize());
	std::map<int, PeerInfo>::iterator iter;
	for (iter = peers.begin(); iter != peers.end(); iter++) {
		fprintf(stdout, "%s\n", iter->second.toString().c_str());
	}
	fprintf(stdout, "\n");
	return 0;
}

int Listener::com_qrs() {
	fprintf(stdout, "keyword: %s, total %ld records.\n", myQueryKeyword.c_str(),
			queryResultSet.size());
	for (size_t i = 0; i < queryResultSet.size(); i++) {
		fprintf(stdout, "%lu: %s\n", i, queryResultSet[i].toString().c_str());
	}
	fprintf(stdout, "\n");
	return 0;
}
int Listener::com_dump() {
	fprintf(stdout, "Local IP (0x%x): %s\n", hostip,
			Util::itoaddress(hostip).c_str());
	fprintf(stdout, "\n");

	fprintf(stdout, "Seed nodes:\n");
	for (size_t i = 0; i < seedNodes.size(); i++) {
		fprintf(stdout, "%s\n", seedNodes[i].toString().c_str());
	}
	fprintf(stdout, "\n");

	fprintf(stdout, "Local files:\n");
	for (size_t i = 0; i < localFiles.size(); i++) {
		fprintf(stdout, "%s\n", localFiles[i].toString().c_str());
	}
	fprintf(stdout, "\n");

	fprintf(stdout, "Peers(%ld):\n", peers.size());
	std::map<int, PeerInfo>::iterator iter;
	for (iter = peers.begin(); iter != peers.end(); iter++) {
		fprintf(stdout, "%s\n", iter->second.toString().c_str());
	}
	fprintf(stdout, "\n");

	fprintf(stdout, "My query keyword=%s, size of results set=%ld\n",
			myQueryKeyword.c_str(), queryResultSet.size());
	for (size_t i = 0; i < queryResultSet.size(); i++) {
		fprintf(stdout, "%lu: %s\n", i, queryResultSet[i].toString().c_str());
	}
	fprintf(stdout, "\n");

	fprintf(stdout, "Timer tasks: %ld\n", timeTaskQueue.size());
	fprintf(stdout, "My Ping historical Queue size: %ld\n", myPingHist.size());
	fprintf(stdout, "Ping historical Queue size: %ld\n", pingHist.size());
	fprintf(stdout, "Pong historical Queue size: %ld\n", pongHist.size());
	fprintf(stdout, "Query historical Queue size: %ld\n", queryHist.size());
	fprintf(stdout, "QueryHits historical Queue size: %ld\n",
			queryhitsHist.size());
	return 0;
}

int Listener::com_search(std::string keyword) {
	fprintf(stdout, "%4s %4s %10s %20s %12s\n", "lidx", "ridx", "size", "name",
			"location");
	this->myQueryHexid = "";
	this->myQueryKeyword = keyword;
	this->queryResultSet.clear();
	int n = this->query_local(keyword);
	if (n == 0) {
		Util::log_debug("No local files found for keyword %s\n",
				keyword.c_str());
	}
	if (this->peers.size() == 0) {
		Util::log_info(
				"Warning! No remote search since no peer is in connection.\n");
	} else {
		this->query_peers(keyword);
	}
	return 0;
}

int Listener::com_get(int idx, std::string localpath) {
	if ((size_t) idx + 1 > queryResultSet.size()) {
		Util::log_error("Error! Result index %d not found!\n", idx);
		return -1;
	}
	QueryResultInfo fi = queryResultSet[idx];
	if (localpath.empty()) {
		localpath = fi.qr.file_name;
	}
	printf("%s\n", fi.toString().c_str());
	Util::log_info("Downloading file %s from %s:%d to %s...\n",
			fi.qr.file_name.c_str(), Util::itoaddress(fi.ip).c_str(), fi.port,
			localpath.c_str());

	int sockfd;
	struct sockaddr_in servaddr;
	int ip = fi.ip;
	unsigned short port = fi.port;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		Util::log_error("socket() failed\n");
		return -1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(fi.port);
	servaddr.sin_addr.s_addr = htonl(fi.ip);

	if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
		Util::log_error("Connect to %s:%d failed\n",
				Util::itoaddress(fi.ip).c_str(), fi.port);
		close(sockfd);
		return -1;
	} else
		Util::log_debug("Connected to %s:%d\n", Util::itoaddress(fi.ip).c_str(),
				fi.port);

	char tmpstr[MAXLINE];
	sprintf(tmpstr, "GET /get/%d/%s/ HTTP/1.0\r\n", fi.qr.file_index,
			fi.qr.file_name.c_str());
	std::string buf = std::string() + tmpstr + "User-Agent: Gnutella/0.4\r\n"
			+ "\r\n";
	if (Util::writen(sockfd, buf.c_str()) < 0) {
		Util::log_error("Send download msg failed.\n");
		close(sockfd);
		return -1;
	} else {
		Util::log_debug("Send download msg to %s:%d\n",
				Util::itoaddress(ip).c_str(), port);
	}

//read head
	buf.empty();
	size_t nread = Util::read_once(sockfd, buf);
	if (nread < 0) {
		Util::log_error("Read response failed\n");
		close(sockfd);
		return -1;
	}
	DownloadResponse dr;
	if (dr.parseFromString(buf) < 0) {
		Util::log_error("parse response failed:%s\n",
				buf.substr(0, 128).c_str());
		close(sockfd);
		return -1;
	}

//read other part
	size_t totalRecvFileBytes = 0;
	bool has_error = false;
	FILE* localfileptr = fopen(localpath.c_str(), "w");
	if (localfileptr) {
		int nwrite = fwrite(dr.part_binary, sizeof(char), dr.part_binary_len,
				localfileptr);
		totalRecvFileBytes += nwrite;
		has_error = has_error && nwrite < 1;

		ssize_t nread;
		char tmpbuf[MAXLINE];
		while (!has_error) {
			if ((nread = read(sockfd, tmpbuf, MAXLINE)) <= 0) {
				if (nread < 0 && errno == EINTR) {
					nread = 0; /* and call write() again */
					continue;
				} else if (nread == 0) { //remote close socket
					break;
				} else {
					has_error = true;
					break;
				}
			}

			int nwrite = fwrite(tmpbuf, sizeof(char), nread, localfileptr);
			totalRecvFileBytes += nwrite;
			if (nwrite < 1) {
				has_error = true;
				break;
			}
		}
		if (has_error) {
			Util::log_error("Download file failed\n");
		}
		fclose(localfileptr);
	} else {
		Util::log_error("Open file error for %s\n", localpath.c_str());
	}
	close(sockfd);

	if (dr.contentlength != totalRecvFileBytes) {
		Util::log_error(
				"Warning! Response size is not same as file written size. The file may corrupt!");
		has_error = true;
	}
	if (!has_error) {
		Util::log_info("Finished downloading file.\n");
	}

	return has_error ? -1 : 0;

}

int Listener::readLocalFileConf() {
	const std::string& filename = conf.localFiles;
	if (!Util::file_exists(filename.c_str())) {
		Util::log_error("Error! File %s does not exists\n", filename.c_str());
		return -1;
	}
	std::vector<std::string> lines = Util::readlines(filename.c_str());
	int index = 0;
	for (size_t i = 0; i < lines.size(); i++) {
		std::string str = Util::trim(lines[i]);
		if (str.empty())
			continue;
		size_t n = str.find_first_of(":");
		if (n == std::string::npos || n + 1 == str.length()) {
			Util::log_error("Warning! ignore line in %s: %s\n",
					filename.c_str(), str.c_str());
			continue;
		}
		std::string thisfilename = str.substr(0, n);
		thisfilename = Util::trim(thisfilename);
		str = str.substr(n + 1);
		std::vector<std::string> vec = Util::splitByDeli2(str, '|');
		if (thisfilename.length() == 0 || vec.size() == 0) {
			Util::log_error(
					"Warning! filename is empty or no keywords found for a line in %s: %s\n",
					filename.c_str(), str.c_str());
			continue;
		}
		std::string thisfilepath = conf.localFilesDirectory + "/"
				+ thisfilename;
		if (!Util::file_exists((thisfilepath).c_str())) {
			Util::log_error("Warning! %s does not exist in folder %s\n",
					thisfilename.c_str(), conf.localFilesDirectory.c_str());
			continue;
		}
		FileInfo fi(thisfilename);
		fi.keywords = vec;
		fi.size = Util::getFileSize(thisfilepath);
		fi.file_index = index++;
		this->localFiles.push_back(fi);
	}
	return 0;
}

int Listener::readLocalSeedConf() {
	const std::string& filename = conf.seedNodes;
	if (!Util::file_exists(filename.c_str())) {
		Util::log_error("Error! File %s does not exists\n", filename.c_str());
		return -1;
	}
	std::vector<std::string> lines = Util::readlines(filename.c_str());
	for (size_t i = 0; i < lines.size(); i++) {
		std::string str = Util::trim(lines[i]);
		if (str.empty())
			continue;
		std::vector<std::string> vec = Util::split(str);
		if (vec.size() != 2) {
			Util::log_error(
					"Warning! filename is empty or no keywords found for a line in %s: %s\n",
					filename.c_str(), str.c_str());
			continue;
		}
		SeedInfo si;
		si.ip = Util::host2ip(vec[0]);
		si.port = atoi(vec[1].c_str());
		this->seedNodes.push_back(si);
	}
	return 0;
}
