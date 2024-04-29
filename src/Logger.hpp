#include <iostream>
#include <string>


enum LogLevel {
    INFO,
    ERROR,
    WARNING,
    TRACE,
    DEBUG,
    FATAL,
    DEFAULT
};

template<typename Message>


void log(Message message, LogLevel level = LogLevel::DEFAULT) {
    switch(level) {
        case LogLevel::ERROR:
            printf("\x1B[31m[ERROR]\033[0m\t");
            std::cout << message << '\n';
            break;
        case LogLevel::DEBUG:
            std::cout << "[DEBUG]\t" << message << '\n';
            break;
        case LogLevel::FATAL:
            std::cout << "[FATAL]\t" << message << '\n';
            break;
        case LogLevel::INFO:
            printf("\x1B[92m[INFO]\033[0m\t");
            std::cout << message << '\n';
            break;
        case LogLevel::TRACE:
            std::cout << "[TRACE]\t" << message << '\n';
            break;
        case LogLevel::WARNING:
            printf("\x1B[33m[WARN]\033[0m\t");
            std::cout << message << '\n';
            break;
        default:
            std::cout << message << '\n';
    }
}
