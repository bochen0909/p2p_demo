/*
 * Packet.cpp
 *
 *  Created on: Nov 2, 2015
 *      Author: lizhen
 */

#include <sstream>
#include <iostream>
#include <assert.h>
#include <strings.h>
#include <arpa/inet.h>
#include <stdexcept>

#include "Util.h"
#include "Packet.h"

OptData::OptData() :
		optlen(0), optdata(0) {

}
OptData::OptData(OptData& other) :
		OptData() {
	this->setOptData(other.optdata, other.optlen);
}

OptData& OptData::operator=(const OptData& other) {
	this->setOptData(other.optdata, other.optlen);
	return *this;
}

OptData::~OptData() {
	if (optdata) {
		delete[] optdata;
		optdata = 0;
		optlen = 0;
	}
}
std::string OptData::toString() {
	std::string str = "";
	if (this->optlen > 0) {
		assert(optdata);
		for (unsigned int i = 0; i < optlen; i++)
			str += this->optdata[i];
	}
	return str;
}

void OptData::parseString(std::string str) {
	if (this->optdata) {
		delete[] this->optdata;
		this->optdata = 0;
	}
	size_t len = str.length();
	this->optlen = len;
	if (len > 0) {
		this->optdata = new unsigned char[len];
		for (unsigned int i = 0; i < len; i++) {
			optdata[i] = (unsigned char) str[i];
		}
	}
}

unsigned char* OptData::getOptData() {
	return optdata;
}
unsigned int OptData::getOptDataLen() {
	return optlen;
}

void OptData::setOptData(unsigned char* buf, unsigned int len) {
	if (this->optdata) {
		delete[] this->optdata;
		this->optdata = 0;
	}
	this->optlen = len;
	if (len > 0) {
		this->optdata = new unsigned char[len];
		for (unsigned int i = 0; i < len; i++) {
			optdata[i] = buf[i];
		}
	}
}

void OptData::initOptData(unsigned int len) {

	unsigned char* tmp = new unsigned char[len];
	for (unsigned int i = 0; i < len; i++)
		tmp[i] = 32;
	this->setOptData(tmp, len);
	delete[] tmp;

}

/////////////////////////Packet//////////////////////////////////
size_t Packet::HEADSIZE = 23;
size_t Packet::IDSIZE = 16;

Packet::Packet() :
		pd(0), ttl(10), hops(0), payload() {
	for (size_t i = 0; i < IDSIZE; i++) {
		id[i] = Util::rand_lim(0xFF);
	}
	id[8] = 0xFF;
	id[15] = 0x00;
}

Packet::~Packet() {
}

//little-endian byte order by specification. Only work little-endian system.
std::string Packet::short2string(unsigned short i) {
	unsigned char*p = (unsigned char*) &i;
	std::string str = "";
	str += *p;
	str += *(p + 1);
	return str;
}

//little-endian byte order by specification. Only work little-endian system.
unsigned short Packet::string2short(std::string str, int pos) {
	unsigned char p[2] = { (unsigned char) str[pos],
			(unsigned char) str[pos + 1] };
	return *((unsigned short*) p);
}

//little-endian byte order by specification. Only work little-endian system.
std::string Packet::int2string(int i) {
	unsigned char*p = (unsigned char*) &i;
	std::string str = "";
	str += *p;
	str += *(p + 1);
	str += *(p + 2);
	str += *(p + 3);
	return str;
}

//little-endian byte order by specification. Only work little-endian system.
size_t Packet::string2uint(std::string str, int pos) {
	unsigned char p[4] = { (unsigned char) str[pos],
			(unsigned char) str[pos + 1], (unsigned char) str[pos + 2],
			(unsigned char) str[pos + 3] };
	return *((int*) p);
}

void Packet::updatePayload() {

}

void Packet::setid(unsigned char newid[16]) {
	for (size_t i = 0; i < IDSIZE; i++) {
		id[i] = newid[i];
	}
}
std::string Packet::hexid() {
	std::string str = "";
	for (size_t i = 0; i < IDSIZE; i++) {
		str += id[i];
	}
	return Util::toHex(str);
}

std::string Packet::key() {
	std::string str = "";
	for (size_t i = 0; i < IDSIZE; i++) {
		str += id[i];
	}
	return Util::toHex(str);
}

std::string Packet::toString() {
	this->updatePayload();
	std::string str = "";
	for (size_t i = 0; i < IDSIZE; i++) {
		str += id[i];
	}
	str += pd;
	str += ttl;
	str += hops;
	unsigned int pl = payload.getOptDataLen();
	str += int2string(pl);
	str += payload.toString();
	return str;
}

void Packet::parseString(std::string str) {
	if (str.length() < IDSIZE) {
		throw std::runtime_error(
				Util::vatostr2("parse msg error: %s",
						Util::toHex(str).c_str()));
	}
	for (size_t i = 0; i < IDSIZE; i++) {
		id[i] = (unsigned char) str.at(i);
	}
	pd = (unsigned char) str[16];
	ttl = (unsigned char) str[17];
	hops = (unsigned char) str[18];
	unsigned int pl = string2uint(str, 19);
	if (pl > 0) {
		if (str.length() != HEADSIZE + pl) {
			throw std::runtime_error(
					Util::vatostr2(
							"parse msg error, payload is not right (%u<>%u):\n%s\n",
							str.length(), HEADSIZE + pl,
							Util::toHex(str).c_str()));
		}

		unsigned char* tmp = new unsigned char[pl];
		for (unsigned int i = 0; i < pl; i++) {
			tmp[i] = (unsigned char) str[HEADSIZE + i];
		}
		this->setPayload(tmp, pl);
		delete[] tmp;
		this->parsePayload();
	}
}

OptData& Packet::getPayloadObj() {
	this->updatePayload();
	return payload;
}
unsigned char* Packet::getPayload() {
	this->updatePayload();
	return payload.getOptData();
}
unsigned int Packet::getPayloadLength() {
	this->updatePayload();
	return this->payload.getOptDataLen();
}

void Packet::setPayload(unsigned char* buf, unsigned int len) {
	this->payload.setOptData(buf, len);
	this->parsePayload();
}

void Packet::initPayload(unsigned int len) {
	this->payload.initOptData(len);
	this->parsePayload();
}

void Packet::parsePayload() {
//empty since header don't have payload
}

//########################## Ping ############################
Ping::Ping() :
		Packet() {
	pd = 0x00;
}
Ping::~Ping() {

}
OptData& Ping::getOptData() {
	return payload;
}

//########################## Pong ############################
Pong::Pong() :
		Packet(), port(0), ip(0), num_files(0), num_size(0) {
	pd = 0x01;
}
Pong::~Pong() {

}
OptData& Pong::getOptData() {
	return optdata;
}

void Pong::updatePayload() {
	std::string str = "";
	str += short2string(port);
	str += this->int2string(htonl(ip));
	str += this->int2string(num_files);
	str += this->int2string(num_size);
	str += optdata.toString();
	this->payload.parseString(str);
}

void Pong::parsePayload() {
	std::string str = payload.toString();
	if (str.length() < 14) {
		throw std::runtime_error(
				Util::vatostr2("parse Pong payload failed:\n%s",
						Util::toHex(str).c_str()));
	}

	port = string2short(str, 0);
	ip = ntohl(string2uint(str, 2));
	num_files = string2uint(str, 6);
	num_size = string2uint(str, 10);
	optdata.parseString(str.substr(14));
}

//########################## Query ############################
Query::Query() :
		Packet(), min_speed(0) {
	pd = 0x80;
}
Query::~Query() {

}
OptData& Query::getOptData() {
	return optdata;
}

void Query::updatePayload() {
	std::string str = "";
	str += short2string(min_speed);
	str += criteria;
	str += (char) 0x00; //NUL (0x00) Terminator
	str += optdata.toString();
	this->payload.parseString(str);
}

void Query::parsePayload() {
	std::string str = payload.toString();
	if (str.length() < 4) {
		throw std::runtime_error(
				Util::vatostr2("parse Query payload failed:\n%s",
						Util::toHex(str).c_str()));
	}

	unsigned int idx = 0;
	min_speed = string2short(str, idx);
	idx += 2;
	criteria = "";
	while (str[idx]) {
		criteria += str[idx];
		idx++;
	}
	idx++; //skip NUL
	if (idx > str.length() || criteria.empty()) {
		throw std::runtime_error(
				Util::vatostr2("parse Query payload failed:\n%s",
						Util::toHex(str).c_str()));
	}
	optdata.parseString(str.substr(idx));
}

//########################## QueryResult ############################
QueryResult::QueryResult() :
		file_index(0), file_size(0) {

}

QueryResult::QueryResult(const QueryResult& other) {
	this->file_index = other.file_index;
	this->file_name = other.file_name;
	this->file_size = other.file_size;
	this->optdata = other.optdata;
}
QueryResult& QueryResult::operator=(const QueryResult& other) {
	this->file_index = other.file_index;
	this->file_name = other.file_name;
	this->file_size = other.file_size;
	this->optdata = other.optdata;
	return *this;
}

std::string QueryResult::toString() {
	std::string str = "";
	str += Packet::int2string(file_index);
	str += Packet::int2string(file_size);
	str += file_name;
	str += (char) 0x00; //NUL (0x00) Terminator
	str += optdata.toString();
	str += (char) 0x00; //NUL (0x00) Terminator
	return str;
}
void QueryResult::parseString(std::string str) {
	if (str.length() < 11) {
		throw std::runtime_error(
				Util::vatostr2("parse QueryResult failed:\n%s",
						Util::toHex(str).c_str()));
	}

	unsigned int idx = 0;
	file_index = Packet::string2uint(str, idx);
	idx += 4;
	file_size = Packet::string2uint(str, idx);
	idx += 4;
	file_name = "";
	while (str[idx]) {
		file_name += str[idx];
		idx++;
	}
	idx++; //skip NUL
	if (file_name.empty() || idx + 1 > str.length()) {
		throw std::runtime_error(
				Util::vatostr2("parse QueryResult failed:\n%s",
						Util::toHex(str).c_str()));
	}

	std::string optstr = "";
	while (idx < str.length() && str[idx]) {
		optstr += str[idx++];
	}
	if (idx + 1 > str.length() || str[idx] != (char) 0x00) {
		throw std::runtime_error(
				Util::vatostr2(
						"parse QueryResult failed. Msg does not endup with NULL:\n%s",
						Util::toHex(str).c_str()));
	}
	optdata.parseString(optstr);
}

unsigned int QueryResult::length() {
	return sizeof(file_index) + sizeof(file_size) + file_name.length()
			+ optdata.getOptDataLen() + 2;
}

//########################## QueryHits ############################
QueryHits::QueryHits() :
		Packet(), num_hits(0), port(0), ip(0), speed(0) {
	pd = 0x81;
	bzero(this->servent_id, 16);
}
QueryHits::~QueryHits() {

}

void QueryHits::updatePayload() {
	std::string str = "";
	str += num_hits;
	str += short2string(port);
	str += int2string(ip);
	str += int2string(speed);
	for (size_t i = 0; i < resultSet.size(); i++) {
		str += resultSet[i].toString();
	}
	str += QHDData.toString();
	for (size_t i = 0; i < IDSIZE; i++) {
		str += servent_id[i];
	}
	this->payload.parseString(str);
}

void QueryHits::parsePayload() {
	std::string str = payload.toString();
	if (str.length() < 27) {
		throw std::runtime_error(
				Util::vatostr2("parse QueryHits failed:\n%s",
						Util::toHex(str).c_str()));
	}
	unsigned int idx = 0;
	num_hits = (unsigned char) str[idx];
	idx += 1;
	port = string2short(str, idx);
	idx += 2;
	ip = string2uint(str, idx);
	idx += 4;
	speed = string2uint(str, idx);
	idx += 4;
	for (size_t i = 0; i < IDSIZE; i++) {
		servent_id[15 - i] = str[str.length() - 1 - i];
	}

	str = str.substr(0, str.length() - IDSIZE);
	str = str.substr(idx);
	resultSet.clear();
	for (size_t i = 0; i < num_hits; i++) {
		QueryResult qr;
		qr.parseString(str);
		resultSet.push_back(qr);
		str = str.substr(qr.length());
	}
	QHDData.parseString(str);
}
