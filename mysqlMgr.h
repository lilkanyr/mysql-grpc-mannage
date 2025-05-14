#pragma once
#include "mysqlDao.h"
#include "configMgr.h"
#include <memory>

class mysqlMgr {
public:
    mysqlMgr() {
        configMgr& config = configMgr::getInstance();
        std::string host = config.getValue("mysql", "host");
        std::string user = config.getValue("mysql", "user");
        std::string password = config.getValue("mysql", "password");
        std::string database = config.getValue("mysql", "database");
        std::string port = config.getValue("mysql", "port");
        
        mysqlPool_ = std::make_unique<mysqlDao>(host, user, password, database, port);
    }
    
    ~mysqlMgr() {}

    bool insert(const std::string& name, int age) {
        return mysqlPool_->executeInTransaction([&](sql::Connection* conn) {
            mysqlDao::QueryGuard pstmt(std::unique_ptr<sql::PreparedStatement>(
                conn->prepareStatement("INSERT INTO test (name, age) VALUES (?, ?)")
            ));
            pstmt->setString(1, name);
            pstmt->setInt(2, age);
            return pstmt->executeUpdate() > 0;
        });
    }

    bool update(const std::string& name, int age) {
        return mysqlPool_->executeInTransaction([&](sql::Connection* conn) {
            mysqlDao::QueryGuard pstmt(std::unique_ptr<sql::PreparedStatement>(
                conn->prepareStatement("UPDATE test SET age = ? WHERE name = ?")
            ));
            pstmt->setInt(1, age);
            pstmt->setString(2, name);
            return pstmt->executeUpdate() > 0;
        });
    }

    bool deleteData(const std::string& name, int age) {
        return mysqlPool_->executeInTransaction([&](sql::Connection* conn) {
            mysqlDao::QueryGuard pstmt(std::unique_ptr<sql::PreparedStatement>(
                conn->prepareStatement("DELETE FROM test WHERE name = ? AND age = ?")
            ));
            pstmt->setString(1, name);
            pstmt->setInt(2, age);
            return pstmt->executeUpdate() > 0;
        });
    }

private:
    std::unique_ptr<mysqlDao> mysqlPool_;
};
