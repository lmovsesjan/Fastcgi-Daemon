#include "settings.h"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "fcgi_server.h"
#include "fastcgi2/config.h"
#include "details/globals.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

fastcgi::FCGIServer *server;

bool
daemonize() {
	pid_t pid = fork();
	if (pid == -1) {
		std::stringstream ss;
		ss << "Could not become a daemon: fork #1 failed: " << errno;
		throw std::logic_error(ss.str());
	}
	if (pid != 0) {
		_exit(0); // exit parent
	}

	pid_t sid = setsid();
	if (sid == -1) {
		std::stringstream ss;
		ss << "Could not become a daemon: setsid failed: " << errno;;
		throw std::logic_error(ss.str());
	}

	// check fork for child
	pid = fork();
	if (pid == -1) {
		std::stringstream ss;
		ss << "Could not become a daemon: fork #2 failed: " << errno;
		throw std::logic_error(ss.str());
	}
	if (pid != 0) {
		_exit(0); // exit session leader
	}

	for (int i = getdtablesize(); i--; ) {
		close(i);
	}
	umask(0002); // disable: S_IWOTH
	chdir("/");

	const char *devnull = "/dev/null";
	stdin = fopen(devnull, "a+");
	if (stdin == NULL) {
		return false;
	}
	stdout = fopen(devnull, "w");
	if (stdout == NULL) {
		return false;
	}
	stderr = fopen(devnull, "w");
	if (stderr == NULL) {
		return false;
	}
	return true;
}

void
signalHandler(int signo) {
	if ((SIGINT == signo || SIGTERM == signo) && ::server != NULL) {
		server->stop();
	}
}   

void
setUpSignalHandlers() {
	if (SIG_ERR == signal(SIGINT, signalHandler)) {
		throw std::runtime_error("Cannot set up SIGINT handler");
	} 
	if (SIG_ERR == signal(SIGTERM, signalHandler)) {
		throw std::runtime_error("Cannot set up SIGTERM handler");
	}
}

int
main(int argc, char *argv[]) {	
	using namespace fastcgi;
	try {
		for (int i = 1; i < argc; ++i) {
			if (!strcmp(argv[i], "--daemon")) {
				if (!daemonize()) {
					return EXIT_FAILURE;
				}
				break;
			}
		}

		boost::scoped_ptr<Config> config(Config::create(argc, argv));
		FCGIServer::writePid(*config);
		boost::shared_ptr<Globals> globals(new Globals(config.get()));
		FCGIServer server(globals);
		::server = &server;
		setUpSignalHandlers();
		server.start();
		server.join();
		return EXIT_SUCCESS;
	}
	catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}
