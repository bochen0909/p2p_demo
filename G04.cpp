/*
 * G04.cpp
 *
 *  Created on: Sep 27, 2015
 *      Author: LiZhen
 */

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include <setjmp.h>
#include <signal.h>
#include <string>
#include "Util.h"
#include "conf.h"
#include "Listener.h"
#include "G04.h"

static char* command_names[] = { (char*) "help", (char*) "search",
		(char*) "get", (char*) "set", (char*) "dump", (char*) "conn",
		(char*) "ping", (char*) "peers", (char*) "qrs", (char*) "!",
		(char*) "exit", (char*) 0 };
static char* command_docs[] = {
		(char*) "Display information about built-in commands.",
		(char*) "Search files on G04 system.",
		(char*) "Download file referred by index in the search result.",
		(char*) "Set or display G04 variables",
		(char*) "Dump internal information.", (char*) "Connect to a peer.",
		(char*) "Send a ping message to all peers.", (char*) "Show peers.",
		(char*) "Show last query result set.",
		(char*) "Execute external shell command.", (char*) "Exit G04.",
		(char*) 0 };

void* download_thread_run(void* arg);
void* listener_thread_run(void* arg);

int G04::com_help(std::vector<std::string> args) {
	if (args.size() > 2) {
		fprintf(stderr, "usage: help [command]\n");
		return -1;
	}

	register int i;
	for (i = 0; command_names[i]; i++) {
		if (args.size() == 1
				|| (strcmp(args[1].c_str(), command_names[i]) == 0)) {
			printf("%s\t\t%s\n", command_names[i], command_docs[i]);
		}
	}
	return (0);
}

int G04::com_get(std::vector<std::string> args) {
	if (args.size() != 2 && args.size() != 3) {
		fprintf(stderr, "usage: get index [local file path]\n");
		return -1;
	}
	int idx = atoi(args[1].c_str());
	std::string localpath = (args.size() == 2 ? "" : args[2]);

	DownloadArgs *da = new DownloadArgs();
	da->listener = listener;
	da->idx = idx;
	da->localpath = localpath;
	pthread_t thread;
	pthread_attr_t attr;
	/* For portability, explicitly create threads in a joinable state */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread, &attr, download_thread_run, (void *) da);
    pthread_attr_destroy(&attr);
	return (0);
}

int G04::com_search(std::vector<std::string> args) {
	if (args.size() != 2) {
		fprintf(stderr, "usage: search <keyword>\n");
		return -1;
	}
	listener->com_search(args[1]);
	return (0);
}

int G04::com_external(std::vector<std::string> args) {
	if (args[1].empty()) {
		fprintf(stderr, "usage: !<external command>\n");
		return -1;
	}
	int exit_code = system(args[1].c_str());
	return exit_code;
}

int G04::com_conn(std::vector<std::string> args) {
	if (args.size() != 3) {
		fprintf(stderr, "usage: conn <ip or hostname> <port>\n");
		return -1;
	}
	int port = atoi(args[2].c_str());
	int exit_code = this->listener->com_connect(args[1], port);
	return exit_code;
}

int G04::com_ping(std::vector<std::string> args) {
	if (args.size() != 1) {
		fprintf(stderr, "usage: ping\n");
		return -1;
	}
	int exit_code = this->listener->com_ping();
	return exit_code;
}

int G04::com_peers(std::vector<std::string> args) {
	if (args.size() != 1) {
		fprintf(stderr, "usage: peers\n");
		return -1;
	}
	int exit_code = this->listener->com_peers();
	return exit_code;
}

int G04::com_qrs(std::vector<std::string> args) {
	if (args.size() != 1) {
		fprintf(stderr, "usage: qrs\n");
		return -1;
	}
	int exit_code = this->listener->com_qrs();
	return exit_code;
}

int G04::com_dump(std::vector<std::string> args) {
	if (args.size() != 1) {
		fprintf(stderr, "usage: dump\n");
		return -1;
	}

	fprintf(stdout, "Internal variables:\n");
	std::map<std::string, std::string>::iterator itor;
	for (itor = variables.begin(); itor != variables.end(); itor++) {
		fprintf(stdout, "%s=%s\n", itor->first.c_str(), itor->second.c_str());
	}
	fprintf(stdout, "\n");
	int exit_code = this->listener->com_dump();
	return exit_code;
}

int G04::com_set(std::vector<std::string> arg) {
	if (arg.size() == 1) {
		std::map<std::string, std::string>::iterator itor;
		for (itor = variables.begin(); itor != variables.end(); itor++) {
			fprintf(stdout, "%s=%s\n", itor->first.c_str(),
					itor->second.c_str());
		}
	} else if (arg.size() == 2) {
		std::map<std::string, std::string>::iterator itor;
		std::vector<std::string> vec = Util::splitByDeli(arg[1], '=');
		std::string varname = vec[0];
		if (vec.size() == 1) {
			for (itor = variables.begin(); itor != variables.end(); itor++) {
				if (Util::startsWith(itor->first, varname))
					fprintf(stdout, "%s=%s\n", itor->first.c_str(),
							itor->second.c_str());
			}
		} else if (vec.size() == 2) {
			variables.erase(varname);
		} else {
			variables[varname] = vec[2];
		}
	} else {
		fprintf(stderr, "usage: set or set <varname>=<value>\n");
		return -1;
	}
	return (0);
}

/* The user wishes to quit using this program.  Just set DONE non-zero. */
int G04::com_quit(std::vector<std::string> arg) {
	done = 1;
	return 0;
}

std::string G04::getvar(const std::string& varname) {
	if (variables.find(varname) != variables.end()) {
		return variables[varname];
	} else {
		return "";
	}
}

char * command_generator(const char *text, int state) {
	static int list_index, len;
	char *name;

	/* If this is a new word to complete, initialize now.  This includes
	 saving the length of TEXT for efficiency, and initializing the index
	 variable to 0. */
	if (!state) {
		list_index = 0;
		len = strlen(text);
	}

	/* Return the next name which partially matches from the command list. */
	while ((name = command_names[list_index++])) {
		if (strncmp(name, text, len) == 0)
			return (Util::dupstr(name));
	}

	/* If no names matched, then return NULL. */
	return ((char *) NULL);
}

char ** myshell_completion(const char* text, int start, int end) {
	char **matches;

	matches = (char **) NULL;

	/* If this word is at the start of the line, then it is a command
	 to complete.  Otherwise it is the name of a file in the current
	 directory. */
	if (start == 0)
		matches = rl_completion_matches(text, command_generator);

	return (matches);
}

G04* G04::m_instance = (G04*) NULL;
G04::G04(const char* progname) :
		done(0), line(NULL), listener(0) {
	this->m_progname = progname;
}

G04::~G04() {
	if (line != NULL)
		free(line);
	if (listener) {
		delete listener;
	}
}

sigjmp_buf ctrlc_buf;

void sig_handler(int signo) {

	if (signo == SIGINT) {
		fprintf(stdout, "\n");
		siglongjmp(ctrlc_buf, 1);
	}
}

int G04::readConf() {
	const char* filename = "g04.conf";
	if (!Util::file_exists(filename)) {
		Util::log_error("Error! File %s does not exists\n", filename);
		return -1;
	}
	std::vector<std::string> lines = Util::readlines(filename);
	for (size_t i = 0; i < lines.size(); i++) {
		std::string str = Util::trim(lines[i]);
		if (str.empty())
			continue;
		std::vector<std::string> vec = Util::splitByDeli(str, '=');
		if (vec.size() < 3) {
			Util::log_error("Warning! Ignore line in %s: %s\n", filename,
					str.c_str());
			continue;
		}

		std::string name = vec[0];
		std::string value = vec[2];
		this->variables[name] = value;
		Util::log_debug("DEBUG: config: '%s' -> '%s'\n", name.c_str(),
				value.c_str());
	}

	std::string tmpstr;
	tmpstr = this->getvar("neighborPort");
	if (tmpstr.empty()) {
		Util::log_error("Error! neighborPort is not set.\n");
		return -1;
	} else {
		conf.neighborPort = atoi(tmpstr.c_str());
	}

	tmpstr = this->getvar("filePort");
	if (tmpstr.empty()) {
		Util::log_error("Error! filePort is not set.\n");
		return -1;
	} else {
		conf.filePort = atoi(tmpstr.c_str());
	}

	tmpstr = this->getvar("NumberOfPeers");
	if (tmpstr.empty()) {
		Util::log_error("Error! NumberOfPeers is not set.\n");
		return -1;
	} else {
		conf.NumberOfPeers = atoi(tmpstr.c_str());
	}

	tmpstr = this->getvar("TTL");
	if (tmpstr.empty()) {
		Util::log_error("Error! NumberOfPeers is not set.\n");
		return -1;
	} else {
		conf.TTL = atoi(tmpstr.c_str());
	}

	tmpstr = this->getvar("seedNodes");
	if (tmpstr.empty()) {
		Util::log_error("Error! seedNodes is not set.\n");
		return -1;
	} else {
		conf.seedNodes = tmpstr;
	}

	tmpstr = this->getvar("isSeedNode");
	if (tmpstr.empty()) {
		Util::log_error("Error! isSeedNode is not set. Should be 0 or 1\n");
		return -1;
	} else {
		conf.isSeedNode = atoi(tmpstr.c_str());
	}

	tmpstr = this->getvar("localFiles");
	if (tmpstr.empty()) {
		Util::log_error("Error! localFiles is not set.\n");
		return -1;
	} else {
		conf.localFiles = tmpstr;
	}

	tmpstr = this->getvar("localFilesDirectory");
	if (tmpstr.empty()) {
		Util::log_error("Error! localFilesDirectory is not set.\n");
		return -1;
	} else {
		conf.localFilesDirectory = tmpstr;
	}

	return 0;
}

int G04::run() {
	if (this->readConf() < 0) {
		return -1;
	}
	rl_readline_name = "G04";
	rl_attempted_completion_function = myshell_completion;

	rl_catch_signals = 1;
//	while ( sigsetjmp( ctrlc_buf, 1 ) != 0)
//		;
	signal(SIGINT, SIG_IGN);

	this->startListner();

	/* Loop reading and executing lines until the user quits. */
	for (; done == 0;) {
		char* line = readline("G04: ");

		if (!line)
			break;

		string s = Util::trim(line);
		if (!s.empty()) {
			add_history(s.c_str());
			execute_line(s);
		}
	}
	printf("\n");
	/* Wait for all threads to complete */
	bool allthreadstopped = false;
	listener->going_stop(true);
	for (int i = 0; i < 10; i++) {
		usleep(300 * 1000);
		if (listener->has_stopped()) {
			allthreadstopped = true;
			break;
		}
	}
	if (!allthreadstopped) {
		fprintf(stderr, "Stoping some threads timeout. Kill them.\n");
		for (size_t i = 0; i < threads.size(); i++) {
			//pthread_tryjoin_np(threads[i], NULL,timeout);
			pthread_kill(threads[i], SIGINT);
		}
	}

	for (size_t i = 0; i < threads.size(); i++) {
		//pthread_tryjoin_np(threads[i], NULL,timeout);
		pthread_join(threads[i], NULL);
	}
	return 0;
}

std::vector<string> G04::parse(const std::string& line) {
	if (Util::startsWith(line, "!")) {
		std::vector<std::string> vec;
		vec.push_back("!");
		vec.push_back(line.substr(1));
		return vec;
	} else {
		std::vector<std::string> vec = Util::splitcmdline(line);
		return vec;
	}
}

int G04::execute_line(const string& l) {
	std::vector<std::string> vec = parse(l);

	if (vec.size() == 0)
		return 0;
	string cmd = vec[0];
	if (0)
		if (!this->isBuiltin(cmd)) {
			fprintf(stderr, "%s command is not supported.\n", cmd.c_str());
			return 1;
		}
	int exit_code = this->runBuiltin(vec);
	return exit_code;
}

G04& G04::instance() {
	if (!G04::m_instance) {
		G04::m_instance = new G04("G04");
	}
	return *m_instance;
}

bool G04::isBuiltin(const std::string& name) {
	char* cmd;
	int i = 0;
	while ((cmd = command_names[i++]) != 0) {
		if (name == cmd) {
			return true;
		}
	}
	return false;
}

int G04::runBuiltin(const std::vector<std::string>& args) {
	string command = args[0];

	/* Call the function. */
	int exit_code = 0;
	if (command == "get") {
		exit_code = this->com_get(args);
	} else if (command == "search" || command == "query") {
		exit_code = this->com_search(args);
	} else if (command == "help" || command == "?") {
		exit_code = this->com_help(args);
	} else if (command == "exit") {
		exit_code = this->com_quit(args);
	} else if (command == "set") {
		exit_code = this->com_set(args);
	} else if (command == "dump") {
		exit_code = this->com_dump(args);
	} else if (command == "conn" || command == "connect") {
		exit_code = this->com_conn(args);
	} else if (command == "ping") {
		exit_code = this->com_ping(args);
	} else if (command == "peers") {
		exit_code = this->com_peers(args);
	} else if (command == "qrs") {
		exit_code = this->com_qrs(args);
	} else if (command == "!") {
		exit_code = this->com_external(args);
	} else {
		fprintf(stderr, "Command is not builtin: %s\n", command.c_str());
		exit_code = -1;
	}
	return exit_code;
}

void* download_thread_run(void* arg) {
	DownloadArgs *da = (DownloadArgs*) arg;
	int idx = da->idx;
	std::string localpath = da->localpath;
	delete da;
	da->listener->com_get(idx, localpath);
	pthread_exit(NULL);
}

void* listener_thread_run(void* arg) {
	Listener* listener = (Listener*) arg;
	listener->startlisten();
	pthread_exit(NULL);
}

int G04::startListner() {
	this->listener = new Listener(this);
	listener->initialize();

	pthread_t thread;
	pthread_attr_t attr;
	/* For portability, explicitly create threads in a joinable state */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&thread, &attr, listener_thread_run,
			(void *) this->listener);
    pthread_attr_destroy(&attr);
	this->threads.push_back(thread);
	Util::log_info("Finish starting listener thread tid=%d\n", thread);
	return 0;
}
