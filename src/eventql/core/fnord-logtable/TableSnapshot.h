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
#ifndef _FNORD_LOGTABLE_TABLESNAPSHOT_H
#define _FNORD_LOGTABLE_TABLESNAPSHOT_H
#include <eventql/util/stdtypes.h>
#include <eventql/util/autoref.h>
#include <eventql/util/random.h>
#include <eventql/util/protobuf/MessageSchema.h>
#include <eventql/util/protobuf/MessageObject.h>
#include <fnord-logtable/TableArena.h>

namespace util {
namespace logtable {

struct TableChunkRef {
  String replica_id;
  String chunk_id;
  uint64_t start_sequence;
  uint64_t num_records;
  uint64_t sstable_checksum;
  uint64_t cstable_checksum;
  uint64_t summary_checksum;
  uint64_t sstable_size;
  uint64_t cstable_size;
  uint64_t summary_size;
};

struct TableGeneration : public RefCounted {
  String table_name;
  uint64_t generation;
  Vector<TableChunkRef> chunks;

  TableGeneration();
  RefPtr<TableGeneration> clone() const;
  void encode(Buffer* buf);
  void decode(const Buffer& buf);
};

struct TableSnapshot : public RefCounted {
  TableSnapshot(
      RefPtr<TableGeneration> _head = new TableGeneration(),
      List<RefPtr<TableArena>> _arenas = List<RefPtr<TableArena>>());

  RefPtr<TableGeneration> head;
  List<RefPtr<TableArena>> arenas;
};

} // namespace logtable
} // namespace util

#endif
