#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <string>

class SystemInfo {
public:
    std::string cpu_name() const;
    unsigned int logical_processors() const;
    unsigned int physical_cores() const;
    std::string operating_system() const;
    std::string compiler() const;
    std::string architecture() const;
    std::string cxx_standard() const;
    std::string optimization() const;
};

#endif
