#ifndef _FASTCGI_FILE_LOGGER_H_
#define _FASTCGI_FILE_LOGGER_H_

#include <vector>
#include <string>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>

#include "fastcgi2/component.h"
#include "fastcgi2/logger.h"

namespace fastcgi
{

class FileLogger : virtual public Logger, virtual public Component {
public:
    FileLogger(ComponentContext *context);
    ~FileLogger();

	virtual void onLoad();
	virtual void onUnload();

	virtual void log(const Logger::Level level, const char *format, va_list args);
private:
	virtual void rollOver();

private:
    // File name
    std::string filename_;

    // Open mode
    mode_t openMode_;

    // Time format specification
    std::string time_format_;

    bool print_level_;
    bool print_time_;

    // File descriptor
    int fd_;

    // Lock of file descriptor to avoid logrotate race-condition
    boost::mutex fdMutex_;


    // Writing queue.
    // All writes happens in separate thread. All someInternal methods just
    // push string into queue and signal conditional variable.

    // Logger is stopping.
    volatile bool stopping_;

    // Writing queue.
    std::vector<std::string> queue_;

    // Condition and mutex for signalling.
    boost::condition queueCondition_;
    boost::mutex queueMutex_;

    // Writing thread.
    boost::thread writingThread_;


    void openFile();
    void prepareFormat(char * buf, size_t size, const Logger::Level level, const char* format);

    void writingThread();
};

}

#endif // _FASTCGI_FILE_LOGGER_H_
