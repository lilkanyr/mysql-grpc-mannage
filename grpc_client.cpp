#include <grpcpp/grpcpp.h>
#include "mgrMysql.grpc.pb.h"
#include <iostream>
#include <string>
#include <memory>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using namespace db_operations;

class DBClient {
public:
    DBClient(std::shared_ptr<Channel> channel)
        : stub_(DBService::NewStub(channel)) {}

    // 执行数据库操作
    bool ExecuteOperation(DBRequest::OperationType operation, 
                         const std::string& name, int age) {
        DBRequest request;
        request.set_operation(operation);
        
        // 设置用户信息
        UserInfo* user_info = request.mutable_user_info();
        user_info->set_name(name);
        user_info->set_age(age);

        // 创建响应对象和上下文
        DBResponse response;
        ClientContext context;

        // 调用远程方法
        Status status = stub_->ExecuteOperation(&context, request, &response);

        // 检查调用状态
        if (!status.ok()) {
            std::cout << "RPC调用失败: " << status.error_message() << std::endl;
            return false;
        }

        // 打印响应结果
        std::cout << "操作" << (response.success() ? "成功" : "失败") << std::endl;
        if (!response.message().empty()) {
            std::cout << "消息: " << response.message() << std::endl;
        }

        return response.success();
    }

private:
    std::unique_ptr<DBService::Stub> stub_;
};

// 辅助函数：显示操作菜单
void showMenu() {
    std::cout << "\n=== 数据库操作菜单 ===" << std::endl;
    std::cout << "1. 插入用户信息" << std::endl;
    std::cout << "2. 更新用户信息" << std::endl;
    std::cout << "3. 删除用户信息" << std::endl;
    std::cout << "0. 退出程序" << std::endl;
    std::cout << "请输入您的选择: ";
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
    // 创建与服务器的连接
    std::string target_str = "localhost:50051";
    DBClient client(
        grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials())
    );

    std::cout << "已连接到服务器: " << target_str << std::endl;

    while (true) {
        showMenu();
        
        int choice;
        std::cin >> choice;
        
        if (choice == 0) {
            break;
        }

        std::string name;
        int age;

        std::cout << "请输入用户名: ";
        std::cin.ignore();
        std::getline(std::cin, name);
        
        std::cout << "请输入年龄: ";
        std::cin >> age;

        bool success = false;
        switch (choice) {
            case 1:
                std::cout << "\n正在插入用户信息..." << std::endl;
                success = client.ExecuteOperation(DBRequest::INSERT, name, age);
                break;
            case 2:
                std::cout << "\n正在更新用户信息..." << std::endl;
                success = client.ExecuteOperation(DBRequest::UPDATE, name, age);
                break;
            case 3:
                std::cout << "\n正在删除用户信息..." << std::endl;
                success = client.ExecuteOperation(DBRequest::DELETE, name, age);
                break;
            default:
                std::cout << "无效的选择！" << std::endl;
                continue;
        }

        std::cout << "操作" << (success ? "成功！" : "失败！") << std::endl;
    }

    std::cout << "正在关闭客户端..." << std::endl;
    return 0;
}