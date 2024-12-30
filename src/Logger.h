#pragma once

#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>

namespace rgc
{

static const std::string RED = "\033[31m";
static const std::string GREEN = "\033[32m";
static const std::string YELLOW = "\033[33m";
static const std::string BLUE = "\033[34m";
static const std::string RESET = "\033[0m";


class Logger
{
public:
    Logger() :m_out(std::cout), m_err(std::cerr) {}

    Logger(std::string const &logFile) : 
        m_fileStream(logFile, std::ios::out | std::ios::app),
        m_out(m_fileStream),
        m_err(m_fileStream)
    {}

    ~Logger()
    {
        if (m_fileStream.is_open())
        {
            m_fileStream.close();
        }
    }

    void logMsg(std::string const &msg, std::chrono::system_clock::time_point const &now) const
    {
        log(m_out, msg, now, GREEN);
    }

    void logWarn(std::string const &msg, std::chrono::system_clock::time_point const &now) const
    {
        log(m_out, msg, now, YELLOW);
    }

    void logErr(std::string const &msg, std::chrono::system_clock::time_point const &now) const
    {
        log(m_err, msg, now, RED);
    }

    void logDebug(std::string const &msg, std::chrono::system_clock::time_point const &now) const
    {
        log(m_out, msg, now, BLUE);
    }

    static Logger makeLogger(std::string const &logFile)
    {
        return (logFile.empty()) ? Logger() : Logger(logFile);
    }

private:

    void log(std::ostream &stream, std::string const &msg, std::chrono::system_clock::time_point const &now, std::string color) const
    {
        auto duration = now.time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;
        std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm = *std::localtime(&now_time_t);

        stream 
            << color
            << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S.") 
            << std::setw(3) << std::setfill('0') << ms << " - " << msg << std::endl
            << RESET;
    }

    std::ofstream m_fileStream;
    std::ostream &m_out;
    std::ostream &m_err;
};

}
