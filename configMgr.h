#pragma once
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <stdexcept>
#include <iostream>

class configMgr {
public:
    static configMgr& getInstance() {
        static configMgr instance;
        return instance;
    }

    std::string getValue(const std::string& section, const std::string& key) const {
        try {
            return pt.get<std::string>(section + "." + key);
        } catch (const std::exception& e) {
            std::cerr << "配置读取错误: " << section << "." << key << " - " << e.what() << std::endl;
            throw;
        }
    }

    void loadConfig(const std::string& filename = "config.ini") {
        try {
            boost::property_tree::ini_parser::read_ini(filename, pt);
            validateConfig();
        } catch (const std::exception& e) {
            std::cerr << "配置文件加载失败: " << e.what() << std::endl;
            throw;
        }
    }

private:
    configMgr() {
        loadConfig();
    }

    void validateConfig() {
        // 验证必需的配置项
        validateSection("mysql", {"host", "port", "user", "password", "database"});
        validateSection("grpc", {"host", "port"});
        
        // 验证端口号
        try {
            int mysql_port = pt.get<int>("mysql.port");
            int grpc_port = pt.get<int>("grpc.port");
            if (mysql_port <= 0 || mysql_port > 65535 ||
                grpc_port <= 0 || grpc_port > 65535) {
                throw std::runtime_error("端口号必须在 1-65535 范围内");
            }
        } catch (const boost::property_tree::ptree_bad_data& e) {
            throw std::runtime_error("端口号必须是有效的数字");
        }
    }

    void validateSection(const std::string& section, 
                        const std::vector<std::string>& required_keys) {
        for (const auto& key : required_keys) {
            if (pt.get<std::string>(section + "." + key, "").empty()) {
                throw std::runtime_error(section + " 部分缺少必需的配置项: " + key);
            }
        }
    }

    boost::property_tree::ptree pt;
};

