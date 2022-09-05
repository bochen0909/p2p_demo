/*
 * TestUtil.cpp
 *
 *  Created on: Nov 2, 2015
 *      Author: lizhen
 */

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "gtest/gtest.h"
#include "Util.h"

using namespace std;

class TestUtil: public ::testing::Test {

protected:
	TestUtil() {
	}

};

TEST_F(TestUtil, getLocalIP) {
	int ip = Util::getLocalIP();
	printf("local ip:%d\n", ip);
	struct in_addr ip_addr;
	ip_addr.s_addr = ip;
	printf("The IP address is %s\n", inet_ntoa(ip_addr));
}
