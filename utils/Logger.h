#include <fstream>
#include <iostream>
#include <string>
#include "../nvm_chip/flash_memory/Physical_Page_Address.h"
#include "../ssd/NVM_Transaction.h"
#include "../sim/Sim_Defs.h"

using namespace std;

namespace Utils
{
    enum class OptStep {BEGIN, END};
    class Logger
    {
        public:
            Logger(string log_path);
            ~Logger();
            void WriteLogger(sim_time_type time, SSD_Components::Transaction_Type opt, OptStep step, NVM::FlashMemory::Physical_Page_Address add);
            bool IsLoggerReady();
        private:
            fstream log_file;
    };
}