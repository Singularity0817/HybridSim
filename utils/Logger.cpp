#include "Logger.h"
#include <assert.h>

using namespace std;

namespace Utils{
    Logger::Logger(string log_path) {
        log_file.open(log_path, ios::out);
        assert(log_file.is_open());
    }

    Logger::~Logger(){
        log_file.close();
    }

    void Logger::WriteLogger(sim_time_type time, SSD_Components::Transaction_Type opt, OptStep step, NVM::FlashMemory::Physical_Page_Address add) {
        assert(log_file.is_open());
        log_file << time << ", ";
        switch (opt) {
        case SSD_Components::Transaction_Type::READ:
            log_file << "R" << ", ";
            break;
        case SSD_Components::Transaction_Type::WRITE:
            log_file << "W" << ", ";
            break;
        case SSD_Components::Transaction_Type::ERASE:
            log_file << "E" << ", ";
            break;
        default:
            log_file << "UNNAMED" << ", ";
        }
        switch (step) {
        case OptStep::BEGIN:
            log_file << "b" << ", ";
            break;
        case OptStep::END:
            log_file << "e" << ", ";
            break;
        default:
            log_file << "UNSTEP" << ", ";
        }
        log_file << add.ChannelID << ":" << add.ChipID << ":" << add.DieID << ":" << add.PlaneID << ":" << add.BlockID << ":" << add.PageID << endl;
    }

    bool Logger::IsLoggerReady() {
        return log_file.is_open();
    }
}