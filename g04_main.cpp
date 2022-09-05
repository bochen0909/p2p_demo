#include <stdio.h>
#include "G04.h"

/**
 * Entry for g04 program.
 */
int main(int argc, char** argv) {
	int exit_code = G04::instance().run();
	return exit_code;
}
