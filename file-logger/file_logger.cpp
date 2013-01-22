#include "settings.h"

#include "file_logger.h"

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include <boost/bind.hpp>

#include "fastcgi2/component_factory.h"
#include "fastcgi2/config.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi {

const size_t BUF_SIZE = 512;

FileLogger::FileLogger(ComponentContext *context) : Component(context),
        openMode_(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH),
        print_level_(true), print_time_(true),
        fd_(-1), stopping_(false),
        writingThread_(boost::bind(&FileLogger::writingThread, this))
{
    const Config *config = context->getConfig();
    const std::string componentXPath = context->getComponentXPath();

    filename_ = config->asString(componentXPath + "/file");
    setLevel(stringToLevel(config->asString(componentXPath + "/level")));

    print_level_ =
        (0 == strcasecmp(config->asString(componentXPath + "/print-level", "yes").c_str(), "yes"));

    print_time_ =
        (0 == strcasecmp(config->asString(componentXPath + "/print-time", "yes").c_str(), "yes"));

    time_format_ = config->asString(componentXPath + "/time-format", "[%Y/%m/%d %T]");

    std::string read = config->asString(componentXPath + "/read", "");
    if (!read.empty()) {
        if (read == "all") {
            openMode_ = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        }
        else if (read == "group") {
            openMode_ = S_IRUSR | S_IWUSR | S_IRGRP;
        }
        else if (read == "user") {
            openMode_ = S_IRUSR | S_IWUSR;
        }
    }

    std::string::size_type pos = 0;
    while (true) {
        pos = filename_.find('/', pos + 1);
        if (std::string::npos == pos) {
            break;
        }
        std::string name = filename_.substr(0, pos);
        int res = mkdir(name.c_str(), openMode_ | S_IXUSR | S_IXGRP | S_IXOTH);
        if (-1 == res && EEXIST != errno) {
            std::cerr << "failed to create dir: " << name << ". Errno: " << errno << std::endl;
        }
    }
    
    openFile();
}

FileLogger::~FileLogger() {
    stopping_ = true;
    queueCondition_.notify_one();
    writingThread_.join();

    if (fd_ != -1) {
        close(fd_);
    }
}

void
FileLogger::onLoad() {
}

void
FileLogger::onUnload() {
}

void FileLogger::openFile() {
    boost::mutex::scoped_lock fdLock(fdMutex_);
    if (fd_ != -1) {
        close(fd_);
    }
    fd_ = open(filename_.c_str(), O_WRONLY | O_CREAT | O_APPEND, openMode_);
    if (fd_ == -1) {
        std::cerr << "File logger cannot open file for writing: " << filename_ << std::endl;
    }
}

void
FileLogger::rollOver() {
    openFile();
}

void
FileLogger::log(const Logger::Level level, const char* format, va_list args) {
    if (level < getLevel()) {
        return;
    }

    // Check without lock!
    if (fd_ == -1) {
        return;
    }

    char fmt[BUF_SIZE];
    prepareFormat(fmt, sizeof(fmt), level, format);

    va_list tmpargs;
    va_copy(tmpargs, args);
    size_t size = vsnprintf(NULL, 0, fmt, tmpargs);
    va_end(tmpargs);

    if (size > 0) {
        std::vector<char> data(size + 1);
        vsnprintf(&data[0], size + 1, fmt, args);
        boost::mutex::scoped_lock lock(queueMutex_);
        queue_.push_back(std::string(data.begin(), data.begin() + size));
        queueCondition_.notify_one();
    }
}

void
FileLogger::prepareFormat(char * buf, size_t size, const Logger::Level level, const char* format) {
    char timestr[64];
    if (print_time_) {
        struct tm tm;
        time_t t;
        time(&t);
        localtime_r(&t, &tm);
        strftime(timestr, sizeof(timestr) - 1, time_format_.c_str(), &tm);
    }

    std::string level_str;
    if (print_level_) {
        level_str = levelToString(level);
    }


    if (print_time_ && print_level_) {
        snprintf(buf, size - 1, "%s %s: %s\n", timestr, level_str.c_str(), format);
    }
    else if (print_time_) {
        snprintf(buf, size - 1, "%s %s\n", timestr, format);
    }
    else if (print_level_) {
        snprintf(buf, size - 1, "%s: %s\n", level_str.c_str(), format);
    }
    else {
        snprintf(buf, size - 1, "%s\n", format);
    }
    buf[size - 1] = '\0';
}

void
FileLogger::writingThread() {
    while (!stopping_) {
        std::vector<std::string> queueCopy;
        {
            boost::mutex::scoped_lock lock(queueMutex_);
            queueCondition_.wait(lock);
            std::swap(queueCopy, queue_);
        }

        boost::mutex::scoped_lock fdlock(fdMutex_);
        if (fd_ != -1) {
            for (std::vector<std::string>::iterator i = queueCopy.begin(); i != queueCopy.end(); ++i) {
                ::write(fd_, i->c_str(), i->length());
            }
        }
    }
}

FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
FCGIDAEMON_ADD_DEFAULT_FACTORY("logger", fastcgi::FileLogger)
FCGIDAEMON_REGISTER_FACTORIES_END()

} // namespace xscript
