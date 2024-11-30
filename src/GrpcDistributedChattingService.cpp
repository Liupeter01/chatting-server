#include <grpc/GrpcBalanceService.hpp>
#include <grpc/GrpcDistributedChattingService.hpp>
#include <config/ServerConfig.hpp>

gRPCDistributedChattingService::gRPCDistributedChattingService() {
  /*pass current server name as a parameter to the balance server, and returns
   * all peers*/
  auto response = gRPCBalancerService::getPeerGrpcServerLists(
      ServerConfig::get_instance()->GrpcServerName);

  if (response.error() !=
      static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {
    spdlog::error("[Balance Server] try retrieve peer servers' info failed!, "
                  "error code {}",
                  response.error());
    std::abort();
  }

  /*get server lists*/
  auto &peer_servers = response.lists();

  /*traversal server lists and create multiple DistributedChattingServicePool
   * according to host and port*/
  std::for_each(
      peer_servers.begin(), peer_servers.end(),
      [this](const message::ServerInfo &server) {
                      m_pools[server.name()] = std::make_shared<
                                stubpool::DistributedChattingServicePool>(server.host(),
                                          server.port());

      });
}

std::optional<std::shared_ptr<stubpool::DistributedChattingServicePool>>
gRPCDistributedChattingService::getTargetChattingServer(const std::string& server_name) {
          /*User input current server as remote grpc-server*/
          if (ServerConfig::get_instance()->GrpcServerName == server_name) {
                    return std::nullopt;
          }

          /*remote server name not exists*/
          auto it = m_pools.find(server_name);
          if (it == m_pools.end()) {
                    return std::nullopt;
          }
          return it->second;
}


message::AddNewFriendResponse 
gRPCDistributedChattingService::sendFriendRequest(const std::string& server_name, 
          const message::AddNewFriendRequest& req){
          grpc::ClientContext context;
          message::AddNewFriendResponse response;

          /*get the connection pool of this server*/
          auto server_op = getTargetChattingServer(server_name);

          //server not found
          if (!server_op.has_value()) {
                    spdlog::warn("[GRPC {} Service]: GRPC {} Not Found", ServerConfig::get_instance()->GrpcServerName, server_name);
                    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
                    return response;
          }

          /*get one connection stub from connection pool*/
          auto stub_op = server_op.value()->acquire_stub();

          //connection stub not found
          if (!stub_op.has_value()) {
                    spdlog::warn("[GRPC {} Service]: Connection Stub Parse Error!", ServerConfig::get_instance()->GrpcServerName);
                    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
                    return response;
          }

          grpc::Status status = stub_op.value().get()->SendFriendRequest(&context, req, &response);

          /*return this stub back*/
          server_op.value()->release(std::move(stub_op.value()));

          ///*error occured*/
          if (!status.ok()) {
                    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
          }
          return response;
}

message::AuthoriseResponse 
gRPCDistributedChattingService::confirmFriendRequest(const std::string& server_name, 
          const message::AuthoriseRequest& req) {
          grpc::ClientContext context;
          message::AuthoriseResponse response;

          /*get the connection pool of this server*/
          auto server_op = getTargetChattingServer(server_name);

          //server not found
          if (!server_op.has_value()) {
                    spdlog::warn("[GRPC {} Service]: GRPC {} Not Found", ServerConfig::get_instance()->GrpcServerName, server_name);
                    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
                    return response;
          }

          /*get one connection stub from connection pool*/
          auto pool = server_op.value();
          auto instance = pool->get_instance();
          auto stub_op = instance->acquire();

          //connection stub not found
          if (!stub_op.has_value()) {
                    spdlog::warn("[GRPC {} Service]: Connection Stub Parse Error!", ServerConfig::get_instance()->GrpcServerName);
                    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
                    return response;
          }

          grpc::Status status = stub_op.value().get()->ConfirmFriendRequest(&context, req, &response);

          /*return this stub back*/
          instance->release(std::move(stub_op.value()));

          ///*error occured*/
          if (!status.ok()) {
                    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
          }

          return response;
}