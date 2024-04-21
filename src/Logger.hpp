#include <iostream>
#include <string>


enum LogLevel {
    INFO,
    ERROR,
    WARNING,
    TRACE,
    DEBUG,
    FATAL
};


class Logger {
public:

    void log(std::string message, LogLevel level) {
        switch(level) {
            case LogLevel::ERROR:
                printf("\x1B[31m[ERROR]\033[0m\t");
                std::cout << message << '\n';
                
                break;
            case LogLevel::DEBUG:
                std::cout << "[DEBUG] " << message << '\n';
                break;
            case LogLevel::FATAL:
                std::cout << "[FATAL] " << message << '\n';
                break;
            case LogLevel::INFO:
                printf("\x1B[94m[INFO]\033[0m\t");
                std::cout << message << '\n';
                break;
            case LogLevel::TRACE:
                std::cout << "[TRACE] " << message << '\n';
                break;
            case LogLevel::WARNING:
                printf("\x1B[33m[WARN]\033[0m\t");
                std::cout << message << '\n';
                break;
        }
    }

};

Logger logger;