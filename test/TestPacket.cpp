/*
 * TestPacket.cpp
 *
 *  Created on: Nov 2, 2015
 *      Author: lizhen
 */

#include <iostream>
#include "gtest/gtest.h"
#include "Packet.h"
#include "Util.h"

using namespace std;

class TestPacket: public ::testing::Test {

protected:
	TestPacket() {
	}

};

string ToHex(const string& s, bool upper_case /* = true */) {
	ostringstream ret;

	for (string::size_type i = 0; i < s.length(); ++i)
		ret << std::hex << std::setfill('0') << std::setw(2)
				<< (upper_case ? std::uppercase : std::nouppercase)
				<< (int) ((unsigned char) s[i]);
	return ret.str();
}

TEST_F(TestPacket, toString) {
	Packet packet;
	std::string str = packet.toString();
	ASSERT_EQ(23, str.length());
	string hexstr = ToHex(str, true);
	cout << hexstr << endl;
	ASSERT_EQ(23 * 2, hexstr.length());
}

TEST_F(TestPacket, rand_limit) {
	for (int i = 0; i < 10000; i++) {
		unsigned char r = Util::rand_lim(0xFF);
		ASSERT_GT(0x100, r);
		ASSERT_LE(0, r);
	}
}

TEST_F(TestPacket, parseString) {
	printf("%d -> %04x\n", 1000, 1000);
	Packet packet;
	packet.ttl = 120;
	packet.initPayload(10);
	packet.pd = 0x23;
	packet.hops = 12;
	std::string str = packet.toString();
	ASSERT_EQ(23 + 10, str.length());
	Packet packet2;
	packet2.parseString(str);
	std::string str2 = packet2.toString();
	for (size_t i = 0; i < str.length(); i++) {
		cout << setfill('0') << setw(2) << i;
	}
	cout << endl;
	cout << ToHex(str, true) << endl;
	cout << ToHex(str2, true) << endl;
	ASSERT_EQ(packet.ttl, packet2.ttl);
	ASSERT_EQ(packet.hops, packet2.hops);
	ASSERT_EQ(packet.pd, packet2.pd);
	for (int i = 0; i < 16; i++) {
		ASSERT_EQ(packet.id[i], packet2.id[i]);
	}
	ASSERT_EQ(packet.getPayloadLength(), packet2.getPayloadLength());
	unsigned char* p1 = packet.getPayload();
	unsigned char* p2 = packet.getPayload();
	for (unsigned int i = 0; i < packet.getPayloadLength(); i++) {
		ASSERT_EQ(p1[i], p2[i]);
	}
	ASSERT_EQ(str, str2);
}

TEST_F(TestPacket, PingClass) {
	Ping obj;
	ASSERT_EQ(0x00, obj.pd);
	ASSERT_EQ(0, obj.getOptData().getOptDataLen());
	obj.initPayload(10);
	std::string str = obj.toString();
	ASSERT_EQ(23 + 10, str.length());
	ASSERT_EQ(10, obj.getOptData().getOptDataLen());
	Ping obj2;
	obj2.parseString(str);
	std::string str2 = obj2.toString();
	ASSERT_EQ(str, str2);
}

TEST_F(TestPacket, PongClass) {
	Pong obj;
	ASSERT_EQ(0x01, obj.pd);
	ASSERT_EQ(0, obj.getOptData().getOptDataLen());
	obj.port = 0x7788;
	obj.ip = 0x11223344;
	obj.num_files = 22;
	obj.num_size = 56789;
	ASSERT_EQ(14, obj.getPayloadLength());
	std::string str = obj.toString();
	for (size_t i = 0; i < str.length(); i++) {
		cout << setfill('0') << setw(2) << i;
	}
	cout << endl;
	cout << ToHex(str, true) << endl;
	ASSERT_EQ(0x88, (unsigned char )str[23]);
	ASSERT_EQ(0x11, (unsigned char )str[25]);
	ASSERT_EQ(23 + 14, str.length());
	obj.getOptData().initOptData(10);
	ASSERT_EQ(14 + 10, obj.getPayloadLength());
	str = obj.toString();
	for (size_t i = 0; i < str.length(); i++) {
		cout << setfill('0') << setw(2) << i;
	}
	cout << endl;
	cout << ToHex(str, true) << endl;
	ASSERT_EQ(23 + 14 + 10, str.length());
	ASSERT_EQ(10, obj.getOptData().getOptDataLen());

	Pong obj2;
	obj2.parseString(str);
	std::string str2 = obj2.toString();
	cout << ToHex(str, true) << endl;
	ASSERT_EQ(obj.port, obj2.port);
	ASSERT_EQ(obj.ip, obj2.ip);
	ASSERT_EQ(obj.num_files, obj2.num_files);
	ASSERT_EQ(obj.num_size, obj2.num_size);
	ASSERT_EQ(str, str2);
}

TEST_F(TestPacket, QueryClass) {
	Query obj;
	ASSERT_EQ(0x80, obj.pd);
	ASSERT_EQ(0, obj.getOptData().getOptDataLen());
	obj.min_speed = 1234;
	obj.criteria = "cplusplus";
	ASSERT_EQ(2 + obj.criteria.length() + 1, obj.getPayloadLength());
	std::string str = obj.toString();
	for (size_t i = 0; i < str.length(); i++) {
		cout << setfill('0') << setw(2) << i;
	}
	cout << endl;
	cout << ToHex(str, true) << endl;
	ASSERT_EQ(23 + 2 + obj.criteria.length() + 1, str.length());
	obj.getOptData().initOptData(10);
	ASSERT_EQ(2 + obj.criteria.length() + 1 + 10, obj.getPayloadLength());
	str = obj.toString();
	for (size_t i = 0; i < str.length(); i++) {
		cout << setfill('0') << setw(2) << i;
	}
	cout << endl;
	cout << ToHex(str, true) << endl;
	ASSERT_EQ(23 + 2 + obj.criteria.length() + 1 + 10, str.length());
	ASSERT_EQ(10, obj.getOptData().getOptDataLen());

	Query obj2;
	obj2.parseString(str);
	std::string str2 = obj2.toString();
	cout << ToHex(str, true) << endl;
	ASSERT_EQ(obj.min_speed, obj2.min_speed);
	ASSERT_EQ(obj.criteria, obj2.criteria);
	ASSERT_EQ(str, str2);
}

TEST_F(TestPacket, QueryResultClass) {
	QueryResult obj;
	obj.file_index = 123;
	obj.file_name = "learn java in 21 days.pdf";
	obj.file_size = 231234;
	int LEN = obj.file_name.length() + 4 + 4 + 2;
	ASSERT_EQ(LEN, obj.length());
	ASSERT_EQ(obj.length(), obj.toString().length());
	std::string str = obj.toString();
	for (size_t i = 0; i < str.length(); i++) {
		cout << setfill('0') << setw(2) << i;
	}
	cout << endl;
	cout << ToHex(str, true) << endl;
	obj.optdata.initOptData(10);
	ASSERT_EQ(LEN + 10, obj.length());
	str = obj.toString();
	for (size_t i = 0; i < str.length(); i++) {
		cout << setfill('0') << setw(2) << i;
	}
	cout << endl;
	cout << ToHex(str, true) << endl;

	QueryResult obj2;
	obj2.parseString(str);
	std::string str2 = obj2.toString();
	cout << ToHex(str2, true) << endl;
	ASSERT_EQ(obj.file_index, obj2.file_index);
	ASSERT_EQ(obj.file_name, obj2.file_name);
	ASSERT_EQ(obj.file_size, obj2.file_size);
	ASSERT_EQ(str, str2);
}

TEST_F(TestPacket, QueryHitsClass) {
	QueryHits obj;
	ASSERT_EQ(0x81, obj.pd);
	ASSERT_EQ(0, obj.QHDData.getOptDataLen());
	ASSERT_EQ(0, obj.resultSet.size());
	obj.num_hits = 2;
	obj.speed = 123123;
	obj.ip = 0x11223344;
	obj.port = 0x5566;

	QueryResult qobj1;
	qobj1.file_index = 123;
	qobj1.file_name = "learn java in 21 days.pdf";
	qobj1.file_size = 231234;

	QueryResult qobj2;
	qobj2.file_index = 32;
	qobj2.file_name = "learn c++ in 21 days.pdf";
	qobj2.file_size = 1231234;

	obj.resultSet.push_back(qobj1);
	obj.resultSet.push_back(qobj2);

	obj.servent_id[12] = 0x99;

	int LEN = 11 + qobj1.length() + qobj2.length() + 0 + 16;
	ASSERT_EQ(LEN, obj.getPayloadLength());
	std::string str = obj.toString();
	for (size_t i = 0; i < str.length(); i++) {
		cout << setfill('0') << setw(2) << i;
	}
	cout << endl;
	cout << ToHex(str, true) << endl;
	ASSERT_EQ(23 + LEN, str.length());
	ASSERT_EQ(str[23 + 1], 0x66);
	ASSERT_EQ(str[23 + 3], 0x44);
	ASSERT_EQ((unsigned char )str[str.length() - 4], 0x99);

	QueryHits obj2;
	obj2.parseString(str);
	std::string str2 = obj2.toString();
	cout << ToHex(str, true) << endl;
	ASSERT_EQ(obj.num_hits, obj2.num_hits);
	ASSERT_EQ(obj.speed, obj2.speed);
	ASSERT_EQ(obj.ip, obj2.ip);
	ASSERT_EQ(obj.port, obj2.port);
	ASSERT_EQ(obj.resultSet.size(), obj2.resultSet.size());
	ASSERT_EQ(str, str2);

	obj.QHDData.parseString("hello world!");
	obj2.parseString(obj.toString());
	ASSERT_EQ(obj.QHDData.toString(), "hello world!");
	ASSERT_EQ(obj.toString(), obj2.toString());

}

#define ASSERT_MYCHAR_EQ(a,b) ASSERT_EQ(std::string(a),std::string(b))
TEST_F(TestPacket, RunTimeTypeInfo) {

	QueryHits obj;
	QueryHits& q = obj;
	Packet& p = obj;
	printf("%s\n",typeid(obj).name());
	ASSERT_MYCHAR_EQ(typeid(obj).name(), typeid(QueryHits).name());
	ASSERT_MYCHAR_EQ(typeid(q).name(), typeid(QueryHits&).name());
	ASSERT_MYCHAR_EQ(typeid(p).name(), typeid(q).name());

	ASSERT_EQ(typeid(obj), typeid(QueryHits));
	ASSERT_EQ(typeid(q), typeid(QueryHits&));
	ASSERT_EQ(typeid(p), typeid(q));
}
