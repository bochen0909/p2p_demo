/*
 * conf.h
 *
 *  Created on: Nov 2, 2015
 *      Author: lizhen
 */

#ifndef CONF_H_
#define CONF_H_

#define	MAXLINE		4096	/* max text line length */
#define	LISTENQ		1024
#define	SERV_PORT		 9877

struct Conf {
	unsigned int neighborPort;
	unsigned int filePort;
	unsigned int NumberOfPeers;
	unsigned int TTL;
	std::string seedNodes;
	unsigned int isSeedNode;
	std::string localFiles;
	std::string localFilesDirectory;
};


#endif /* CONF_H_ */
