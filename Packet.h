/*
 * Packet.h
 *
 * Defines p2p msg packets of Ping/Pong/Query/QueryHits
 * The packet is encoded in binary strictly following
 * The v0.4 specification.
 *
 *  Created on: Nov 12, 2015
 *      Author: lizhen
 */

#ifndef PACKET_H_
#define PACKET_H_

/***
 * Optional data in binary packet.
 */
class OptData {
public:
	OptData();
	OptData(OptData& other);
	OptData& operator=(const OptData& other);
	virtual ~OptData();
	virtual std::string toString();
	virtual void parseString(std::string str);
	virtual unsigned char* getOptData();
	virtual unsigned int getOptDataLen();
	virtual void setOptData(unsigned char* buf, unsigned int len);
	virtual void initOptData(unsigned int len);

protected:
	unsigned int optlen; // payload length
	unsigned char* optdata; // payload length
};

typedef unsigned char* PACKET_ID;

/**
 * Packet header class.
 */
class Packet {
public:
	static size_t HEADSIZE;
	static size_t IDSIZE;
	unsigned char id[16];
	unsigned char pd; //payload descriptor
	unsigned char ttl;
	unsigned char hops;

	Packet();
	virtual ~Packet();
	virtual void setid(unsigned char newid[16]);
	virtual std::string hexid();
	virtual std::string key();
	virtual std::string toString();
	virtual void parseString(std::string str);
	virtual unsigned char* getPayload();
	virtual OptData& getPayloadObj();
	virtual unsigned int getPayloadLength();
	virtual void setPayload(unsigned char* buf, unsigned int len);
	virtual void initPayload(unsigned int len);

public:
	std::string short2string(unsigned short i);

	unsigned short string2short(std::string str, int pos);

	static std::string int2string(int i);

	static size_t string2uint(std::string str, int pos);

protected:

	virtual void parsePayload();

	virtual void updatePayload();
protected:
	OptData payload;
};

/**
 * Ping message
 */
class Ping: public Packet {
public:
	Ping();
	virtual ~Ping();
	OptData& getOptData();
};

/**
 * Pong Message
 */
class Pong: public Packet {
public:
	unsigned short port;
	int ip;
	unsigned int num_files;
	unsigned int num_size; //size in Kilobytes
	Pong();
	virtual ~Pong();
	OptData& getOptData();
protected:
	virtual void parsePayload();

	virtual void updatePayload();
protected:
	OptData optdata;
};

/**
 * Query Message
 */
class Query: public Packet {
public:
	unsigned short min_speed;
	std::string criteria;
	Query();
	virtual ~Query();
	OptData& getOptData();
protected:
	virtual void parsePayload();

	virtual void updatePayload();
protected:
	OptData optdata;
};

/**
 * One piece of query result in QueryHits packet
 */
class QueryResult {
public:
	QueryResult();
	QueryResult(const QueryResult& other);
	QueryResult& operator=(const QueryResult& other);
	unsigned int file_index;
	unsigned int file_size;
	std::string file_name;
	OptData optdata;

	std::string toString();
	void parseString(std::string);

	unsigned int length();

};

/**
 * QueryHits Message
 */
class QueryHits: public Packet {
public:
	unsigned char num_hits;
	unsigned short port;
	int ip;
	unsigned int speed;
	std::vector<QueryResult> resultSet;
	OptData QHDData;
	unsigned char servent_id[16];
	QueryHits();
	virtual ~QueryHits();
protected:
	virtual void parsePayload();

	virtual void updatePayload();
};

#endif /* PACKET_H_ */
