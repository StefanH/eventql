/**
 * Copyright (c) 2016 zScale Technology GmbH <legal@zscale.io>
 * Authors:
 *   - Paul Asmuth <paul@zscale.io>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License ("the license") as
 * published by the Free Software Foundation, either version 3 of the License,
 * or any later version.
 *
 * In accordance with Section 7(e) of the license, the licensing of the Program
 * under the license does not imply a trademark license. Therefore any rights,
 * title and interest in our trademarks remain entirely with us.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You can be released from the requirements of the license by purchasing a
 * commercial license. Buying such a license is mandatory as soon as you develop
 * commercial activities involving this program without disclosing the source
 * code of your own applications
 */
#pragma once
#include "eventql/eventql.h"
#include "eventql/config/config_directory.h"
#include "eventql/util/protobuf/msg.h"

typedef struct _zhandle zhandle_t;

namespace eventql {

class StandaloneConfigDirectory : public ConfigDirectory {
public:

  StandaloneConfigDirectory();

  String getServerID() const override;

  ClusterConfig getClusterConfig() const override;

  void updateClusterConfig(ClusterConfig config) override;

  void setClusterConfigChangeCallback(
      Function<void (const ClusterConfig& cfg)> fn) override;

  ServerConfig getServerConfig(const String& sever_name) const override;

  void updateServerConfig(ServerConfig config) override;

  Vector<ServerConfig> listServers() const override;

  void setServerConfigChangeCallback(
      Function<void (const ServerConfig& cfg)> fn) override;

  RefPtr<NamespaceConfigRef> getNamespaceConfig(
      const String& customer_key) const override;

  void updateNamespaceConfig(NamespaceConfig config) override;

  void listNamespaces(
      Function<void (const NamespaceConfig& cfg)> fn) const override;

  void setNamespaceConfigChangeCallback(
      Function<void (const NamespaceConfig& cfg)> fn) override;

  TableDefinition getTableConfig(
      const String& db_namespace,
      const String& table_name) const override;

  void updateTableConfig(
      const TableDefinition& table,
      bool force = false) override;

  void listTables(
      Function<void (const TableDefinition& tbl)> fn) const override;

  void setTableConfigChangeCallback(
      Function<void (const TableDefinition& tbl)> fn) override;

  Status start() override;
  void stop() override;

protected:

  mutable std::mutex mutex_;
  ClusterConfig cluster_config_;
  HashMap<String, ServerConfig> servers_;
  HashMap<String, String> servers_live_;
  HashMap<String, NamespaceConfig> namespaces_;
  HashMap<String, TableDefinition> tables_;

  Vector<Function<void (const ClusterConfig& cfg)>> on_cluster_change_;
  Vector<Function<void (const ServerConfig& cfg)>> on_server_change_;
  Vector<Function<void (const NamespaceConfig& cfg)>> on_namespace_change_;
  Vector<Function<void (const TableDefinition& cfg)>> on_table_change_;

};

} // namespace eventql
