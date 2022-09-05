Project: Demo of a peer-to-peer file share system.
==================
Demo of a peer-to-peer file share system.

Build
==================
In the project folder run _make_, for example 

	$ make
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb	 -DLOG_LEVEL=3 -DG04_INFO_STDOUT    -c -o g04_main.o g04_main.cpp
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb	 -DLOG_LEVEL=3 -DG04_INFO_STDOUT    -c -o G04.o G04.cpp
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb	 -DLOG_LEVEL=3 -DG04_INFO_STDOUT    -c -o Util.o Util.cpp
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb	 -DLOG_LEVEL=3 -DG04_INFO_STDOUT    -c -o Listener.o Listener.cpp
	g++ -Wall -ansi -pedantic -std=c++11 -ggdb	 -DLOG_LEVEL=3 -DG04_INFO_STDOUT    -c -o Packet.o Packet.cpp
	g++  g04_main.o G04.o Util.o Listener.o Packet.o -o g04 -lreadline -lpthread

	
To clean the build
	       
	$ make clean
	rm -fr g04  *.o *.x core


Quick Start 
==================
There is an **example** folder which contains 6 nodes folders: 2 seed nodes and 4 non-seed nodes.

Go to one of them and use __"../../g04"__ to start the node. 

Since a node shows command line for user to input. Multiple terminals have to be opened to start more nodes. 
	
Documentation
==================
Synopsis
--------

	g04

Description
-----------
g04 is a simple implementation of Gnutella-like p2p system. 

Start the program
-----------------
g04 reads configuration from __g04.conf__ file in the working folder. The file must exist to start the program. 
 
	$ g04
	Bind neighbor socket at port 18952
	Bind file socket at port 18953
	Using IP 128.186.120.36 for peers
	Finish starting listener thread tid=-413460736
	G04:

After the g04 is started, the main thread provides a command line  REPL interface for user's input. 
In addition g04 starts a listener thread and two ports are binded: neighbor port and file port.

The neighbor port listens peer's connection messages as well as Gnutella __ping__, __pong__, __query__, __queryHits__ messages.
All these messages follows Gnutella Protocol Specification v0.4 [http://rfc-gnutella.sourceforge.net/developer/stable/](http://rfc-gnutella.sourceforge.net/developer/stable/).  Especially  _ping_, _pong_, _query_, _queryHist_ messages are encoded in binary.

The file port listens the _get_ messages. A new thread is started for each get request since a download request may take a quite long time.  






g04 Commands 
==================
Get help

	G04: help
	help            Display information about built-in commands.
	search          Search files on G04 system.
	get             Download file referred by index in the search result.
	set             Set or display G04 variables
	dump            Dump internal information.
	conn            Connect to a peer.
	ping            Send a ping message to all peers.
	peers           Show peers.
	qrs             Show last query result set.
	!               Execute external shell command.
	exit            Exit G04.


Display configuration 

	G04: set
	NumberOfPeers=4
	TTL=5
	filePort=18953
	isSeedNode=1
	localFiles=local_file.conf
	localFilesDirectory=share
	neighborPort=18952
	seedNodes=seed_node.conf


Show peers
 
	G04: peers
	Peers(3/3):
	(0x7f000001) 127.0.0.1:17952, sockfd=6
	(0x7f000001) 127.0.0.1:35212, sockfd=7
	(0x7f000001) 127.0.0.1:35214, sockfd=8

Search a keyword. It lists the lidx(local result index for the query) and ridx(remote file index on peer machine). 

	G04: search object
	lidx ridx       size                 name     location
	
	   0    1      35933        cplusplus.txt    localhost:11953
	
	   1    1      35933        cplusplus.txt    localhost:23953
	   2    2     154749               oo.pdf    localhost:23953
	
	   3    1      35933        cplusplus.txt    localhost:13953
	   4    2     154749               oo.pdf    localhost:13953
	
	   5    1      35933        cplusplus.txt    localhost:18953
	   6    2     154749               oo.pdf    localhost:18953
	   7    4   77170082           big.tar.gz    localhost:18953
	
	   8    0      35933        cplusplus.txt    localhost:12953


Since the search is asynchronous, the search results may mixes with other output. Use _qrs_ to list the latest query results.

	G04: qrs
	keyword: object, total 9 records.
	0: 7f000101 127.0.1.1 11953 1 cplusplus.txt 35933
	1: 7f000101 127.0.1.1 23953 1 cplusplus.txt 35933
	2: 7f000101 127.0.1.1 23953 2 oo.pdf 154749
	3: 7f000101 127.0.1.1 13953 1 cplusplus.txt 35933
	4: 7f000101 127.0.1.1 13953 2 oo.pdf 154749
	5: 7f000101 127.0.1.1 18953 1 cplusplus.txt 35933
	6: 7f000101 127.0.1.1 18953 2 oo.pdf 154749
	7: 7f000101 127.0.1.1 18953 4 big.tar.gz 77170082
	8: 7f000101 127.0.1.1 12953 0 cplusplus.txt 35933


Use _get lidx_ to download a file from the previous search. The get command starts a new thread in background to download the file since it may take a long time.  For example, get big.tar.gz by

	G04: get 7
	7f000101 127.0.1.1 18953 4 big.tar.gz 77170082
	Downloading file big.tar.gz from 127.0.1.1:18953 to big.tar.gz...
	Finished downloading file.
	
	G04: !ls -alrt big*
	-rw-rw-r-- 1 shi shi 77170082 Nov 12 10:21 big.tar.gz
	
Other commands of ping, conn, dump are straightforward. They are provided for debug purpose. 

Format of Configuration Files
===============================
g04.conf format
-----------------
A sample configuration looks like

	neighborPort            =18952
	filePort                =18953
	NumberOfPeers           =4
	TTL                     =5
	seedNodes               =seed_node.conf
	isSeedNode              =1
	localFiles              =local_file.conf
	localFilesDirectory     =share

All the fields are required.  White spaces around "=" are trimmed. 

seed nodes conf format (e.g. seed_node.conf in the sample)
-------------------------------------------------------
hostname/ip and port number separated by space.  A sample configuration looks like

	localhost 17952
	localhost 18952

local shared file config format (e.g. local_file.conf in the sample)
-------------------------------------------------------
A sample configuration looks like

	theory.txt: theory|that|and|scientific|with|more|
	cplusplus.txt: type|programming|standard|features|classes|object
	oo.pdf: dynamic|features|classes|object|language|programming|variables

g04 checks the file existence when startup. Non-existing files are ignored.


Miscellaneous
=============
The __test__ folder has some unit tests code. The build is straightforward. However It requires [goolgetest](https://github.com/google/googletest) unit test framework installed. So it is not included in the **all** target.   

Restrictions
=============
* Only part of Gnutella Protocol Specification v0.4 is implemented.  
	
	- connection
	- get
	- ping
	- pong
	- query
	- queryHits




