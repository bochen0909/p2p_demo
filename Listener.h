/*
 * Listener.h
 *
 *  Created on: Nov 2, 2015
 *      Author: lizhen
 */

#ifndef LISTENER_H_
#define LISTENER_H_
#include <map>
#include "conf.h"
#include "Packet.h"

//forward declaration
class G04;

/**
 * Info of One piece of message
 */
struct MsgInfo {
	std::string key;
	int sockfd;
	time_t ts;
	MsgInfo() :
			sockfd(0), ts(0) {
	}
	MsgInfo(std::string k, int s, time_t t) {
		key = k;
		sockfd = s;
		ts = t;
	}
};

/**
 * One seed host
 */
struct SeedInfo {
	int ip; //host order
	unsigned int port;
	std::string toString();
};

/**
 * One peer host
 */
struct PeerInfo {
	int ip; //host order
	unsigned short port;
	int sockfd;
	std::string address();
	std::string toString();
};

/**
 * Fileinfo for local shared files
 */
struct FileInfo {
	std::string file_name;
	std::vector<std::string> keywords;
	size_t size;
	int file_index;
	FileInfo(std::string fn) :
			size(0), file_index(0) {
		file_name = fn;
	}
	void addKeyword(std::string kw);
	bool match(std::string kw);
	std::string toString();
};

/**
 * One query result from a peer
 */
struct QueryResultInfo {
	int ip;  //host order
	unsigned short port;
	QueryResult qr;
	QueryResultInfo(int ip, unsigned short port, QueryResult& qr) {
		this->ip = ip;
		this->port = port;
		this->qr = qr;
	}
	std::string toString();
};

/**
 * Download Request from a peer
 */
struct DownloadRequest {
	int fileindex;
	char filename[256];
	char httpversion[32];
	char agentname[256];
	char agentversion[32];
	char range[256];
	char connection[256];
	DownloadRequest();
	std::string toString();
	int parseFromString(const std::string& s);
};

/**
 * Response to a download request
 */
struct DownloadResponse {
	char httpversion[32]; //1.0
	char retcode[32]; //e.g. 200,404
	char retdesc[256]; //e.g. OK
	char servername[256];
	char contenttype[256];
	size_t contentlength;
	char part_binary[MAXLINE]; //part of the binary data follows the header
	size_t part_binary_len;
	DownloadResponse();
	std::string toString();
	int parseFromString(const std::string& s);
};

/**
 * Timers to do tasks like cleanup or finding peers
 */
struct TimerTask {
	std::string name;
	time_t last_exec;
	int inteveral; //in second
};

/***
 * Listener that handle all peers' requests.
 * The requests are waited by select.
 * Small request is handled in current thread.
 * Big request like downloading is handled by
 * a separated thread.
 */
class Listener {
	friend void* peer_download_thread_handler(void* arg);
public:
	Listener(G04* g04);
	virtual ~Listener();
	int initialize();
	int startlisten();
	bool going_stop();
	void going_stop(bool b);
	bool has_stopped();

	bool isStaleMessage(Packet& msg);
	void saveMsgHist(int sockfd, Packet& msg);

	/**
	 * ip and port: host bytes order
	 */
	int com_connect(int ip, int port);
	int com_connect(std::string host, int port);
	int com_ping();
	int com_peers();
	int com_dump();
	int com_qrs();
	int com_search(std::string keyword);
	int com_get(int idx, std::string localpath);

protected:
	/**
	 * handle new connection from peer
	 * return: the number of request handled
	 */
	int handle_new_peer();
	int handle_file_download(int sockfd, DownloadRequest dr);
	int handle_file_download_request();
	int handle_file_download_by_thread(int sockfd, DownloadRequest dr);
	int handle_peer_select(fd_set& rset, int msgcnt);
	int handle_timer_task();

	int handle_peer_msg(int sockfd, const std::string& msg);
	int handle_ping(int sockfd, Ping msg);
	int handle_pong(int sockfd, Pong msg);
	int handle_Query(int sockfd, Query msg);
	int handle_QueryHits(int sockfd, QueryHits msg);
	int reply_ping(int sockfd, Ping& ping);
	int reply_query(int sockfd, Query& query);
	int reply_ack(int sockfd);
	size_t read_one_packet(int sockfd, std::string& buf);

	bool isMyPing(Ping& msg);
	bool isMyPing(Pong& msg);
	bool isMyQuery(Query& msg);
	bool isMyQuery(QueryHits& msg);
	bool peerFull();
	size_t maxPeerSize();
	void erasePeer(int sockfd);
	bool has_peer(int ip, unsigned short port);
	int get_peer_fd(int ip, unsigned short port);

	int forward_message(Packet& msg, int src_sockfd);
	int forward_message_single(Packet& msg, int sockfd);

	int connect_peer(int ip, unsigned short port);
	int ping_peer(int sockfd);
	int ping_peer(int sockfd, PACKET_ID id);
	int ping_peers();
	int connect_seeds();
	int try_remove_one_seed();
	int query_peer(int sockfd, std::string keyword);
	int query_peer(int sockfd, std::string keyword, PACKET_ID id);
	int query_peers(std::string keyword);
	int query_local(std::string keyword);

	int save_and_display_query_result(QueryHits qh);
	int display(int localidx, QueryResultInfo rs);
	void query(std::string keyword, QueryHits& msg);
	int cleanHist();

	int readLocalFileConf();
	int readLocalSeedConf();

protected:
	Conf conf;

	int neighborfd;
	int filefd;

	fd_set fdset;

	bool bStop;
	bool hasStoped;

	int maxfd;

	std::vector<SeedInfo> seedNodes;
	std::map<int, PeerInfo> peers;
	std::vector<FileInfo> localFiles;

	std::string myQueryHexid;
	std::string myQueryKeyword;
	std::vector<QueryResultInfo> queryResultSet;

	std::map<std::string, MsgInfo> myPingHist;
	std::map<std::string, MsgInfo> pingHist;
	std::map<std::string, MsgInfo> pongHist;
	std::map<std::string, MsgInfo> queryHist;
	std::map<std::string, MsgInfo> queryhitsHist;

	std::vector<TimerTask> timeTaskQueue;

	int hostip;
};

struct DownloadRequestArgs {
	int sockfd;
	DownloadRequest dr;
	Listener* listener;
};
#endif /* LISTENER_H_ */
