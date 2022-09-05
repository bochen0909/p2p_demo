/*
 * G04.h
 *
 *  Created on: Sep 27, 2015
 *      Author: Lizhen
 */

#ifndef G04_H_
#define G04_H_

#include <readline/readline.h>
#include <readline/history.h>
#include <vector>
#include <map>
#include <string>
#include "conf.h"

using namespace std;
class Listener;
/**
 * G04 class
 */
class G04 {
	friend class Listener;
public:
	/**
	 * return a singleton instance for the class
	 */
	static G04& instance();

	/**
	 * true if a command is built in command
	 */
	static bool isBuiltin(const std::string& name);

	/**
	 * run a build in command.
	 * The first argument is the command name.
	 */
	int runBuiltin(const std::vector<std::string>& args);

public:
	/**
	 * destructor
	 */
	virtual ~G04();

	/**
	 * start the shell
	 */
	int run();

	/** find the full path for a non-builtin command.
	 *  It search MYPATH variable first, then search the PATH variable.
	 */
	std::string findExternalCommandPath(const std::string& comname);

	friend char * command_generator(const char *text, int state);
	friend char ** myshell_completion(const char* text, int start, int end);

protected:
	/**
	 * Parse one line of user input. If no error, run it.
	 */
	int execute_line(const string& line);

	/**
	 * Buildin command
	 */
	int com_help(std::vector<std::string> arg);

	/**
	 * Buildin command
	 */
	int com_get(std::vector<std::string> arg);

	/**
	 * Buildin command
	 */
	int com_search(std::vector<std::string> ignore);

	/**
	 * Buildin command
	 */
	int com_external(std::vector<std::string> ignore);

	/**
	 * Buildin command
	 */
	int com_conn(std::vector<std::string> args);

	/**
	 * Buildin command
	 */
	int com_ping(std::vector<std::string> args);

	/**
	 * Buildin command
	 */
	int com_peers(std::vector<std::string> args);

	/**
	 * Buildin command
	 */
	int com_qrs(std::vector<std::string> args);
	/**
	 * Buildin command
	 */
	int com_quit(std::vector<std::string> arg);

	/**
	 * Buildin command
	 */
	int com_dump(std::vector<std::string> args);

	/**
	 * Buildin command
	 */
	int com_set(std::vector<std::string> arg);

	/**
	 * return the value of a variable.
	 * It searches local variables.
	 */
	std::string getvar(const std::string& varname);

	/**
	 * parse command line
	 */
	std::vector<string> parse(const std::string& line);

	/**
	 * start a new thread to listen peer's request,
	 * including connect/download/ping/pong/query/queryHits
	 */
	int startListner();

	/**
	 * read g04.conf configuration file
	 */
	int readConf();

protected:
	/**
	 * constructor
	 */
	G04(const char* progname);
protected:
	string m_progname;
	int done;
	char* line;
	std::map<std::string, std::string> variables;

	Listener* listener;

	std::vector<pthread_t> threads;

	Conf conf;

private:
	static G04* m_instance; //singleton

};

struct DownloadArgs {
	int idx;
	std::string localpath;
	Listener* listener;
};

#endif /* G04_H_ */
