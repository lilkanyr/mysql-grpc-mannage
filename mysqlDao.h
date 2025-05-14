#pragma once
#include <iostream>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/connection.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
#include <cppconn/prepared_statement.h>
#include <memory>
#include <vector>
#include <mutex>
#include <queue>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <functional>

class SqlConnection {
public:
    SqlConnection(sql::Connection* conn, long long time) 
        : conn_(conn), time_(time) {}
    
    ~SqlConnection() {
        if (conn_) {
            conn_->close();
            delete conn_;
        }
    }
    
    sql::Connection* conn_;
    long long time_;
};

class mysqlDao {
public:
    mysqlDao(const std::string &host, const std::string &user, 
             const std::string &password, const std::string &database, 
             const std::string &port)
        : host_(host), user_(user), password_(password), 
          database_(database), port_(port), 
          nums_(0), stop_(false),  // 修复初始化顺序
          driver_(get_driver_instance())
    {
        try {
            std::string connection_url = "tcp://" + host_ + ":" + port_;
            std::cout << "正在连接数据库: " << connection_url << std::endl;
            
            for (int i = 0; i < max_conn_num_; ++i) {
                try {
                    sql::Connection* conn = driver_->connect(connection_url, user_, password_);
                    conn->setSchema(database_);
                    conn_queue_.push(std::make_unique<SqlConnection>(conn, getCurrentTime()));
                    std::cout << "成功创建连接 #" << (i + 1) << std::endl;
                } catch (const sql::SQLException &e) {
                    std::cerr << "连接 #" << (i + 1) << " 创建失败: " << e.what() << std::endl;
                    std::cerr << "错误代码: " << e.getErrorCode() << std::endl;
                    std::cerr << "SQL状态: " << e.getSQLState() << std::endl;
                }
            }
            
            if (conn_queue_.empty()) {
                throw std::runtime_error("无法创建任何数据库连接");
            }

            keep_alive_thread_ = std::thread([this]() {
                while (!stop_) {
                    std::this_thread::sleep_for(std::chrono::seconds(60));
                    keepAlive();
                }
            });
            keep_alive_thread_.detach();
        } catch (const std::exception &e) {
            std::cerr << "数据库初始化错误: " << e.what() << std::endl;
            throw;
        }
    }

    ~mysqlDao() {
        stop_ = true;
        if (keep_alive_thread_.joinable()) {
            keep_alive_thread_.join();
        }
        std::unique_lock<std::mutex> lock(conn_mutex_);
        while (!conn_queue_.empty()) {
            conn_queue_.pop();
        }
    }

    std::unique_ptr<SqlConnection> getConnection() {
        std::unique_lock<std::mutex> lock(conn_mutex_);
        if (!conn_queue_.empty()) {
            auto conn = std::move(conn_queue_.front());
            conn_queue_.pop();
            return conn;
        }
        return nullptr;
    }

    void releaseConnection(std::unique_ptr<SqlConnection> conn) {
        std::unique_lock<std::mutex> lock(conn_mutex_);
        if (conn) {
            conn_queue_.push(std::move(conn));
            conn_cond_.notify_one();
        }
    }

    class Transaction {
    public:
        Transaction(std::unique_ptr<SqlConnection> conn) : conn_(std::move(conn)) {
            if (conn_) {
                conn_->conn_->setAutoCommit(false);
            }
        }
        
        ~Transaction() {
            if (conn_) {
                try {
                    conn_->conn_->rollback();
                } catch (...) {}
                conn_->conn_->setAutoCommit(true);
            }
        }
        
        void commit() {
            if (conn_) {
                conn_->conn_->commit();
            }
        }
        
        sql::Connection* get() {
            return conn_ ? conn_->conn_ : nullptr;
        }
        
    private:
        std::unique_ptr<SqlConnection> conn_;
    };

    template<typename T>
    class QueryGuard {
    public:
        QueryGuard(std::unique_ptr<T> query) : query_(std::move(query)) {}
        ~QueryGuard() {
            if (query_) {
                try {
                    sql::ResultSet* rs = query_->getResultSet();
                    if (rs) {
                        while (rs->next()) {} // 消费所有结果
                        delete rs;
                    }
                    while (query_->getMoreResults()) {
                        rs = query_->getResultSet();
                        if (rs) {
                            while (rs->next()) {} // 消费所有结果
                            delete rs;
                        }
                    }
                } catch (...) {} // 忽略清理过程中的错误
            }
        }
        
        T* get() { return query_.get(); }
        T* operator->() { return query_.get(); }
        
    private:
        std::unique_ptr<T> query_;
    };

    Transaction beginTransaction() {
        return Transaction(getConnection());
    }

    template<typename Func>
    bool executeInTransaction(Func&& func) {
        auto transaction = beginTransaction();
        if (!transaction.get()) {
            return false;
        }
        
        try {
            if (func(transaction.get())) {
                transaction.commit();
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "事务执行失败: " << e.what() << std::endl;
        }
        return false;
    }

private:
    long long getCurrentTime() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    // 添加重试相关常量
    static const int MAX_RETRY_ATTEMPTS = 3;
    static const int RETRY_DELAY_MS = 1000;

    bool tryReconnect(sql::Connection* conn) {
        for (int attempt = 1; attempt <= MAX_RETRY_ATTEMPTS; ++attempt) {
            try {
                std::cout << "尝试重新连接 (第 " << attempt << " 次)" << std::endl;
                
                // 先关闭旧连接
                if (conn) {
                    try {
                        conn->close();
                    } catch (...) {}
                    delete conn;
                }

                // 创建新连接
                std::string connection_url = "tcp://" + host_ + ":" + port_;
                conn = driver_->connect(connection_url, user_, password_);
                conn->setSchema(database_);
                
                // 测试连接
                std::unique_ptr<sql::Statement> test_stmt(conn->createStatement());
                test_stmt->execute("SELECT 1");
                
                std::cout << "重新连接成功" << std::endl;
                return true;
            } catch (const sql::SQLException& e) {
                std::cerr << "重新连接尝试 " << attempt << " 失败: " << e.what() << std::endl;
                std::cerr << "错误代码: " << e.getErrorCode() << std::endl;
                std::cerr << "SQL状态: " << e.getSQLState() << std::endl;
                
                if (attempt < MAX_RETRY_ATTEMPTS) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                }
            } catch (const std::exception& e) {
                std::cerr << "重新连接时发生未知错误: " << e.what() << std::endl;
                if (attempt < MAX_RETRY_ATTEMPTS) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                }
            }
        }
        return false;
    }

    void keepAlive() {
        std::unique_lock<std::mutex> lock(conn_mutex_);
        auto curr_time = getCurrentTime();
        std::queue<std::unique_ptr<SqlConnection>> temp_queue;
        int reconnect_count = 0;
        bool had_failures = false;
        
        while (!conn_queue_.empty()) {
            auto conn = std::move(conn_queue_.front());
            conn_queue_.pop();
            
            try {
                // 执行简单查询来保持连接
                std::unique_ptr<sql::Statement> stmt(conn->conn_->createStatement());
                stmt->execute("SELECT 1");
                conn->time_ = curr_time;
                temp_queue.push(std::move(conn));
            } catch (const sql::SQLException& e) {
                had_failures = true;
                std::cerr << "连接保活失败: " << e.what() << std::endl;
                
                sql::Connection* new_conn = nullptr;
                if (tryReconnect(new_conn)) {
                    temp_queue.push(std::make_unique<SqlConnection>(new_conn, curr_time));
                    reconnect_count++;
                }
            }
        }
        
        conn_queue_ = std::move(temp_queue);
        
        if (had_failures) {
            std::cout << "保活周期完成 - 重新连接成功数: " << reconnect_count 
                     << ", 当前活动连接数: " << conn_queue_.size() << "/" << max_conn_num_ << std::endl;
            
            // 如果连接池严重不足，尝试补充
            while (conn_queue_.size() < static_cast<std::size_t>(max_conn_num_ / 2)) {
                try {
                    std::string connection_url = "tcp://" + host_ + ":" + port_;
                    sql::Connection* new_conn = driver_->connect(connection_url, user_, password_);
                    new_conn->setSchema(database_);
                    conn_queue_.push(std::make_unique<SqlConnection>(new_conn, curr_time));
                    std::cout << "补充了新的连接" << std::endl;
                } catch (const sql::SQLException& e) {
                    std::cerr << "补充新连接失败: " << e.what() << std::endl;
                    break;
                }
            }
        }
    }

private:
    const int max_conn_num_ = 5;
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    std::string port_;
    std::atomic<int> nums_;
    std::atomic<bool> stop_;
    sql::Driver* driver_;
    
    std::queue<std::unique_ptr<SqlConnection>> conn_queue_;
    std::mutex conn_mutex_;
    std::condition_variable conn_cond_;
    std::thread keep_alive_thread_;
};
