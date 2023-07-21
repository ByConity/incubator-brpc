// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// This file may have been modified by Bytedance Ltd. and/or its
// affiliates (“Bytedance's Modifications”). All Bytedance's
// Modifications are Copyright (2022) Bytedance Ltd. and/or its 
// affiliates.


#include <gflags/gflags.h>
#include <string>                                       // std::string
#include <set>                                          // std::set
#include "butil/string_printf.h"
#include "butil/third_party/rapidjson/document.h"
#include "butil/third_party/rapidjson/stringbuffer.h"
#include "butil/third_party/rapidjson/prettywriter.h"
#include "butil/time/time.h"
#include "bthread/bthread.h"
#include "brpc/log.h"
#include "brpc/channel.h"
#include "brpc/policy/file_naming_service.h"
#include "brpc/policy/consul_naming_service.h"


namespace brpc {
namespace policy {

DEFINE_string(consul_agent_addr, "http://127.0.0.1:2280",
              "The query string of request consul for discovering service.");
DEFINE_string(consul_service_discovery_url,
              "/v1/lookup/name",
              "The url of consul for discovering service.");
DEFINE_int32(consul_connect_timeout_ms, 200,
             "Timeout for creating connections to consul in milliseconds");
DEFINE_int32(consul_blocking_query_wait_secs, 60,
             "Maximum duration for the blocking request in secs.");
DEFINE_bool(consul_enable_degrade_to_file_naming_service, false,
            "Use local backup file when consul cannot connect");
DEFINE_string(consul_file_naming_service_dir, "",
    "When it degraded to file naming service, the file with name of the "
    "service name will be searched in this dir to use.");
DEFINE_int32(consul_retry_interval_ms, 500,
             "Wait so many milliseconds before retry when error happens");
DEFINE_int32(consul_polling_interval_secs, 10,
             "Wait so many seconds before polling consul again");

std::string RapidjsonValueToString(const BUTIL_RAPIDJSON_NAMESPACE::Value& value) {
    BUTIL_RAPIDJSON_NAMESPACE::StringBuffer buffer;
    BUTIL_RAPIDJSON_NAMESPACE::PrettyWriter<BUTIL_RAPIDJSON_NAMESPACE::StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}

std::string addBracketsIfIpv6(const std::string & host_name)
{
    std::string res = host_name;

    if ((host_name.find_first_of(':') != std::string::npos) &&
        (!host_name.empty()) &&
        (host_name.back() != ']'))
    {
        res = '[' + host_name + ']';
    }
    return res;
}

int ConsulNamingService::DegradeToOtherServiceIfNeeded(const char* service_name,
                                                       std::vector<ServerNode>* servers) {
    if (FLAGS_consul_enable_degrade_to_file_naming_service && !_backup_file_loaded) {
        _backup_file_loaded = true;
        const std::string file(FLAGS_consul_file_naming_service_dir + service_name);
        LOG(INFO) << "Load server list from " << file;
        FileNamingService fns;
        return fns.GetServers(file.c_str(), servers);
    }
    return -1;
}

std::string GetConsulAddr() {
  // if gflag is set, just use it
  gflags::CommandLineFlagInfo info;
  if (GetCommandLineFlagInfo("consul_agent_addr", &info) && !info.is_default) {
    return FLAGS_consul_agent_addr;
  }

  std::string agent_host = "127.0.0.1";
  int agent_port = 2280;

  // get ip from env
  auto envs = {"CONSUL_HTTP_HOST", "MY_HOST_IP", "TCE_HOST_IP", "MY_HOST_IPV6"};
  for (const char * env : envs) {
    const char* host = getenv(env);
    if (host && host[0]) {
      agent_host = host;
      break;
    }
  }

  // get port from env
  const char* port = getenv("CONSUL_HTTP_PORT");
  if (port && port[0]) {
    int tmp_port = std::stoi(port);
    if ((tmp_port > 0) && (tmp_port < 65536)) {
      agent_port = tmp_port;
    }
  }

  return "http://" + addBracketsIfIpv6(agent_host) + ":" + std::to_string(agent_port);
}

int ConsulNamingService::GetServers(const char* service_name,
                                    std::vector<ServerNode>* servers) {
    if (!_consul_connected) {
        ChannelOptions opt;
        opt.protocol = PROTOCOL_HTTP;
        opt.connect_timeout_ms = FLAGS_consul_connect_timeout_ms;
        opt.timeout_ms = (FLAGS_consul_blocking_query_wait_secs + 10) * butil::Time::kMillisecondsPerSecond;
        std::string consul_agent_addr = GetConsulAddr();
        if (_channel.Init(consul_agent_addr.c_str(), "rr", &opt) != 0) {
            LOG(ERROR) << "Fail to init channel to consul at "
                 << consul_agent_addr;
            return DegradeToOtherServiceIfNeeded(service_name, servers);
        }
        _consul_connected = true;
    }

    if (_consul_url.empty()) {
        std::string consul_url_parameter;
        char* my_host_ip = getenv("MY_HOST_IP");
        char* byted_host_ip = getenv("BYTED_HOST_IP");
        char* my_host_ipv6 = getenv("MY_HOST_IPV6");
        char* byted_host_ipv6 = getenv("BYTED_HOST_IPV6");
        bool is_ipv4 = (my_host_ip != NULL && strlen(my_host_ip) > 0) || (byted_host_ip != NULL && strlen(byted_host_ip) > 0);
        bool is_ipv6 = (my_host_ipv6 != NULL && strlen(my_host_ipv6) > 0) || (byted_host_ipv6 != NULL && strlen(byted_host_ipv6) > 0);
        if (is_ipv4 && is_ipv6) {
           consul_url_parameter = "&addr-family=dual-stack&unique=v6";
        } else if (is_ipv6) {
           consul_url_parameter = "&addr-family=v6";
        } else {
           consul_url_parameter = "&addr-family=v4";
        }

        _consul_url.append(FLAGS_consul_service_discovery_url)
               .append("?name=")
               .append(service_name)
               .append(consul_url_parameter);
    }

    servers->clear();
    std::string consul_url(_consul_url);

    Controller cntl;
    cntl.http_request().uri() = consul_url;
    _channel.CallMethod(NULL, &cntl, NULL, NULL, NULL);
    if (cntl.Failed()) {
        LOG(ERROR) << "Fail to access " << consul_url << ": "
                   << cntl.ErrorText();
        return DegradeToOtherServiceIfNeeded(service_name, servers);
    }

    // Sort/unique the inserted vector is faster, but may have a different order
    // of addresses from the file. To make assertions in tests easier, we use
    // set to de-duplicate and keep the order.
    std::set<ServerNode> presence;

    BUTIL_RAPIDJSON_NAMESPACE::Document services;
    services.Parse(cntl.response_attachment().to_string().c_str());
    if (!services.IsArray()) {
        LOG(ERROR) << "The consul's response for "
                   << service_name << " is not a json array";
        return -1;
    }

    for (BUTIL_RAPIDJSON_NAMESPACE::SizeType i = 0; i < services.Size(); ++i) {
        auto itr_host = services[i].FindMember("Host");
        if (itr_host == services[i].MemberEnd() || !itr_host->value.IsString()) {
            LOG(ERROR) << "No host info in node: "
                       << RapidjsonValueToString(services[i]);
            continue;
        }

        auto itr_port = services[i].FindMember("Port");
        if (itr_port == services[i].MemberEnd() || !itr_port->value.IsUint()) {
            LOG(ERROR) << "No port info in node: "
                       << RapidjsonValueToString(services[i]);
            continue;
        }

        butil::EndPoint end_point;
        std::string formated_host = addBracketsIfIpv6(services[i]["Host"].GetString());
        if (str2endpoint(formated_host.c_str(),
                         services[i]["Port"].GetUint(),
                         &end_point) != 0) {
            LOG(ERROR) << "Service with illegal address or port: "
                       << RapidjsonValueToString(services[i]);
            continue;
        }

        ServerNode node;
        node.addr = end_point;
        auto itr_tags = services[i].FindMember("Tags");
        if (itr_tags == services[i].MemberEnd() || !itr_tags->value.IsObject()) {
                LOG(ERROR) << "Service tags returned by consul is not object, service: "
                           << RapidjsonValueToString(services[i]);
                continue;
        } else {
                std::stringstream ss;

                for (auto itr = itr_tags->value.MemberBegin();
                     itr != itr_tags->value.MemberEnd(); ++itr) {
                        std::string tag(itr->name.GetString());

                        tag.append("=").append(itr->value.GetString());
                        node.tags.push_back(tag);
                }

                std::sort(node.tags.begin(), node.tags.end());
                for (auto t : node.tags) {
                        ss << t << ",";
                }
                node.tag = ss.str();
        }

        if (presence.insert(node).second) {
            servers->push_back(node);
        } else {
            RPC_VLOG << "Duplicated server=" << node;
        }
    }

    if (servers->empty() && !services.Empty()) {
        LOG(ERROR) << "All service about " << service_name
                   << " from consul is invalid, refuse to update servers";
          return -1;
    }

    RPC_VLOG << "Got " << servers->size()
             << (servers->size() > 1 ? " servers" : " server")
             << " from " << service_name;
    return 0;
}

int ConsulNamingService::RunNamingService(const char* service_name,
                                          NamingServiceActions* actions) {
    std::vector<ServerNode> servers;
    bool ever_reset = false;
    for (;;) {
        servers.clear();
        const int rc = GetServers(service_name, &servers);
        if (bthread_stopped(bthread_self())) {
            RPC_VLOG << "Quit NamingServiceThread=" << bthread_self();
            return 0;
        }
        if (rc == 0) {
            ever_reset = true;
            actions->ResetServers(servers);
        } else {
            if (!ever_reset) {
                // ResetServers must be called at first time even if GetServers
                // failed, to wake up callers to `WaitForFirstBatchOfServers'
                ever_reset = true;
                servers.clear();
                actions->ResetServers(servers);
            }
            if (bthread_usleep(std::max(FLAGS_consul_retry_interval_ms, 1) * butil::Time::kMillisecondsPerSecond) < 0) {
                if (errno == ESTOP) {
                    RPC_VLOG << "Quit NamingServiceThread=" << bthread_self();
                    return 0;
                }
                PLOG(FATAL) << "Fail to sleep";
                return -1;
            }
        }
        if (bthread_usleep(FLAGS_consul_polling_interval_secs * butil::Time::kMicrosecondsPerSecond) < 0) {
            if (errno == ESTOP) {
                RPC_VLOG << "Quit NamingServiceThread=" << bthread_self();
                return 0;
            }
            PLOG(FATAL) << "Fail to sleep";
            return -1;
        }
    }
    CHECK(false);
    return -1;
}


void ConsulNamingService::Describe(std::ostream& os,
                                   const DescribeOptions&) const {
    os << "consul";
    return;
}

NamingService* ConsulNamingService::New() const {
    return new ConsulNamingService;
}

void ConsulNamingService::Destroy() {
    delete this;
}

}  // namespace policy
} // namespace brpc
