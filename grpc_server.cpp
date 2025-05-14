#include "mysqlDao.h"
#include "mysqlMgr.h"
#include "configMgr.h"
#include <grpcpp/grpcpp.h>
#include "mgrMysql.grpc.pb.h"
#include "mgrMysql.pb.h"
#include <iomanip>
#include <ctime>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using namespace db_operations;

// 获取当前时间的字符串
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_tm = std::localtime(&now_c);
    
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now_tm);
    return std::string(buffer);
}

// 打印操作日志
void logOperation(const std::string& operation, const UserInfo& user_info, 
                 bool success, const std::string& message) {
    std::cout << "=====================================" << std::endl;
    std::cout << "时间: " << getCurrentTime() << std::endl;
    std::cout << "操作: " << operation << std::endl;
    std::cout << "用户信息:" << std::endl;
    std::cout << "  姓名: " << user_info.name() << std::endl;
    std::cout << "  年龄: " << user_info.age() << std::endl;
    std::cout << "状态: " << (success ? "成功" : "失败") << std::endl;
    std::cout << "消息: " << message << std::endl;
    std::cout << "=====================================" << std::endl;
}

class DBServiceImpl final : public DBService::Service {
public:
    DBServiceImpl() {
        try {
            mysqlMgr_ = std::make_unique<mysqlMgr>();
            std::cout << "数据库管理器初始化成功" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "数据库管理器初始化失败: " << e.what() << std::endl;
            throw;
        }
    }

private:
    Status HandleInsert(const UserInfo& user_info, DBResponse* response) {
        try {
            if (mysqlMgr_->insert(user_info.name(), user_info.age())) {
                response->set_success(true);
                response->set_message("插入成功");
                logOperation("INSERT", user_info, true, "插入成功");
                return Status::OK;
            } else {
                response->set_success(false);
                response->set_message("插入失败");
                logOperation("INSERT", user_info, false, "插入失败");
                return Status(grpc::StatusCode::INTERNAL, "Database insert operation failed");
            }
        } catch (const std::exception& e) {
            response->set_success(false);
            response->set_message(std::string("插入异常: ") + e.what());
            logOperation("INSERT", user_info, false, e.what());
            return Status(grpc::StatusCode::INTERNAL, e.what());
        }
    }

    Status HandleUpdate(const UserInfo& user_info, DBResponse* response) {
        try {
            if (mysqlMgr_->update(user_info.name(), user_info.age())) {
                response->set_success(true);
                response->set_message("更新成功");
                logOperation("UPDATE", user_info, true, "更新成功");
                return Status::OK;
            } else {
                response->set_success(false);
                response->set_message("更新失败");
                logOperation("UPDATE", user_info, false, "更新失败");
                return Status(grpc::StatusCode::INTERNAL, "Database update operation failed");
            }
        } catch (const std::exception& e) {
            response->set_success(false);
            response->set_message(std::string("更新异常: ") + e.what());
            logOperation("UPDATE", user_info, false, e.what());
            return Status(grpc::StatusCode::INTERNAL, e.what());
        }
    }

    Status HandleDelete(const UserInfo& user_info, DBResponse* response) {
        try {
            if (mysqlMgr_->deleteData(user_info.name(), user_info.age())) {
                response->set_success(true);
                response->set_message("删除成功");
                logOperation("DELETE", user_info, true, "删除成功");
                return Status::OK;
            } else {
                response->set_success(false);
                response->set_message("删除失败");
                logOperation("DELETE", user_info, false, "删除失败");
                return Status(grpc::StatusCode::INTERNAL, "Database delete operation failed");
            }
        } catch (const std::exception& e) {
            response->set_success(false);
            response->set_message(std::string("删除异常: ") + e.what());
            logOperation("DELETE", user_info, false, e.what());
            return Status(grpc::StatusCode::INTERNAL, e.what());
        }
    }

public:
    Status ExecuteOperation(ServerContext* /*context*/, const DBRequest* request,
                          DBResponse* response) override {
        try {
            const UserInfo& user_info = request->user_info();
            
            switch(request->operation()) {
                case DBRequest::INSERT:
                    return HandleInsert(user_info, response);
                case DBRequest::UPDATE:
                    return HandleUpdate(user_info, response);
                case DBRequest::DELETE:
                    return HandleDelete(user_info, response);
                default:
                    response->set_success(false);
                    response->set_message("未知操作类型");
                    return Status(grpc::StatusCode::INVALID_ARGUMENT, "Unknown operation type");
            }
        } catch (const std::exception& e) {
            response->set_success(false);
            response->set_message(std::string("操作异常: ") + e.what());
            return Status(grpc::StatusCode::INTERNAL, e.what());
        }
    }

private:
    std::unique_ptr<mysqlMgr> mysqlMgr_;
};

int main(int /*argc*/, char** /*argv*/) {
    try {
        std::string server_address = "0.0.0.0:50051";
        DBServiceImpl service;

        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);

        std::unique_ptr<Server> server(builder.BuildAndStart());
        std::cout << "服务器正在监听: " << server_address << std::endl;

        server->Wait();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "服务器启动失败: " << e.what() << std::endl;
        return 1;
    }
}