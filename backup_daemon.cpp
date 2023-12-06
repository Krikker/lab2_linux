#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <syslog.h>
#include <fstream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <ctime>
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <boost/bind.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace fs = std::filesystem;

void performBackup(const std::string& sourceDir, const std::string& backupDir) {
    try {
        fs::path sourcePath(sourceDir);
        fs::path backupPath(backupDir);

        if (!fs::exists(sourcePath) || !fs::is_directory(sourcePath)) {
            syslog(LOG_ERR, "Исходный каталог не существует или не является директорией");
            return;
        }

        if (!fs::exists(backupPath)) {
            fs::create_directories(backupPath);
            syslog(LOG_INFO, "Создан новый каталог для резервных копий");
        }

        std::time_t now = std::time(nullptr);
        std::tm timeinfo = *std::localtime(&now);

        std::stringstream timestamp;
        timestamp << std::put_time(&timeinfo, "%H-%M-%S");

        for (const auto& entry : fs::directory_iterator(sourcePath)) {
            fs::path backupFile = backupPath / (entry.path().filename().string() + "_backup_" + timestamp.str());
            fs::copy_file(entry.path(), backupFile, fs::copy_options::overwrite_existing);
            syslog(LOG_INFO, "Скопирован файл: %s", entry.path().filename().c_str());
        }
    } catch (const std::exception& e) {
        syslog(LOG_ERR, "Ошибка при выполнении резервного копирования: %s", e.what());
    }
}

void readConfig(const std::string& configFile) {
    try {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(configFile, pt);

        std::string sourceDir = pt.get<std::string>("source_directory");
        std::string backupDir = pt.get<std::string>("backup_directory");
        int backupFrequency = pt.get<int>("backup_frequency_minutes");

        while (true) {
            performBackup(sourceDir, backupDir);
            std::this_thread::sleep_for(std::chrono::minutes(backupFrequency));
        }
    } catch (std::exception const& e) {
        std::cerr << "Ошибка при чтении конфигурации: " << e.what() << std::endl;
    }
}

int main() {
    openlog("backup_daemon", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Демон резервного копирования запущен");

    readConfig("/etc/backup_config.json");

    closelog();
    return 0;
}
