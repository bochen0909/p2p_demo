/*
 * TestListener.cpp
 *
 *  Created on: Nov 2, 2015
 *      Author: lizhen
 */

#include <iostream>
#include "gtest/gtest.h"
#include "Listener.h"
#include "Util.h"

using namespace std;

class TestListener: public ::testing::Test {

protected:
	TestListener() {
	}

};

TEST_F(TestListener, DownloadRequest) {
	DownloadRequest dq;
	dq.fileindex = 1;
	strcpy(dq.filename, "abc.txt");
	strcpy(dq.connection, "keep-alive");
	std::string str1 = dq.toString();
	cout << str1 << endl;

	DownloadRequest dq2;
	dq2.parseFromString(str1);
	cout << dq2.toString() << endl;

	ASSERT_EQ(dq.fileindex, dq2.fileindex);
	ASSERT_EQ(std::string(dq.filename), std::string(dq2.filename));

}

TEST_F(TestListener, DownloadResponse) {
	DownloadResponse dq;
	strcpy(dq.retcode, "404");
	strcpy(dq.retdesc, "Not Found");
	dq.contentlength = 123456;
	dq.part_binary_len = 3;
	dq.part_binary[0] = 0x12;
	dq.part_binary[1] = 0x13;
	dq.part_binary[2] = 0x14;
	std::string str1 = dq.toString();
	cout << str1 << endl;

	DownloadResponse dq2;
	dq2.parseFromString(str1);
	cout << dq2.toString() << endl;

	ASSERT_EQ(dq.contentlength, dq2.contentlength);
	ASSERT_EQ(dq.part_binary_len, dq2.part_binary_len);
	ASSERT_EQ(dq.part_binary[0], dq2.part_binary[0]);
	ASSERT_EQ(dq.part_binary[1], dq2.part_binary[1]);
	ASSERT_EQ(dq.part_binary[2], dq2.part_binary[2]);
	ASSERT_EQ(std::string(dq.retcode), std::string(dq2.retcode));
	ASSERT_EQ(std::string(dq.retdesc), std::string(dq2.retdesc));
	ASSERT_EQ(std::string(dq.servername), std::string(dq2.servername));
	ASSERT_EQ(std::string(dq.contenttype), std::string(dq2.contenttype));
	ASSERT_EQ(std::string(dq.httpversion), std::string(dq2.httpversion));

}
