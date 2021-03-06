/**
 * Copyright (c) 2016 DeepCortex GmbH <legal@eventql.io>
 * Authors:
 *   - Paul Asmuth <paul@eventql.io>
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
#include "eventql/db/metadata_file.h"
#include "eventql/util/inspect.h"

namespace eventql {

MetadataFile::MetadataFile() {}

MetadataFile::MetadataFile(
    const SHA1Hash& transaction_id,
    uint64_t transaction_seq,
    KeyspaceType keyspace_type,
    const Vector<PartitionMapEntry>& partition_map,
    uint64_t flags) :
    flags_(flags),
    transaction_id_(transaction_id),
    transaction_seq_(transaction_seq),
    keyspace_type_(keyspace_type),
    partition_map_(partition_map) {}

const SHA1Hash& MetadataFile::getTransactionID() const {
  return transaction_id_;
}

uint64_t MetadataFile::getSequenceNumber() const {
  return transaction_seq_;
}

KeyspaceType MetadataFile::getKeyspaceType() const {
  return keyspace_type_;
}

const Vector<MetadataFile::PartitionMapEntry>&
    MetadataFile::getPartitionMap() const {
  return partition_map_;
}

MetadataFile::PartitionMapIter MetadataFile::getPartitionMapBegin() const {
  return partition_map_.begin();
}

MetadataFile::PartitionMapIter MetadataFile::getPartitionMapEnd() const {
  return partition_map_.end();
}

MetadataFile::PartitionMapIter MetadataFile::lookup(
    const String& key) const {
  if (partition_map_.empty()) {
    return partition_map_.end();
  }

  size_t low = 0;
  size_t high = partition_map_.size() - 1;
  while (low != high) {
    size_t mid = (low + high + 1) / 2;
    int cmp = compareKeys(partition_map_[mid].begin, key);
    if (cmp < 0) {
      low = mid;
    } else if (cmp > 0) {
      if (mid == 0) {
        return partition_map_.begin();
      }

      high = mid - 1;
    } else {
      return partition_map_.begin() + mid;
    }
  }

  return partition_map_.begin() + low;
}

MetadataFile::PartitionMapIter MetadataFile::getPartitionMapAt(
    const String& key) const {
  if (key.empty() || partition_map_.empty()) {
    return getPartitionMapEnd();
  }

  auto iter = lookup(key);
  if (flags_ & MFILE_FINITE) {
    if (compareKeys(iter->begin, key) <= 0 &&
        compareKeys(iter->end, key) > 0) {
      return iter;
    } else {
      return partition_map_.end();
    }
  } else {
    return iter;
  }
}

MetadataFile::PartitionMapIter MetadataFile::getPartitionMapRangeBegin(
    const String& begin) const {
  if (begin.empty() || partition_map_.empty()) {
    return getPartitionMapBegin();
  }

  auto iter = lookup(begin);
  if (flags_ & MFILE_FINITE) {
    if (iter == partition_map_.end() || compareKeys(iter->end, begin) > 0) {
      return iter;
    } else {
      return iter + 1;
    }
  } else {
    return iter;
  }
}

MetadataFile::PartitionMapIter MetadataFile::getPartitionMapRangeEnd(
    const String& end) const {
  if (end.empty() || partition_map_.empty()) {
    return getPartitionMapEnd();
  }

  auto iter = lookup(end);
  if (iter == partition_map_.end() || compareKeys(iter->begin, end) >= 0) {
    return iter;
  } else {
    return iter + 1;
  }
}

int MetadataFile::compareKeys(const String& a, const String& b) const {
  return comparePartitionKeys(keyspace_type_, a, b);
}

static Status decodeServerList(
    uint64_t version,
    Vector<MetadataFile::PartitionPlacement>* servers,
    InputStream* is) {
  auto n = is->readVarUInt();

  for (size_t i = 0; i < n; ++i) {
    MetadataFile::PartitionPlacement s;
    s.server_id = is->readLenencString();
    if (version >= 2) {
      s.placement_id = is->readUInt64();
    } else {
      s.placement_id = 0;
      is->readUInt64();
    }
    servers->emplace_back(s);
  }

  return Status::success();
}

Status MetadataFile::decode(InputStream* is) {
  // file format version
  auto version = is->readUInt32();
  if (version > kBinaryFormatVersion) {
    return Status(eIOError, "invalid file format version");
  }

  // flags
  flags_ = 0;
  if (version > 2) {
    flags_ = is->readVarUInt();
  }

  // transaction id
  is->readNextBytes(
      (char*) transaction_id_.mutableData(),
      transaction_id_.size());

  // transaction seq
  transaction_seq_ = is->readUInt64();

  // keyspace type
  keyspace_type_ = static_cast<KeyspaceType>(is->readUInt8());

  // partition map
  partition_map_.clear();
  auto pmap_size = is->readVarUInt();
  for (size_t i = 0; i < pmap_size; ++i) {
    PartitionMapEntry e;

    // begin
    e.begin = is->readLenencString();

    // end
    if (flags_ & MFILE_FINITE) {
      e.end = is->readLenencString();
    }

    // partition id
    is->readNextBytes(
        (char*) e.partition_id.mutableData(),
        e.partition_id.size());

    // servers
    auto rc = decodeServerList(version, &e.servers, is);
    if (!rc.isSuccess()) {
      return rc;
    }

    // servers joining
    decodeServerList(version, &e.servers_joining, is);
    if (!rc.isSuccess()) {
      return rc;
    }

    // servers leaving
    decodeServerList(version, &e.servers_leaving, is);
    if (!rc.isSuccess()) {
      return rc;
    }

    e.splitting = is->readUInt8() > 0;
    if (e.splitting) {
      // splitting
      switch (version) {
        case 1: {
          e.splitting = false;
          is->readLenencString();
          Vector<PartitionPlacement> tmp;
          if (!decodeServerList(version, &tmp, is).isSuccess()) {
            return rc;
          }
          if (!decodeServerList(version, &tmp, is).isSuccess()) {
            return rc;
          }
          break;
        }

        default:
        case 2: {
          // split_point
          e.split_point = is->readLenencString();

          // split_partition_id_low
          is->readNextBytes(
              (char*) e.split_partition_id_low.mutableData(),
              e.split_partition_id_low.size());

          // split_partition_id_high
          is->readNextBytes(
              (char*) e.split_partition_id_high.mutableData(),
              e.split_partition_id_high.size());

          // split_servers_low
          decodeServerList(version, &e.split_servers_low, is);
          if (!rc.isSuccess()) {
            return rc;
          }

          // split_servers_high
          decodeServerList(version, &e.split_servers_high, is);
          if (!rc.isSuccess()) {
            return rc;
          }
        }

        break;
      }

    }

    partition_map_.emplace_back(e);
  }

  return Status::success();
}

static Status encodeServerList(
    const Vector<MetadataFile::PartitionPlacement>& servers,
    OutputStream* os) {
  os->appendVarUInt(servers.size());
  for (const auto& s : servers) {
    os->appendLenencString(s.server_id);
    os->appendUInt64(s.placement_id);
  }

  return Status::success();
}

Status MetadataFile::encode(OutputStream* os) const  {
  // file format version
  os->appendUInt32(kBinaryFormatVersion);

  // file format version
  os->appendVarUInt(flags_);

  // transaction id
  os->write((const char*) transaction_id_.data(), transaction_id_.size());

  // transaction seq
  os->appendUInt64(transaction_seq_);

  // keyspace type
  os->appendUInt8(static_cast<uint8_t>(keyspace_type_));

  // partition map
  os->appendVarUInt(partition_map_.size());
  for (const auto& p : partition_map_) {
    // begin
    os->appendLenencString(p.begin);

    // end
    if (flags_ & MFILE_FINITE) {
      os->appendLenencString(p.end);
    }

    // partition id
    os->write((const char*) p.partition_id.data(), p.partition_id.size());

    // servers
    auto rc = encodeServerList(p.servers, os);
    if (!rc.isSuccess()) {
      return rc;
    }

    // servers_joining
    rc = encodeServerList(p.servers_joining, os);
    if (!rc.isSuccess()) {
      return rc;
    }

    // servers_leaving
    rc = encodeServerList(p.servers_leaving, os);
    if (!rc.isSuccess()) {
      return rc;
    }

    // splitting
    os->appendUInt8(p.splitting);
    if (p.splitting) {
      // split_point
      os->appendLenencString(p.split_point);

      // split_partition_id_low
      os->write(
          (const char*) p.split_partition_id_low.data(),
          p.split_partition_id_low.size());

      // split_partition_id_high
      os->write(
          (const char*) p.split_partition_id_high.data(),
          p.split_partition_id_high.size());

      // split_servers_low
      rc = encodeServerList(p.split_servers_low, os);
      if (!rc.isSuccess()) {
        return rc;
      }

      // split_servers_high
      rc = encodeServerList(p.split_servers_high, os);
      if (!rc.isSuccess()) {
        return rc;
      }
    }
  }

  return Status::success();
}

Status MetadataFile::computeChecksum(SHA1Hash* checksum) const {
  Buffer buf;
  auto os = BufferOutputStream::fromBuffer(&buf);
  auto rc = encode(os.get());
  if (!rc.isSuccess()) {
    return rc;
  }

  *checksum = SHA1::compute(buf.data(), buf.size());
  return Status::success();
}

uint64_t MetadataFile::getFlags() const {
  return flags_;
}

bool MetadataFile::hasFinitePartitions() const {
  return flags_ & MFILE_FINITE;
}

MetadataFile::PartitionMapEntry::PartitionMapEntry() : splitting(false) {}

int comparePartitionKeys(
    KeyspaceType keyspace_type,
    const String& a,
    const String& b) {
  switch (keyspace_type) {
    case KEYSPACE_STRING: {
      if (a < b) {
        return -1;
      } else if (a > b) {
        return 1;
      } else {
        return 0;
      }
    }

    case KEYSPACE_UINT64: {
      uint64_t a_uint = 0;
      uint64_t b_uint = 0;
      if (a.size() == sizeof(uint64_t)) {
        memcpy(&a_uint, a.data(), sizeof(uint64_t));
      }
      if (b.size() == sizeof(uint64_t)) {
        memcpy(&b_uint, b.data(), sizeof(uint64_t));
      }

      if (a_uint < b_uint) {
        return -1;
      } else if (a_uint > b_uint) {
        return 1;
      } else {
        return 0;
      }
    }
  }
}

String encodePartitionKey(
    KeyspaceType keyspace_type,
    const String& key) {
  switch (keyspace_type) {
    case KEYSPACE_STRING: {
      return key;
    }

    case KEYSPACE_UINT64: {
      uint64_t uint = 0;
      if (key.size() > 0) {
        try {
          uint = std::stoull(key);
        } catch (...) {
          RAISEF(kRuntimeError, "invalid partiiton key: >$0<", key);
        }
      }

      return String((const char*) &uint, sizeof(uint64_t));
    }
  }
}

String decodePartitionKey(
    KeyspaceType keyspace_type,
    const String& key) {
  switch (keyspace_type) {
    case KEYSPACE_STRING: {
      return key;
    }

    case KEYSPACE_UINT64: {
      if (key.size() == sizeof(uint64_t)) {
        uint64_t uint;
        memcpy((void*) &uint, key.data(), sizeof(uint64_t));
        return StringUtil::toString(uint);
      } else {
        return "";
      }
    }
  }
}

} // namespace eventql

