/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#define USING_LOG_PREFIX SHARE
#include <time.h>
#include "ob_backup_struct.h"
#include "lib/utility/ob_defer.h"
#include "lib/alloc/alloc_assist.h"
#include "lib/utility/utility.h"
#include "common/ob_record_header.h"
#include "share/config/ob_server_config.h"
#include "share/schema/ob_multi_version_schema_service.h"
#include "share/backup/ob_backup_path.h"
#include "share/backup/ob_backup_config.h"
#include "share/backup/ob_backup_lease_info_mgr.h"
#include "storage/tx/ob_i_ts_source.h"
#include "storage/tx/ob_ts_mgr.h"


using namespace oceanbase;
using namespace lib;
using namespace common;
using namespace share;

bool share::is_valid_backup_inner_table_version(const ObBackupInnerTableVersion &version)
{
  return version > 0 && version < OB_BACKUP_INNER_TABLE_VMAX;
}

OB_SERIALIZE_MEMBER(ObPhysicalRestoreBackupDestList,
  backup_set_path_list_,
  backup_piece_path_list_,
  log_path_list_);

ObPhysicalRestoreBackupDestList::ObPhysicalRestoreBackupDestList()
  : allocator_(),
    backup_set_path_list_(),
    backup_piece_path_list_(),
    log_path_list_()
{
}

ObPhysicalRestoreBackupDestList::~ObPhysicalRestoreBackupDestList()
{
}

int ObPhysicalRestoreBackupDestList::assign(const ObPhysicalRestoreBackupDestList &list)
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(backup_set_path_list_.assign(list.backup_set_path_list_))) {
    LOG_WARN("failed to assign backup set path list", KR(ret));
  } else if (OB_FAIL(backup_piece_path_list_.assign(list.backup_piece_path_list_))) {
    LOG_WARN("failed to assign backup piece path list", KR(ret));
  } else if (OB_FAIL(log_path_list_.assign(list.log_path_list_))) {
    LOG_WARN("failed to assign log path list", KR(ret));
  } else if (OB_FAIL(backup_set_desc_list_.assign(list.backup_set_desc_list_))) {
    LOG_WARN("failed to assign backup set desc list", K(ret));
  }
  return ret;
}

int ObPhysicalRestoreBackupDestList::set(
    const common::ObIArray<share::ObRestoreBackupSetBriefInfo> &backup_set_list,
    const common::ObIArray<share::ObBackupPiecePath> &backup_piece_list,
    const common::ObIArray<share::ObBackupPathString> &log_path_list)
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(backup_piece_path_list_.assign(backup_piece_list))) {
    LOG_WARN("failed to assign", KR(ret));
  } else if (OB_FAIL(log_path_list_.assign(log_path_list))) {
    LOG_WARN("failed to assign", KR(ret));
  } else {
    ARRAY_FOREACH_X(backup_set_list, i, cnt, OB_SUCC(ret)) {
      const share::ObRestoreBackupSetBriefInfo &info = backup_set_list.at(i);
      if (OB_FAIL(backup_set_path_list_.push_back(info.backup_set_path_))) {
        LOG_WARN("fail to push back backup set path", K(ret));
      } else if (OB_FAIL(backup_set_desc_list_.push_back(info.backup_set_desc_))) {
        LOG_WARN("fail to push back backup set desc", K(ret));
      }
    }
  }
  return ret;
}

void ObPhysicalRestoreBackupDestList::reset()
{
}

int ObPhysicalRestoreBackupDestList::get_backup_set_brief_info_list(
    common::ObIArray<share::ObRestoreBackupSetBriefInfo> &backup_set_list)
{
  int ret = OB_SUCCESS;
  backup_set_list.reset();
  if (backup_set_desc_list_.count() != backup_set_path_list_.count()) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("backup set desc list is mismatched with backup set path list", K(ret),
        K(backup_set_desc_list_), K(backup_set_path_list_));
  } else {
    share::ObRestoreBackupSetBriefInfo tmp_info;
    int64_t cnt = backup_set_desc_list_.count();
    for (int64_t i = 0; OB_SUCC(ret) && i < cnt; i++) {
      tmp_info.reset();
      tmp_info.backup_set_desc_ = backup_set_desc_list_.at(i);
      if (OB_FAIL(tmp_info.backup_set_path_.assign(backup_set_path_list_.at(i)))) {
        LOG_WARN("fail to assign backup set path", K(ret));
      } else if (OB_FAIL(backup_set_list.push_back(tmp_info))) {
        LOG_WARN("fail to push backup tmp info", K(ret), K(tmp_info));
      }
    }
  }
  return ret;
}

int ObPhysicalRestoreBackupDestList::get_backup_set_list_format_str(
    common::ObIAllocator &allocator, common::ObString &str) const
{
  int ret = OB_SUCCESS;
  char *str_buf = NULL;
  int64_t str_buf_len = get_backup_set_list_format_str_len() + 1;
  if (str_buf_len > OB_MAX_LONGTEXT_LENGTH + 1) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("format str is too long", KR(ret), K(str_buf_len));
  } else if (OB_ISNULL(str_buf = static_cast<char *>(allocator.alloc(str_buf_len)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc buf", KR(ret), K(str_buf_len));
  } else if (backup_set_path_list_.count() <= 0) {
    MEMSET(str_buf, '\0', str_buf_len);
  } else {
    int64_t pos = 0;
    for (int64_t i = 0; OB_SUCC(ret) && i < backup_set_path_list_.count(); i++) {
      const share::ObBackupSetPath &item = backup_set_path_list_.at(i);
      if (OB_FAIL(databuff_printf(str_buf, str_buf_len, pos, "%s%s",
                  0 == i ? "" : ",",
                  item.ptr()))) {
        LOG_WARN("fail to append str", KR(ret), K(item), K(pos), K(str_buf));
      }
    }

  }
  if (OB_FAIL(ret)) {
  } else if (OB_ISNULL(str_buf) || str_buf_len <= 0) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("unexpected format str", KR(ret), K(str_buf), K(str_buf_len));
  } else {
    str.assign_ptr(str_buf, static_cast<int32_t>(str_buf_len - 1));
    LOG_DEBUG("get format backup set path list str", KR(ret), K(str));
  }
  return ret;
}

int ObPhysicalRestoreBackupDestList::get_backup_set_desc_list_format_str(
    common::ObIAllocator &allocator, common::ObString &str) const
{
  int ret = OB_SUCCESS;
  char *str_buf = NULL;
  int64_t str_buf_len = get_backup_set_list_format_str_len() + 1;
  if (str_buf_len > OB_MAX_LONGTEXT_LENGTH + 1) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("format str is too long", KR(ret), K(str_buf_len));
  } else if (OB_ISNULL(str_buf = static_cast<char *>(allocator.alloc(str_buf_len)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc buf", KR(ret), K(str_buf_len));
  } else if (backup_set_desc_list_.count() <= 0) {
    MEMSET(str_buf, '\0', str_buf_len);
  } else {
    int64_t pos = 0;
    for (int64_t i = 0; OB_SUCC(ret) && i < backup_set_desc_list_.count(); i++) {
      const share::ObBackupSetDesc &item = backup_set_desc_list_.at(i);
      if (OB_FAIL(databuff_printf(str_buf, str_buf_len, pos, "%ld:%s%s",
          item.backup_set_id_, item.backup_type_.get_backup_type_str(),
          backup_set_desc_list_.count() - 1 == i ? "\0" : ","))) {
        LOG_WARN("fail to append str", KR(ret), K(item), K(pos), K(str_buf));
      }
    }
  }
  if (OB_FAIL(ret)) {
  } else if (OB_ISNULL(str_buf) || str_buf_len <= 0) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("unexpected format str", KR(ret), K(str_buf), K(str_buf_len));
  } else {
    str.assign_ptr(str_buf, static_cast<int32_t>(STRLEN(str_buf)));
    LOG_DEBUG("get format backup set path list str", KR(ret), K(str));
  }
  return ret;
}

int ObPhysicalRestoreBackupDestList::get_backup_piece_list_format_str(
    common::ObIAllocator &allocator, common::ObString &str) const
{
  int ret = OB_SUCCESS;
  char *str_buf = NULL;
  int64_t str_buf_len = get_backup_piece_list_format_str_len() + 1;
  if (str_buf_len > OB_MAX_LONGTEXT_LENGTH + 1) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("format str is too long", KR(ret), K(str_buf_len));
  } else if (OB_ISNULL(str_buf = static_cast<char *>(allocator.alloc(str_buf_len)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc buf", KR(ret), K(str_buf_len));
  } else if (backup_piece_path_list_.count() <= 0) {
    MEMSET(str_buf, '\0', str_buf_len);
  } else {
    int64_t pos = 0;
    for (int64_t i = 0; OB_SUCC(ret) && i < backup_piece_path_list_.count(); i++) {
      const share::ObBackupPiecePath &item = backup_piece_path_list_.at(i);
      if (OB_FAIL(databuff_printf(str_buf, str_buf_len, pos, "%s%s",
                  0 == i ? "" : ",", item.ptr()))) {
        LOG_WARN("fail to append str", KR(ret), K(item), K(pos), K(str_buf));
      }
    }
  }
  if (OB_FAIL(ret)) {
  } else if (OB_ISNULL(str_buf) || str_buf_len <= 0) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("unexpected format str", KR(ret), K(str_buf), K(str_buf_len));
  } else {
    str.assign_ptr(str_buf, static_cast<int32_t>(str_buf_len - 1));
    LOG_DEBUG("get format backup piece path list str", KR(ret), K(str));
  }
  return ret;
}

int ObPhysicalRestoreBackupDestList::get_log_path_list_format_str(
    common::ObIAllocator &allocator, common::ObString &str) const
{
  int ret = OB_SUCCESS;
  char *str_buf = NULL;
  int64_t str_buf_len = get_log_path_list_format_str_len() + 1;
  if (str_buf_len > OB_MAX_LONGTEXT_LENGTH + 1) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("format str is too long", KR(ret), K(str_buf_len));
  } else if (OB_ISNULL(str_buf = static_cast<char *>(allocator.alloc(str_buf_len)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc buf", KR(ret), K(str_buf_len));
  } else if (log_path_list_.count() <= 0) {
    MEMSET(str_buf, '\0', str_buf_len);
  } else {
    int64_t pos = 0;
    for (int64_t i = 0; OB_SUCC(ret) && i < log_path_list_.count(); i++) {
      const share::ObBackupPathString &item = log_path_list_.at(i);
      if (OB_FAIL(databuff_printf(str_buf, str_buf_len, pos, "%s%s",
                  0 == i ? "" : ",", item.ptr()))) {
        LOG_WARN("fail to append str", KR(ret), K(item), K(pos), K(str_buf));
      }
    }
  }
  if (OB_FAIL(ret)) {
  } else if (OB_ISNULL(str_buf) || str_buf_len <= 0) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("unexpected format str", KR(ret), K(str_buf), K(str_buf_len));
  } else {
    str.assign_ptr(str_buf, static_cast<int32_t>(str_buf_len - 1));
    LOG_DEBUG("get log path list str", KR(ret), K(str));
  }
  return ret;
}

int ObPhysicalRestoreBackupDestList::backup_set_list_assign_with_format_str(const common::ObString &str)
{
  int ret = OB_SUCCESS;
  char *buf = nullptr;
  share::ObBackupSetPath path;
  ObArenaAllocator allocator;
  int64_t buf_len = str.length() + 1;
  if (OB_ISNULL(buf = static_cast<char *>(allocator.alloc(buf_len)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_ERROR("fail to alloc string", K(ret), "buf length", buf_len);
  } else if (OB_FALSE_IT(MEMSET(buf, 0, buf_len))) {
  } else if (OB_FAIL(databuff_printf(buf, buf_len, "%.*s", static_cast<int>(str.length()), str.ptr()))) {
    LOG_WARN("fail to print str", K(ret), K(str), K(buf_len));
  } else {
    char *token = nullptr;
    char *saveptr = nullptr;
    token = buf;
    for (char *str = token; OB_SUCC(ret); str = nullptr) {
      token = ::STRTOK_R(str, ",", &saveptr);
      if (OB_ISNULL(token)) {
        break;
      } else if (OB_FALSE_IT(path.reset())) {
      } else if (OB_FAIL(path.assign(token))) {
        LOG_WARN("fail to assign backup set path", K(ret));
      } else if (OB_FAIL(backup_set_path_list_.push_back(path))) {
        LOG_WARN("fail to push back path", K(ret), K(path));
      }
    }
  }

  return ret;
}

int ObPhysicalRestoreBackupDestList::backup_set_desc_list_assign_with_format_str(const common::ObString &str)
{
  int ret = OB_SUCCESS;
  char *buf = nullptr;
  share::ObBackupSetDesc desc;
  ObArenaAllocator allocator;
  int64_t buf_len = str.length() + 1;
  if (OB_ISNULL(buf = static_cast<char *>(allocator.alloc(buf_len)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_ERROR("fail to alloc string", K(ret), "buf length", buf_len);
  } else if (OB_FALSE_IT(MEMSET(buf, 0, buf_len))) {
  } else if (OB_FAIL(databuff_printf(buf, buf_len, "%.*s", static_cast<int>(str.length()), str.ptr()))) {
    LOG_WARN("fail to print str", K(ret), K(str), K(buf_len));
  } else {
    char *token = nullptr;
    char *saveptr = nullptr;
    token = buf;
    for (char *str = token; OB_SUCC(ret); str = nullptr) {
      token = ::STRTOK_R(str, ",", &saveptr);
      char type_buf[OB_INNER_TABLE_BACKUP_TYPE_LENTH];
      if (OB_ISNULL(token)) {
        break;
      } else if (2 != sscanf(token, "%ld:%5s", &desc.backup_set_id_, type_buf)) {
        ret = OB_INVALID_ARGUMENT;
        LOG_WARN("invalid backup set desc list str", K(ret), K(str));
      } else if (OB_FAIL(desc.backup_type_.set_backup_type(type_buf))) {
        LOG_WARN("invalid backup set desc list str", K(ret), KP(type_buf), K(str));
      } else if (OB_FAIL(backup_set_desc_list_.push_back(desc))) {
        LOG_WARN("fail to push back set desc", K(ret), K(desc));
      }
    }
  }

  return ret;
}

int ObPhysicalRestoreBackupDestList::backup_piece_list_assign_with_format_str(const common::ObString &str)
{
  int ret = OB_SUCCESS;
  char *buf = nullptr;
  share::ObBackupPiecePath path;
  ObArenaAllocator allocator;
  int64_t buf_len = str.length() + 1;
  if (OB_ISNULL(buf = static_cast<char *>(allocator.alloc(buf_len)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_ERROR("fail to alloc string", K(ret), "buf length", buf_len);
  } else if (OB_FALSE_IT(MEMSET(buf, 0, buf_len))) {
  } else if (OB_FAIL(databuff_printf(buf, buf_len, "%.*s", static_cast<int>(str.length()), str.ptr()))) {
    LOG_WARN("fail to print str", K(ret), K(str), K(buf_len));
  } else {
    char *token = nullptr;
    char *saveptr = nullptr;
    token = buf;
    for (char *str = token; OB_SUCC(ret); str = nullptr) {
      token = ::STRTOK_R(str, ",", &saveptr);
      if (OB_ISNULL(token)) {
        break;
      } else if (OB_FALSE_IT(path.reset())) {
      } else if (OB_FAIL(path.assign(token))) {
        LOG_WARN("fail to assign backup set path", K(ret));
      } else if (OB_FAIL(backup_piece_path_list_.push_back(path))) {
        LOG_WARN("fail to push back path", K(ret), K(path));
      }
    }
  }

  return ret;
}

int ObPhysicalRestoreBackupDestList::log_path_list_assign_with_format_str(const common::ObString &str)
{
  int ret = OB_SUCCESS;
  char *buf = nullptr;
  share::ObBackupPathString path;
  ObArenaAllocator allocator;
  int64_t buf_len = str.length() + 1;
  if (OB_ISNULL(buf = static_cast<char *>(allocator.alloc(buf_len)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_ERROR("fail to alloc string", K(ret), "buf length", buf_len);
  } else if (OB_FALSE_IT(MEMSET(buf, 0, buf_len))) {
  } else if (OB_FAIL(databuff_printf(buf, buf_len, "%.*s", static_cast<int>(str.length()), str.ptr()))) {
    LOG_WARN("fail to print str", K(ret), K(str), K(buf_len));
  } else {
    char *token = nullptr;
    char *saveptr = nullptr;
    token = buf;
    for (char *str = token; OB_SUCC(ret); str = nullptr) {
      token = ::STRTOK_R(str, ",", &saveptr);
      if (OB_ISNULL(token)) {
        break;
      } else if (OB_FALSE_IT(path.reset())) {
      } else if (OB_FAIL(path.assign(token))) {
        LOG_WARN("fail to assign log path", K(ret));
      } else if (OB_FAIL(log_path_list_.push_back(path))) {
        LOG_WARN("fail to push log path", K(ret), K(path));
      }
    }
  }

  return ret;
}

int ObPhysicalRestoreBackupDestList::backup_set_list_assign_with_hex_str(const common::ObString &str)
{
  int ret = OB_SUCCESS;
  int64_t str_size = str.length();
  char *deserialize_buf = NULL;
  int64_t deserialize_size = str.length() / 2 + 1;
  int64_t deserialize_pos = 0;
  if (str_size <= 0) {
    // skip
  } else if (OB_ISNULL(deserialize_buf = static_cast<char*>(allocator_.alloc(deserialize_size)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc memory", KR(ret), K(deserialize_size));
  } else if (OB_FAIL(hex_to_cstr(str.ptr(), str_size, deserialize_buf, deserialize_size))) {
    LOG_WARN("fail to get cstr from hex", KR(ret), K(str_size), K(deserialize_size), K(str));
  } else if (OB_FAIL(backup_set_path_list_.deserialize(deserialize_buf, deserialize_size, deserialize_pos))) {
    LOG_WARN("fail to deserialize backup set path list", KR(ret), K(str),
             "deserialize_buf", ObString(deserialize_size, deserialize_buf));
  } else if (deserialize_pos > deserialize_size) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("deserialize error", KR(ret), K(deserialize_pos), K(deserialize_size));
  } else {
    LOG_DEBUG("assign with hex str", KR(ret), K(str));
  }
  return ret;
}

int ObPhysicalRestoreBackupDestList::backup_piece_list_assign_with_hex_str(
    const common::ObString &str)
{
  int ret = OB_SUCCESS;
  int64_t str_size = str.length();
  char *deserialize_buf = NULL;
  int64_t deserialize_size = str.length() / 2 + 1;
  int64_t deserialize_pos = 0;
  if (str_size <= 0) {
    // skip
  } else if (OB_ISNULL(deserialize_buf = static_cast<char*>(allocator_.alloc(deserialize_size)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc memory", KR(ret), K(deserialize_size));
  } else if (OB_FAIL(hex_to_cstr(str.ptr(), str_size, deserialize_buf, deserialize_size))) {
    LOG_WARN("fail to get cstr from hex", KR(ret), K(str_size), K(deserialize_size), K(str));
  } else if (OB_FAIL(backup_piece_path_list_.deserialize(deserialize_buf, deserialize_size, deserialize_pos))) {
    LOG_WARN("fail to deserialize backup piece path list", KR(ret), K(str),
             "deserialize_buf", ObString(deserialize_size, deserialize_buf));
  } else if (deserialize_pos > deserialize_size) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("deserialize error", KR(ret), K(deserialize_pos), K(deserialize_size));
  } else {
    LOG_DEBUG("assign with hex str", KR(ret), K(str));
  }
  return ret;
}

int ObPhysicalRestoreBackupDestList::log_path_list_assign_with_hex_str(
    const common::ObString &str)
{
  int ret = OB_SUCCESS;
  int64_t str_size = str.length();
  char *deserialize_buf = NULL;
  int64_t deserialize_size = str.length() / 2 + 1;
  int64_t deserialize_pos = 0;
  if (str_size <= 0) {
    // skip
  } else if (OB_ISNULL(deserialize_buf = static_cast<char*>(allocator_.alloc(deserialize_size)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc memory", KR(ret), K(deserialize_size));
  } else if (OB_FAIL(hex_to_cstr(str.ptr(), str_size, deserialize_buf, deserialize_size))) {
    LOG_WARN("fail to get cstr from hex", KR(ret), K(str_size), K(deserialize_size), K(str));
  } else if (OB_FAIL(log_path_list_.deserialize(deserialize_buf, deserialize_size, deserialize_pos))) {
    LOG_WARN("fail to deserialize log path list", KR(ret), K(str),
             "deserialize_buf", ObString(deserialize_size, deserialize_buf));
  } else if (deserialize_pos > deserialize_size) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("deserialize error", KR(ret), K(deserialize_pos), K(deserialize_size));
  } else {
    LOG_DEBUG("assign with hex str", KR(ret), K(str));
  }
  return ret;
}

int ObPhysicalRestoreBackupDestList::get_backup_set_list_hex_str(
    common::ObIAllocator &allocator,
    common::ObString &str) const
{
  int ret = OB_SUCCESS;
  char *serialize_buf = NULL;
  int64_t serialize_size = backup_set_path_list_.get_serialize_size();
  int64_t serialize_pos = 0;
  char *hex_buf = NULL;
  int64_t hex_size = 2 * serialize_size;
  int64_t hex_pos = 0;
  if (serialize_size > OB_MAX_LONGTEXT_LENGTH / 2) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("serialize_size is too long", KR(ret), K(serialize_size));
  } else if (OB_ISNULL(serialize_buf = static_cast<char*>(allocator.alloc(serialize_size)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc memory", KR(ret));
  } else if (OB_FAIL(backup_set_path_list_.serialize(serialize_buf, serialize_size, serialize_pos))) {
    LOG_WARN("fail to serialize table_items", KR(ret), K_(backup_set_path_list));
  } else if (serialize_pos > serialize_size) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("serialize error", KR(ret), K(serialize_pos), K(serialize_size));
  } else if (OB_ISNULL(hex_buf = static_cast<char*>(allocator.alloc(hex_size)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc memory", KR(ret), K(hex_size));
  } else if (OB_FAIL(hex_print(serialize_buf, serialize_pos, hex_buf, hex_size, hex_pos))) {
    LOG_WARN("fail to print hex", KR(ret), K(serialize_pos), K(hex_size));
  } else if (hex_pos > hex_size) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("encode error", KR(ret), K(hex_pos), K(hex_size));
  } else {
    str.assign_ptr(hex_buf, static_cast<int32_t>(hex_size));
    LOG_DEBUG("get hex backup set list str", KR(ret), K(str));
  }
  return ret;
}

int ObPhysicalRestoreBackupDestList::get_backup_piece_list_hex_str(
    common::ObIAllocator &allocator,
    common::ObString &str) const
{
  int ret = OB_SUCCESS;
  char *serialize_buf = NULL;
  int64_t serialize_size = backup_piece_path_list_.get_serialize_size();
  int64_t serialize_pos = 0;
  char *hex_buf = NULL;
  int64_t hex_size = 2 * serialize_size;
  int64_t hex_pos = 0;
  if (serialize_size > OB_MAX_LONGTEXT_LENGTH / 2) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("serialize_size is too long", KR(ret), K(serialize_size));
  } else if (OB_ISNULL(serialize_buf = static_cast<char*>(allocator.alloc(serialize_size)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc memory", KR(ret));
  } else if (OB_FAIL(backup_piece_path_list_.serialize(serialize_buf, serialize_size, serialize_pos))) {
    LOG_WARN("fail to serialize table_items", KR(ret), K_(backup_piece_path_list));
  } else if (serialize_pos > serialize_size) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("serialize error", KR(ret), K(serialize_pos), K(serialize_size));
  } else if (OB_ISNULL(hex_buf = static_cast<char*>(allocator.alloc(hex_size)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc memory", KR(ret), K(hex_size));
  } else if (OB_FAIL(hex_print(serialize_buf, serialize_pos, hex_buf, hex_size, hex_pos))) {
    LOG_WARN("fail to print hex", KR(ret), K(serialize_pos), K(hex_size));
  } else if (hex_pos > hex_size) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("encode error", KR(ret), K(hex_pos), K(hex_size));
  } else {
    str.assign_ptr(hex_buf, static_cast<int32_t>(hex_size));
    LOG_DEBUG("get hex backup piece list str", KR(ret), K(str));
  }
  return ret;
}

int ObPhysicalRestoreBackupDestList::get_log_path_list_hex_str(
    common::ObIAllocator &allocator,
    common::ObString &str) const
{
  int ret = OB_SUCCESS;
  char *serialize_buf = NULL;
  int64_t serialize_size = log_path_list_.get_serialize_size();
  int64_t serialize_pos = 0;
  char *hex_buf = NULL;
  int64_t hex_size = 2 * serialize_size;
  int64_t hex_pos = 0;
  if (serialize_size > OB_MAX_LONGTEXT_LENGTH / 2) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("serialize_size is too long", KR(ret), K(serialize_size));
  } else if (OB_ISNULL(serialize_buf = static_cast<char*>(allocator.alloc(serialize_size)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc memory", KR(ret));
  } else if (OB_FAIL(log_path_list_.serialize(serialize_buf, serialize_size, serialize_pos))) {
    LOG_WARN("fail to serialize table_items", KR(ret), K_(log_path_list));
  } else if (serialize_pos > serialize_size) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("serialize error", KR(ret), K(serialize_pos), K(serialize_size));
  } else if (OB_ISNULL(hex_buf = static_cast<char*>(allocator.alloc(hex_size)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("fail to alloc memory", KR(ret), K(hex_size));
  } else if (OB_FAIL(hex_print(serialize_buf, serialize_pos, hex_buf, hex_size, hex_pos))) {
    LOG_WARN("fail to print hex", KR(ret), K(serialize_pos), K(hex_size));
  } else if (hex_pos > hex_size) {
    ret = OB_SIZE_OVERFLOW;
    LOG_WARN("encode error", KR(ret), K(hex_pos), K(hex_size));
  } else {
    str.assign_ptr(hex_buf, static_cast<int32_t>(hex_size));
    LOG_DEBUG("get hex log path list str", KR(ret), K(str));
  }
  return ret;
}

int64_t ObPhysicalRestoreBackupDestList::get_backup_set_list_format_str_len() const
{
  int64_t length = 0;
  for (int64_t i = 0; i < backup_set_path_list_.count(); ++i) {
    const ObBackupSetPath &path = backup_set_path_list_.at(i);
    length += path.size() + (0 == i ? 0 : 1);
  }
  return length;
}

int64_t ObPhysicalRestoreBackupDestList::get_backup_piece_list_format_str_len() const
{
  int64_t length = 0;
  for (int64_t i = 0; i < backup_piece_path_list_.count(); ++i) {
    const ObBackupPiecePath &path = backup_piece_path_list_.at(i);
    length += path.size() + (0 == i ? 0 : 1);
  }
  return length;
}

int64_t ObPhysicalRestoreBackupDestList::get_log_path_list_format_str_len() const
{
  int64_t length = 0;
  for (int64_t i = 0; i < log_path_list_.count(); ++i) {
    const ObBackupPathString &path = log_path_list_.at(i);
    length += path.size() + (0 == i ? 0 : 1);
  }
  return length;
}

/******************ObBackupCommonHeader*******************/
ObBackupCommonHeader::ObBackupCommonHeader()
{
  reset();
}

void ObBackupCommonHeader::reset()
{
  memset(this, 0, sizeof(ObBackupCommonHeader));
  header_version_ = COMMON_HEADER_VERSION;
  compressor_type_ = static_cast<uint8_t>(common::ObCompressorType::INVALID_COMPRESSOR);
  data_type_ = static_cast<uint16_t>(ObBackupFileType::BACKUP_TYPE_MAX);
  header_length_ = static_cast<uint16_t>(sizeof(ObBackupCommonHeader));
}


void ObBackupCommonHeader::set_header_checksum()
{
  header_checksum_ = calc_header_checksum();
}

int ObBackupCommonHeader::set_checksum(const char *buf, const int64_t len)
{
  int ret = OB_SUCCESS;

  if (OB_ISNULL(buf) || len < 0 || data_zlength_ != len) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid args", K(ret), KP(buf), K(len), K_(data_zlength));
  } else {
    data_checksum_ = ob_crc64(buf, len);
    header_checksum_ = calc_header_checksum();
  }
  return ret;
}

int ObBackupCommonHeader::check_header_checksum() const
{
  int ret = OB_SUCCESS;
  int16_t checksum = calc_header_checksum();
  if (OB_UNLIKELY(header_checksum_ != checksum)) {
    ret = OB_CHECKSUM_ERROR;
    COMMON_LOG(WARN, "check header checksum failed.", K(*this), K(checksum), K(ret));
  }
  return ret;
}


int16_t ObBackupCommonHeader::calc_header_checksum() const
{
  int16_t checksum = 0;

  checksum = static_cast<int16_t>(checksum ^ static_cast<int16_t>(header_version_));
  checksum = static_cast<int16_t>(checksum ^ static_cast<int16_t>(compressor_type_));
  checksum = checksum ^ data_type_;
  checksum = checksum ^ header_length_;
  format_i64(data_length_, checksum);
  format_i64(data_zlength_, checksum);
  format_i64(data_checksum_, checksum);
  format_i64(align_length_, checksum);
  return checksum;
}

int ObBackupCommonHeader::check_valid() const
{
  int ret = OB_SUCCESS;

  if (COMMON_HEADER_VERSION != header_version_
      || ObCompressorType::INVALID_COMPRESSOR == compressor_type_
      || ((ObCompressorType::NONE_COMPRESSOR == compressor_type_)
          && (data_length_ != data_zlength_))
      || data_type_ >= ObBackupFileType::BACKUP_TYPE_MAX
      || header_length_ != sizeof(ObBackupCommonHeader)
      || align_length_ < 0) {
    ret = OB_INVALID_ERROR;
    COMMON_LOG(WARN, "invalid header", K(ret), K(*this),
        "ObBackupCommonHeader size", sizeof(ObBackupCommonHeader));
  } else if (OB_FAIL(check_header_checksum())) {
    COMMON_LOG(WARN, "header checksum error", K(ret));
  }

  return ret;
}

bool ObBackupCommonHeader::is_compresssed_data() const
{
  return (data_length_ != data_zlength_)
      || (compressor_type_ > ObCompressorType::NONE_COMPRESSOR);
}


int ObBackupCommonHeader::check_data_checksum(const char *buf, const int64_t len) const
{
  int ret = OB_SUCCESS;
  if (NULL == buf || len < 0) {
    ret = OB_INVALID_ARGUMENT;
    COMMON_LOG(WARN, "invalid argument.", K(ret), KP(buf), K(len));
  } else if ((0 == len) && (0 != data_length_
      || 0 != data_zlength_ || 0 != data_checksum_)) {
    ret = OB_INVALID_ARGUMENT;
    COMMON_LOG(WARN, "invalid argument.", K(ret), K(len),
        K_(data_length), K_(data_zlength), K_(data_checksum));
  } else if (data_zlength_ != len) {
    ret = OB_INVALID_ARGUMENT;
    COMMON_LOG(WARN, "invalid argument.", K(ret), K(len), K_(data_zlength));
  } else {
    int64_t crc_check_sum = ob_crc64(buf, len);
    if (crc_check_sum != data_checksum_) {
      ret = OB_CHECKSUM_ERROR;
      COMMON_LOG(WARN, "checksum error", K(crc_check_sum),
          K_(data_checksum), K(ret));
    }
  }
  return ret;
}

static const char *status_strs[] = {
    "INVALID",
    "STOP",
    "BEGINNING",
    "DOING",
    "STOPPING",
    "INTERRUPTED",
    "MIXED",
    "PAUSED",
};

const char *ObLogArchiveStatus::get_str(const STATUS &status)
{
  const char *str = nullptr;

  if (status < 0 || status >= MAX) {
    str = "UNKNOWN";
  } else {
    str = status_strs[status];
  }
  return str;
}

ObLogArchiveStatus::STATUS ObLogArchiveStatus::get_status(const char *status_str)
{
  ObLogArchiveStatus::STATUS status = ObLogArchiveStatus::MAX;

  const int64_t count = ARRAYSIZEOF(status_strs);
  STATIC_ASSERT(static_cast<int64_t>(ObLogArchiveStatus::MAX) == count, "status count mismatch");
  for (int64_t i = 0; i < count; ++i) {
    if (0 == strcmp(status_str, status_strs[i])) {
      status = static_cast<ObLogArchiveStatus::STATUS>(i);
      break;
    }
  }
  return status;
}

ObTenantLogArchiveStatus::ObTenantLogArchiveStatus()
  :tenant_id_(OB_INVALID_ID),
   copy_id_(0),
   start_ts_(0),
   checkpoint_ts_(0),
   incarnation_(0),
   round_(0),
   status_(ObLogArchiveStatus::MAX),
   is_mark_deleted_(false),
   is_mount_file_created_(false),
   compatible_(COMPATIBLE::NONE),
   backup_piece_id_(0),
   start_piece_id_(0)
{
}

void ObTenantLogArchiveStatus::reset()
{
  tenant_id_ = OB_INVALID_ID;
  copy_id_ = 0;
  start_ts_ = 0;
  checkpoint_ts_ = 0;
  incarnation_ = 0;
  round_ = 0;
  status_ = ObLogArchiveStatus::MAX;
  is_mark_deleted_ = false;
  is_mount_file_created_ = false;
  compatible_ = COMPATIBLE::NONE;
  backup_piece_id_ = 0;
  start_piece_id_ = 0;
}

bool ObTenantLogArchiveStatus::is_valid() const
{
  return tenant_id_ != OB_INVALID_ID
      && copy_id_ >= 0
      && ObLogArchiveStatus::is_valid(status_)
      && incarnation_ >= 0
      && round_ >= 0
      && start_ts_ >= -1
      && checkpoint_ts_ >= -1
      && compatible_ >= COMPATIBLE::NONE
      && backup_piece_id_ >= 0
      && start_piece_id_ >= 0;
}

bool ObTenantLogArchiveStatus::is_compatible_valid(COMPATIBLE compatible)
{
  return compatible >= COMPATIBLE::NONE && compatible < COMPATIBLE::MAX;
}

// 备份状态机
// STOP->STOP
// BEGINNING -> BEGINNING\DOING\STOPPING\INERRUPTED
// DOING -> DOING\STOPPING\INERRUPTED
// STOPPING -> STOPPIONG\STOP
// INTERRUPTED -> INERRUPTED\STOPPING
// 备份备份状态机
// DOING->PAUSED\STOP
// PAUSED->DOING
int ObTenantLogArchiveStatus::update(const ObTenantLogArchiveStatus &new_status)
{
  int ret = OB_SUCCESS;

  if (!is_valid()) {
    ret = OB_NOT_INIT;
    LOG_WARN("not valid status", K(ret), K(*this));
  } else if (!new_status.is_valid() || new_status.incarnation_ != incarnation_
      || new_status.round_ != round_ || new_status.tenant_id_ != tenant_id_
      || new_status.compatible_ != compatible_) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid args", K(ret), "is_new_status_valid", new_status.is_valid(),
        K(new_status), K(*this));
  } else if (is_mount_file_created_ && !new_status.is_mount_file_created_) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("is_mount_file_created_ cannot change", K(ret), K(*this), K(new_status));
  } else {
    is_mount_file_created_ = new_status.is_mount_file_created_;
    switch (status_) {
    case ObLogArchiveStatus::STOP: {
      if (OB_FAIL(update_stop_(new_status))) {
        LOG_WARN("failed to update stop", K(ret));
      }
      break;
    }
    case ObLogArchiveStatus::BEGINNING: {
      if (OB_FAIL(update_beginning_(new_status))) {
        LOG_WARN("failed to update beginning", K(ret));
      }
      break;
    }
    case ObLogArchiveStatus::DOING: {
      if (OB_FAIL(update_doing_(new_status))) {
        LOG_WARN("failed to update doing", K(ret));
      }
      break;
    }
    case ObLogArchiveStatus::STOPPING: {
      if (OB_FAIL(update_stopping_(new_status))) {
        LOG_WARN("failed to update stopping", K(ret));
      }
      break;
    }
    case ObLogArchiveStatus::INTERRUPTED: {
      if (OB_FAIL(update_interrupted_(new_status))) {
        LOG_WARN("failed to update interrupted", K(ret));
      }
      break;
    }
    case ObLogArchiveStatus::PAUSED: {
      if (OB_FAIL(update_paused_(new_status))) {
        LOG_WARN("failed to update paused", K(ret));
      }
      break;
    }
    default : {
      ret = OB_ERR_SYS;
      LOG_ERROR("invalid status", K(ret), K(*this), K(new_status));
    }
    } // end switch
  }

  return ret;
}

int ObTenantLogArchiveStatus::get_piece_key(ObBackupPieceInfoKey &key) const
{
  int ret = OB_SUCCESS;
  key.reset();

  key.incarnation_ = incarnation_;
  key.tenant_id_ = tenant_id_;
  key.round_id_ = round_;
  if (start_piece_id_ > 0) {
    key.backup_piece_id_ = backup_piece_id_;
  } else {
    key.backup_piece_id_ = 0;// 0 means not switch piece
  }
  key.copy_id_ = copy_id_;
  return ret;
}

bool ObTenantLogArchiveStatus::need_switch_piece() const
{
  return start_piece_id_ > 0;
}

int ObTenantLogArchiveStatus::update_stop_(const ObTenantLogArchiveStatus &new_status)
{
  int ret = OB_SUCCESS;

  if (ObLogArchiveStatus::STOP != new_status.status_) {
    ret = OB_LOG_ARCHIVE_STAT_NOT_MATCH;
    LOG_ERROR("stop status can only update with stop", K(ret), K(*this), K(new_status));
  } else {
    LOG_INFO("no need update stop status", K(*this), K(new_status));
  }
  return ret;
}

int ObTenantLogArchiveStatus::update_beginning_(const ObTenantLogArchiveStatus &new_status)
{
  int ret = OB_SUCCESS;

  if (ObLogArchiveStatus::DOING == new_status.status_) {
    if (0 >= new_status.start_ts_ || 0 >= new_status.checkpoint_ts_) {
      ret = OB_INVALID_LOG_ARCHIVE_STATUS;
      LOG_WARN("[LOG_ARCHIVE] checkpoint_ts or start_ts must not less than 0",
          K(ret), K(*this), K(new_status));
    } else {
      LOG_INFO("[LOG_ARCHIVE] update beginning to doing", K(*this), K(new_status));
      status_ = new_status.status_;
      start_ts_ = new_status.start_ts_;
      checkpoint_ts_ = new_status.checkpoint_ts_;
    }
  } else if (ObLogArchiveStatus::BEGINNING == new_status.status_) {
    // do nothing
  } else if (ObLogArchiveStatus::STOPPING == new_status.status_
      || ObLogArchiveStatus::STOP == new_status.status_
      || ObLogArchiveStatus::INTERRUPTED == new_status.status_) {
    LOG_INFO("[LOG_ARCHIVE] update beginning to new stat", K(*this), K(new_status));
    status_ = new_status.status_;
  } else {
    ret = OB_LOG_ARCHIVE_STAT_NOT_MATCH;
    LOG_ERROR("stat not match", K(ret), K(*this), K(new_status));
  }
  return ret;
}

int ObTenantLogArchiveStatus::update_doing_(const ObTenantLogArchiveStatus &new_status)
{
  int ret = OB_SUCCESS;

  if (ObLogArchiveStatus::DOING == new_status.status_) {
    if (start_ts_ < new_status.start_ts_) {
      ret = OB_INVALID_LOG_ARCHIVE_STATUS;
      LOG_ERROR("new start_ts must not larger than the prev one",
          K(ret), K(*this), K(new_status));
    } else if (checkpoint_ts_ > new_status.checkpoint_ts_) {
      ret = OB_INVALID_LOG_ARCHIVE_STATUS;
      LOG_WARN("new checkpoint_ts must not less than the prev one",
          K(ret), K(*this), K(new_status));
    } else {
      LOG_INFO("update doing stat", K(*this), K(new_status));
      checkpoint_ts_ = new_status.checkpoint_ts_;
    }
  } else if (ObLogArchiveStatus::STOPPING == new_status.status_
      || ObLogArchiveStatus::STOP == new_status.status_
      || ObLogArchiveStatus::INTERRUPTED == new_status.status_
      || ObLogArchiveStatus::PAUSED == new_status.status_) {
    FLOG_INFO("[LOG_ARCHIVE] update doing to new stat", K(*this), K(new_status));
    status_ = new_status.status_;
  } else {
    ret = OB_LOG_ARCHIVE_STAT_NOT_MATCH;
    LOG_ERROR("stat not match", K(ret), K(*this), K(new_status));
  }
  return ret;
}

int ObTenantLogArchiveStatus::update_stopping_(const ObTenantLogArchiveStatus &new_status)
{
  int ret = OB_SUCCESS;

  if (ObLogArchiveStatus::STOPPING == new_status.status_) {
    // do nothing
  } else if (ObLogArchiveStatus::STOP == new_status.status_) {
    FLOG_INFO("[LOG_ARCHIVE] update stopping to new stat", K(*this), K(new_status));
    status_ = new_status.status_;
  } else {
    ret = OB_LOG_ARCHIVE_STAT_NOT_MATCH;
    LOG_ERROR("stat not match", K(ret), K(*this), K(new_status));
  }
  return ret;
}

int ObTenantLogArchiveStatus::update_interrupted_(const ObTenantLogArchiveStatus &new_status)
{
  int ret = OB_SUCCESS;

  if (ObLogArchiveStatus::INTERRUPTED == new_status.status_) {
    // do nothing
  } else if (ObLogArchiveStatus::STOPPING == new_status.status_
      || ObLogArchiveStatus::STOP == new_status.status_) {
    FLOG_INFO("[LOG_ARCHIVE] update interrupted to new stat", K(*this), K(new_status));
    status_ = new_status.status_;
  } else {
    ret = OB_LOG_ARCHIVE_STAT_NOT_MATCH;
    LOG_ERROR("stat not match", K(ret), K(*this), K(new_status));
  }
  return ret;
}

int ObTenantLogArchiveStatus::update_paused_(const ObTenantLogArchiveStatus &new_status)
{
  int ret = OB_SUCCESS;

  if (ObLogArchiveStatus::INTERRUPTED == new_status.status_) {
    // do nothing
  } else if (ObLogArchiveStatus::DOING == new_status.status_
      || ObLogArchiveStatus::STOP == new_status.status_) {
    FLOG_INFO("[LOG_ARCHIVE] update paused to new stat", K(*this), K(new_status));
    status_ = new_status.status_;
  } else {
    ret = OB_LOG_ARCHIVE_STAT_NOT_MATCH;
    LOG_ERROR("stat not match", K(ret), K(*this), K(new_status));
  }
  return ret;
}

OB_SERIALIZE_MEMBER(ObTenantLogArchiveStatus, tenant_id_, start_ts_, checkpoint_ts_,
    incarnation_, round_, status_, is_mark_deleted_, is_mount_file_created_, compatible_,
    backup_piece_id_, start_piece_id_);

ObLogArchiveBackupInfo::ObLogArchiveBackupInfo()
  : status_()
{
  backup_dest_[0] = '\0';
}

void ObLogArchiveBackupInfo::reset()
{
  status_.reset();
  backup_dest_[0] = '\0';
}

bool ObLogArchiveBackupInfo::is_valid() const
{
  return status_.is_valid();
}

//TODO(yaoying.yyy): S3 is alse oss?
bool ObLogArchiveBackupInfo::is_oss() const
{
  ObString dest(backup_dest_);
  return dest.prefix_match(OB_OSS_PREFIX);
}

bool ObLogArchiveBackupInfo::is_same(const ObLogArchiveBackupInfo &other) const
{
  return 0 == strncmp(backup_dest_, other.backup_dest_, sizeof(backup_dest_))
      && status_.tenant_id_ == other.status_.tenant_id_
      && status_.start_ts_ == other.status_.start_ts_
      && status_.checkpoint_ts_ == other.status_.checkpoint_ts_
      && status_.incarnation_ == other.status_.incarnation_
      && status_.round_ == other.status_.round_
      && status_.status_ == other.status_.status_
      && status_.is_mark_deleted_ == other.status_.is_mark_deleted_;
}

int ObLogArchiveBackupInfo::get_piece_key(ObBackupPieceInfoKey &key) const
{
  return status_.get_piece_key(key);
}

int ObLogArchiveBackupInfo::get_backup_dest(ObBackupDest &backup_dest) const
{
  int ret = OB_SUCCESS;
  backup_dest.reset();
  if (0 == strlen(backup_dest_)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", KR(ret));
  } else if (OB_FAIL(backup_dest.set(backup_dest_))) {
    LOG_WARN("failed to set backup dest", KR(ret), K(backup_dest_));
  }
  return ret;
}

//***********************ObBackupStorageInfo***************************
ObBackupStorageInfo::ObBackupStorageInfo()
  : device_type_(ObStorageType::OB_STORAGE_MAX_TYPE)
{
  endpoint_[0] = '\0';
  access_id_[0] = '\0';
  access_key_[0] = '\0';
  extension_[0] = '\0';
}

ObBackupStorageInfo::~ObBackupStorageInfo()
{
  reset();
}

void ObBackupStorageInfo::reset()
{
  device_type_ = ObStorageType::OB_STORAGE_MAX_TYPE;
  endpoint_[0] = '\0';
  access_id_[0] = '\0';
  access_key_[0] = '\0';
  extension_[0] = '\0';
}

bool ObBackupStorageInfo::is_valid() const
{
  return device_type_ >= 0 && device_type_ < ObStorageType::OB_STORAGE_MAX_TYPE;
}

const char *ObBackupStorageInfo::get_type_str() const
{
  return get_storage_type_str(device_type_);
}

int ObBackupStorageInfo::set(
    const common::ObStorageType device_type,
    const char *endpoint,
    const char *authorization,
    const char *extension)
{
  int ret = OB_SUCCESS;
  if (is_valid()) {
    ret = OB_INIT_TWICE;
    LOG_WARN("storage info init twice", K(ret));
  } else if (OB_ISNULL(endpoint) || OB_ISNULL(authorization) || OB_ISNULL(extension) || OB_STORAGE_MAX_TYPE == device_type) {
    ret = OB_INVALID_BACKUP_DEST;
    LOG_WARN("invalid args", K(ret), KP(endpoint), KP(authorization), KP(extension), K(device_type));
  } else if (FALSE_IT(device_type_ = device_type)) {
  } else if (0 != strlen(endpoint) && OB_FAIL(databuff_printf(endpoint_, sizeof(endpoint_), "%s", endpoint))) {
    LOG_WARN("failed to set endpoint", K(ret));
  } else if (0 != strlen(authorization) && OB_FAIL(parse_authorization_(authorization))) {
    LOG_WARN("failed to parse authorization", K(ret));
  } else if (0 != strlen(extension) && OB_FAIL(databuff_printf(extension_, sizeof(extension_), "%s", extension))) {
    LOG_WARN("failed to set extension", K(ret));
  }

  if (OB_FAIL(ret)) {
  } else if (ObStorageType::OB_STORAGE_FILE != device_type_ && (0 == strlen(endpoint_) || 0 == strlen(access_id_) || 0 == strlen(access_key_))) {
    ret = OB_INVALID_BACKUP_DEST;
    LOG_WARN("backup device is not nfs, endpoint/access_id/access_key do not allow to be empty", K(ret), K_(device_type), K_(endpoint), K_(access_id));
  } else if (ObStorageType::OB_STORAGE_FILE == device_type_ && (0 != strlen(endpoint_) || 0 != strlen(access_id_) || 0 != strlen(access_key_))) {
    ret = OB_INVALID_BACKUP_DEST;
    LOG_WARN("backup device is nfs, endpoint/access_id/access_key not be empty", K(ret), K_(device_type), K_(endpoint), K_(access_id));
  }
  return ret;
}

int ObBackupStorageInfo::set_access_key_(const char *buf, const bool need_decrypt)
{
  int ret = OB_SUCCESS;
  if (OB_ISNULL(buf)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("buf is null", K(ret), K(need_decrypt));
  } else if (0 != strlen(access_key_)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("access_key has been set value", K(ret), K_(access_key));
  } else if (OB_FAIL(set_storage_info_field_(buf, access_key_, sizeof(access_key_)))) {
    LOG_WARN("failed to set endpoint",K(ret), K(buf));
  }
  return ret;
}

int ObBackupStorageInfo::check_delete_mode_(const char *delete_mode)
{
  int ret = OB_SUCCESS;
  if (NULL == delete_mode) {
    ret = OB_INVALID_ARGUMENT;
    OB_LOG(WARN, "invalid args", K(ret), KP(delete_mode));
  } else if (0 != strcmp(delete_mode, "delete")
      && 0 != strcmp(delete_mode, "tagging")) {
    ret = OB_INVALID_ARGUMENT;
    OB_LOG(WARN, "delete mode is invalid", K(ret), K(delete_mode));
  }
  return ret;
}

// oss:host=xxxx&access_id=xxx&access_key=xxx
// cos:host=xxxx&access_id=xxx&access_key=xxxappid=xxx
int ObBackupStorageInfo::set(
    const common::ObStorageType device_type,
    const char *storage_info)
{
  int ret = OB_SUCCESS;
  if (is_valid()) {
    ret = OB_INIT_TWICE;
    LOG_WARN("storage info init twice", K(ret));
  } else if (OB_ISNULL(storage_info) || strlen(storage_info) >= OB_MAX_BACKUP_STORAGE_INFO_LENGTH) {
    ret = OB_INVALID_BACKUP_DEST;
    LOG_WARN("storage info is too long", K(ret), K(storage_info), K(strlen(storage_info)));
  } else if (FALSE_IT(device_type_ = device_type)) {
  } else if (0 == strlen(storage_info)) {
    if (OB_STORAGE_FILE != device_type_) {
      ret = OB_INVALID_BACKUP_DEST;
      LOG_WARN("backup dest is valid", K(ret), K_(device_type));
    }
  } else {
    int64_t info_len = strlen(storage_info);
    char tmp[OB_MAX_BACKUP_STORAGE_INFO_LENGTH] = { 0 };
    char *token = NULL;
    char *saved_ptr = NULL;
    const char *HOST = "host=";
    const char *APPID = "appid=";
    const char *DELETE_MODE = "delete_mode=";
    bool has_appid = false;

    MEMCPY(tmp, storage_info, info_len);
    tmp[info_len] = '\0';
    token = tmp;
    for (char *str = token; OB_SUCC(ret); str = NULL) {
      token = ::strtok_r(str, "&", &saved_ptr);
      if (NULL == token) {
        break;
      } else if (0 == strncmp(HOST, token, strlen(HOST))) {
        if (OB_FAIL(set_storage_info_field_(token, endpoint_, sizeof(endpoint_)))) {
          LOG_WARN("failed to set endpoint",K(ret), K(token));
        }
      } else if (0 == strncmp(ACCESS_ID, token, strlen(ACCESS_ID))) {
        if (OB_FAIL(set_storage_info_field_(token, access_id_, sizeof(access_id_)))) {
          LOG_WARN("failed to set access id", K(ret), K(token));
        }
      } else if (0 == strncmp(ACCESS_KEY, token, strlen(ACCESS_KEY))) {
        if (OB_FAIL(set_access_key_(token, false/*need decrypt*/))) {
          LOG_WARN("failed to set oss extension", K(ret), K(token));
        }
      } else if (OB_STORAGE_COS == device_type && 0 == strncmp(APPID, token, strlen(APPID))) {
        has_appid = true;
        if (OB_FAIL(set_storage_info_field_(token, extension_, sizeof(extension_)))) {
          LOG_WARN("failed to set cos extension", K(ret), K(token));
        }
      } else if (OB_STORAGE_OSS == device_type && 0 == strncmp(DELETE_MODE, token, strlen(DELETE_MODE))) {
        if (OB_FAIL(check_delete_mode_(token + strlen(DELETE_MODE)))) {
          OB_LOG(WARN, "failed to set delete mode", K(ret), K(token));
        } else if (OB_FAIL(set_storage_info_field_(token, extension_, sizeof(extension_)))) {
          LOG_WARN("failed to set cos extension", K(ret), K(token));
        }
      } else {
        LOG_DEBUG("unkown storage info", K(*token));
      }
    }

    if (OB_FAIL(ret)) {
    } else if (OB_STORAGE_OSS == device_type &&(0 == strlen(endpoint_) || 0 == strlen(access_id_) || 0 == strlen(access_key_))) {
      ret = OB_INVALID_BACKUP_DEST;
      LOG_WARN("failed to parse storage info", K(ret), K_(endpoint), K_(access_id), K_(access_key));
    } else if (OB_STORAGE_COS == device_type && !has_appid) {
      ret = OB_INVALID_BACKUP_DEST;
      LOG_WARN("failed to parse  cos info", K(ret), K_(extension));
    } else {
      LOG_INFO("success to parse storage info", K(ret), K_(endpoint), K_(access_id), K_(extension));
    }
  }

  return ret;
}

int ObBackupStorageInfo::set(
    const char *uri,
    const char *storage_info)
{
  int ret = OB_SUCCESS;
  common::ObStorageType device_type;
  if (OB_FAIL(get_storage_type_from_path(uri, device_type))) {
    LOG_WARN("failed to get storage type from path", K(ret), KPC(this));
  } else if (OB_FAIL(set(device_type, storage_info))) {
    LOG_WARN("failed to set storage info", K(ret), KPC(this));
  }
  return ret;
}

int ObBackupStorageInfo::parse_authorization_(const char *authorization)
{
  int ret = OB_SUCCESS;
  char tmp_authorization[OB_MAX_BACKUP_AUTHORIZATION_LENGTH] = { 0 };
  char *token = NULL;
  char *saved_ptr = NULL;
  if (OB_ISNULL(authorization) || 0 == strlen(authorization)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("authorization is invalid", K(ret));
  } else if (OB_FAIL(databuff_printf(tmp_authorization, sizeof(tmp_authorization), "%s", authorization))) {
    LOG_WARN("failed to set token", K(ret));
  } else {
    token = tmp_authorization;
    for (char *str = token; OB_SUCC(ret); str = NULL) {
      token = ::strtok_r(str, "&", &saved_ptr);
      if (NULL == token) {
        break;
      } else if (0 == strncmp(ACCESS_ID, token, strlen(ACCESS_ID))) {
        if (OB_FAIL(set_storage_info_field_(token, access_id_, sizeof(access_id_)))) {
          LOG_WARN("failed to set access id", K(ret), K(token));
        }
      } else if (0 == strncmp(ACCESS_KEY, token, strlen(ACCESS_KEY))) {
        if (OB_FAIL(set_storage_info_field_(token, access_key_, sizeof(access_key_)))) {
          LOG_WARN("failed to set access key", K(ret), K(token));
        }
      } else {
        ret = OB_INVALID_ARGUMENT;
        LOG_WARN("invalid argument", K(ret), K(token));
      }
    }
  }
  return ret;
}

int ObBackupStorageInfo::get_authorization_info(char *authorization, int64_t length)
{
  int ret = OB_SUCCESS;
  char encrypt_key[OB_MAX_BACKUP_SERIALIZEKEY_LENGTH] = { 0 };
  STATIC_ASSERT(OB_MAX_BACKUP_AUTHORIZATION_LENGTH > (OB_MAX_BACKUP_ACCESSID_LENGTH + OB_MAX_BACKUP_SERIALIZEKEY_LENGTH), "array length overflow");
  if (!is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("storage info not init", K(ret));
  } else if (OB_ISNULL(authorization) || length <= 0) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("storage info is invalid", K(ret));
  } else if (OB_STORAGE_FILE == device_type_) {
    // do nothing
  } else if (OB_FAIL(databuff_printf(authorization, length, "%s&%s",  access_id_, access_key_))) {
    LOG_WARN("failed to set authorization", K(ret), K_(access_id), K(access_key_));
  }
  return ret;
}

int ObBackupStorageInfo::get_storage_info_str(char *storage_info, int64_t info_len, const bool need_encrypt) const
{
  int ret = OB_SUCCESS;
  char encrypt_key[OB_MAX_BACKUP_SERIALIZEKEY_LENGTH] = { 0 };
  const char *key = NULL;
  if (!is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("storage info not init", K(ret));
  } else if (OB_ISNULL(storage_info) || info_len <= 0) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("storage info is invalid", K(ret));
  } else if (OB_STORAGE_FILE == device_type_) {
    // do nothing
  } else {
    key = access_key_;
  }

  if (OB_FAIL(ret)) {
  } else if (OB_STORAGE_FILE != device_type_ && OB_FAIL(databuff_printf(storage_info, info_len, "%s&%s&%s",  endpoint_, access_id_, key))) {
    LOG_WARN("failed to set storage info", K(ret));
  } else if (0 != strlen(extension_) && info_len > strlen(storage_info)) {
    const int64_t str_len = strlen(storage_info);
    if (OB_FAIL(databuff_printf(storage_info + str_len, info_len - str_len, "&%s", extension_))) {
      LOG_WARN("failed to add extension", K(ret), K_(extension));
    }
  }
  return ret;
}

int ObBackupStorageInfo::assign(const ObBackupStorageInfo &storage_info)
{
  int ret = OB_SUCCESS;
  device_type_ = storage_info.device_type_;
  MEMCPY(endpoint_, storage_info.endpoint_, sizeof(endpoint_));
  MEMCPY(access_id_, storage_info.access_id_, sizeof(access_id_));
  MEMCPY(access_key_, storage_info.access_key_, sizeof(access_key_));
  MEMCPY(extension_, storage_info.extension_, sizeof(extension_));

  return ret;
}

bool ObBackupStorageInfo::operator ==(const ObBackupStorageInfo &storage_info) const
{
  return device_type_ == storage_info.device_type_
      && (0 == STRCMP(endpoint_, storage_info.endpoint_))
      && (0 == STRCMP(access_id_, storage_info.access_id_))
      && (0 == STRCMP(access_key_, storage_info.access_key_))
      && (0 == STRCMP(extension_, storage_info.extension_));
}

bool ObBackupStorageInfo::operator !=(const ObBackupStorageInfo &storage_info) const
{
  return !(*this == storage_info);
}

int64_t ObBackupStorageInfo::hash() const
{
  int64_t hash_value = 0;
  hash_value = murmurhash(&device_type_, static_cast<int32_t>(sizeof(device_type_)), hash_value);
  hash_value = murmurhash(endpoint_, static_cast<int32_t>(strlen(endpoint_)), hash_value);
  hash_value = murmurhash(access_id_, static_cast<int32_t>(strlen(access_id_)), hash_value);
  hash_value = murmurhash(access_key_, static_cast<int32_t>(strlen(access_key_)), hash_value);
  hash_value = murmurhash(extension_, static_cast<int32_t>(strlen(extension_)), hash_value);
  return hash_value;
}

int ObBackupStorageInfo::set_storage_info_field_(const char *info, char *field, const int64_t length)
{
  int ret = OB_SUCCESS;
  if (NULL == info || NULL == field) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid args", K(ret), KP(info), KP(field));
  } else {
    const int64_t info_len = strlen(info);
    int64_t pos = strlen(field);
    if (info_len >= length) {
      ret = OB_INVALID_ARGUMENT;
      LOG_WARN("info is too long ", K(ret), K(info), K(length));
    } else if (OB_FAIL(databuff_printf(field, length, pos, "%s", info))){
      LOG_WARN("failed to set storage info field", K(ret), K(info), K(field));
    }
  }
  return ret;
}


ObBackupDest::ObBackupDest()
  : root_path_(NULL),
    storage_info_(NULL),
    allocator_("ObBackupDest")
{
}

ObBackupDest::~ObBackupDest()
{
  reset();
}

bool ObBackupDest::is_valid() const
{
  return NULL != root_path_ && NULL != storage_info_;
}

void ObBackupDest::reset()
{
  allocator_.reset();
  root_path_ = NULL;
  storage_info_ = NULL;
}

bool ObBackupDest::operator ==(const ObBackupDest &backup_dest) const
{
  bool is_equal = true;
  is_equal = is_root_path_equal(backup_dest);
  if (!is_equal) {
    // do nothing
  } else if ((OB_ISNULL(storage_info_) && !OB_ISNULL(backup_dest.storage_info_))
      || (!OB_ISNULL(storage_info_) && OB_ISNULL(backup_dest.storage_info_))) {
    is_equal = false;
  } else if (!OB_ISNULL(storage_info_) && !OB_ISNULL(backup_dest.storage_info_)) {
    if (*storage_info_ != *(backup_dest.storage_info_)) {
      is_equal = false;
    }
  }
  return is_equal;
}

bool ObBackupDest::operator !=(const ObBackupDest &backup_dest) const
{
  return !(*this == backup_dest);
}

int ObBackupDest::deep_copy(const ObBackupDest &backup_dest)
{
  reset();
  int ret = OB_SUCCESS;
  ObArenaAllocator allocator;
  char *backup_dest_str = NULL;
  if (OB_ISNULL(backup_dest_str = reinterpret_cast<char *>(allocator.alloc(share::OB_MAX_BACKUP_DEST_LENGTH)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("allocate memory failed", KR(ret));
  } else if (OB_FAIL(backup_dest.get_backup_dest_str(backup_dest_str, share::OB_MAX_BACKUP_DEST_LENGTH))) {
    LOG_WARN("failed to get backup dest str", K(ret));
  } else if (OB_FAIL(set(backup_dest_str))) {
    LOG_WARN("failed to set backup dest", K(ret));
  }
  return ret;
}

int64_t ObBackupDest::hash() const
{
  int64_t hash_val = 0;
  if (is_valid()) {
    hash_val = murmurhash(root_path_, static_cast<int32_t>(strlen(root_path_)), hash_val);
    hash_val += storage_info_->hash();
  }
  return hash_val;
}

int ObBackupDest::alloc_and_init()
{
  int ret = OB_SUCCESS;
  void *raw_ptr = NULL;
  if (is_valid()) {
    // do nothing
  } else if (OB_ISNULL(root_path_ = static_cast<char *>(allocator_.alloc(OB_MAX_BACKUP_PATH_LENGTH)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("failed to alloc root_path memory", K(ret));
  } else if (OB_ISNULL(raw_ptr = allocator_.alloc(sizeof(ObBackupStorageInfo)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("failed to alloc storage_info memory", K(ret));
  } else {
    storage_info_ = new (raw_ptr) ObBackupStorageInfo();
    MEMSET(root_path_, 0, OB_MAX_BACKUP_PATH_LENGTH);
  }
  return ret;
}

int ObBackupDest::parse_backup_dest_str_(const char *backup_dest)
{
  int ret = OB_SUCCESS;
  ObString bakup_dest_str(backup_dest);
  common::ObStorageType type;
  int64_t pos = 0;
  if (!is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("backup_dest not init", K(ret), K(backup_dest));
  } else if (OB_FAIL(get_storage_type_from_path(bakup_dest_str, type))) {
    LOG_WARN("failed to get storage type", K(ret));
  } else {
    // oss://backup_dir/?host=xxx.com&access_id=111&access_key=222
    // file:///root_backup_dir"
    while (backup_dest[pos] != '\0') {
      if ('?' == backup_dest[pos]) {
        break;
      }
      ++pos;
    }

    if (pos >= OB_MAX_BACKUP_PATH_LENGTH) {
      ret = OB_INVALID_BACKUP_DEST;
      LOG_ERROR("backup dest is too long, cannot work", K(ret), K(pos), K(backup_dest));
    } else {
      MEMCPY(root_path_, backup_dest, pos);
      root_path_[pos] = '\0';
      if ('?' == backup_dest[pos]) {
        ++pos;
      }
      if (OB_FAIL(storage_info_->set(type, backup_dest + pos))) {
        LOG_WARN("failed to init storage_info", K(ret), K(type), K(pos), K(backup_dest));
      }
    }
  }
  return ret;
}

int ObBackupDest::set(const char *backup_dest)
{
  int ret = OB_SUCCESS;
  reset();
  if (is_valid()) {
    ret = OB_INIT_TWICE;
    LOG_WARN("cannot init twice", K(ret), K(*this));
  } else if (OB_ISNULL(backup_dest) || strlen(backup_dest) >= OB_MAX_BACKUP_DEST_LENGTH) {
    ret = OB_INVALID_BACKUP_DEST;
    LOG_WARN("invalid args", K(ret), KP(backup_dest));
  } else if (OB_FAIL(alloc_and_init())) {
    LOG_WARN("failed to alloc and init backup dest", K(ret));
  } else if (OB_FAIL(parse_backup_dest_str_(backup_dest))) {
    LOG_WARN("failed to parse backup dest str", K(ret), K(backup_dest));
  } else {
    root_path_trim_();
  }
  return ret;
}

int ObBackupDest::set(const common::ObString &backup_dest)
{
  int ret = OB_SUCCESS;
  ObArenaAllocator allocator;
  char *backup_dest_str = NULL;
  if (OB_ISNULL(backup_dest_str = reinterpret_cast<char *>(allocator.alloc(backup_dest.length() + 1)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("allocate memory failed", KR(ret));
  } else {
    MEMCPY(backup_dest_str, backup_dest.ptr(), backup_dest.length());
    backup_dest_str[backup_dest.length()] = '\0';
    if (OB_FAIL(set(backup_dest_str))) {
      LOG_WARN("failed to set backup dest", KR(ret), K(backup_dest));
    }
  }
  return ret;
}

int ObBackupDest::set(const ObBackupPathString &backup_dest_str)
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(set(backup_dest_str.ptr()))) {
    LOG_WARN("failed to set backup dest", KR(ret), K(backup_dest_str));
  }
  return ret;
}

int ObBackupDest::set(
    const char *path,
    const char *endpoint,
    const char *authorization,
    const char *extension)
{
  int ret = OB_SUCCESS;
  reset();
  common::ObStorageType type;
  ObString root_path_str(path);
  if (is_valid()) {
    ret = OB_INIT_TWICE;
    LOG_WARN("cannot init twice", K(ret), K(*this));
  } else if (OB_ISNULL(path) || OB_ISNULL(endpoint) || OB_ISNULL(authorization) || OB_ISNULL(extension)) {
    ret = OB_INVALID_BACKUP_DEST;
    LOG_WARN("invalid args", K(ret), KP(path), KP(endpoint));
  } else if (OB_FAIL(alloc_and_init())) {
    LOG_WARN("failed to alloc and init backup dest", K(ret));
  } else if (OB_FAIL(get_storage_type_from_path(root_path_str, type))) {
    LOG_WARN("failed to get storage type", K(ret));
  } else if (OB_FAIL(databuff_printf(root_path_, OB_MAX_BACKUP_PATH_LENGTH, "%s", path))) {
    LOG_WARN("failed to set root path", K(ret), K(path), K(strlen(path)));
  } else if (OB_FAIL(storage_info_->set(type, endpoint, authorization, extension))) {
    LOG_WARN("failed to set storage info", K(ret), K(endpoint), K(authorization), K(extension));
  } else {
    root_path_trim_();
  }
  return ret;
}

int ObBackupDest::set(const char *root_path, const char *storage_info)
{
  int ret = OB_SUCCESS;
  reset();
  common::ObStorageType type;
  char storage_info_str[OB_MAX_BACKUP_STORAGE_INFO_LENGTH] = { 0 };
  if (is_valid()) {
    ret = OB_INIT_TWICE;
    LOG_WARN("cannot init twice", K(ret), K(*this));
  } else if (OB_ISNULL(root_path) || OB_ISNULL(storage_info)) {
    ret = OB_INVALID_BACKUP_DEST;
    LOG_WARN("invalid args", K(ret));
  } else if (OB_FAIL(alloc_and_init())) {
    LOG_WARN("failed to alloc and init backup dest", K(ret));
  } else if (OB_FAIL(get_storage_type_from_path(root_path, type))) {
    LOG_WARN("failed to get storage type", K(ret));
  } else if (OB_FAIL(databuff_printf(root_path_, OB_MAX_BACKUP_PATH_LENGTH, "%s", root_path))) {
    LOG_WARN("failed to set root path", K(ret), K(root_path));
  } else if (OB_FAIL(storage_info_->set(type, storage_info))) {
    LOG_WARN("failed to set storage info", K(ret));
  } else {
    root_path_trim_();
  }
  return ret;
}

int ObBackupDest::set(const char *root_path, const ObBackupStorageInfo *storage_info)
{
  int ret = OB_SUCCESS;
  reset();
  if (is_valid()) {
    ret = OB_INIT_TWICE;
    LOG_WARN("cannot init twice", K(ret), K(*this));
  } else if (OB_ISNULL(root_path) || OB_ISNULL(storage_info) || !storage_info->is_valid()) {
    ret = OB_INVALID_BACKUP_DEST;
    LOG_WARN("invalid args", K(ret), K(root_path), K(storage_info));
  } else if (OB_FAIL(alloc_and_init())) {
    LOG_WARN("failed to alloc and init backup dest", K(ret));
  } else if (OB_FAIL(databuff_printf(root_path_, OB_MAX_BACKUP_PATH_LENGTH, "%s", root_path))) {
    LOG_WARN("failed to set root path", K(ret), K(root_path));
  } else if (OB_FAIL(storage_info_->assign(*storage_info))) {
    LOG_WARN("failed to set storage info", K(ret));
  } else {
    root_path_trim_();
  }
  return ret;
}


void ObBackupDest::root_path_trim_()
{
  int len = static_cast<int32_t>(strlen(root_path_));
  for (int i = len - 1; i >=0 ; i--) {
    if (root_path_[i] == '/') {
      root_path_[i] = '\0';
    } else {
      break;
    }
  }
}

bool ObBackupDest::is_root_path_equal(const ObBackupDest &backup_dest) const
{
  bool is_equal = true;
  if ((OB_ISNULL(root_path_) && !OB_ISNULL(backup_dest.root_path_))
      || (!OB_ISNULL(root_path_) && OB_ISNULL(backup_dest.root_path_))) {
    is_equal = false;
  } else if (!OB_ISNULL(root_path_) && !OB_ISNULL(backup_dest.root_path_)) {
    if (strlen(root_path_) != strlen(backup_dest.root_path_)) {
      is_equal = false;
    } else if (0 != MEMCMP(root_path_, backup_dest.root_path_, strlen(root_path_))) {
      is_equal = false;
    }
  }
  return is_equal;
}

int ObBackupDest::is_backup_path_equal(const ObBackupDest &backup_dest, bool &is_equal) const
{
  int ret = OB_SUCCESS;
  is_equal = true;
  if (!is_valid() || !backup_dest.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("backup dest is valid", K(ret), K(*this), K(backup_dest));
  } else if(0 != MEMCMP(root_path_, backup_dest.root_path_, OB_MAX_BACKUP_PATH_LENGTH)) {
    is_equal = false;
  } else if (0 != MEMCMP(storage_info_->endpoint_, backup_dest.storage_info_->endpoint_, sizeof(storage_info_->endpoint_))) {
    is_equal = false;
  }
  return ret;
}

// backup_path = root_path + host
int ObBackupDest::get_backup_path_str(char *buf, const int64_t buf_size) const
{
  int ret = OB_SUCCESS;
  if (!is_valid()) {
    ret = OB_NOT_INIT;
    LOG_WARN("backup dest is not init", K(ret), K(*this));
  } else if (OB_ISNULL(buf) || buf_size < share::OB_MAX_BACKUP_DEST_LENGTH) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get backup dest str get invalid argument", K(ret), KP(buf), K(buf_size));
  } else if (OB_FAIL(databuff_printf(buf, buf_size, "%s", root_path_))) {
    LOG_WARN("failed to set backup path", K(ret), K(root_path_), K(sizeof(root_path_)));
  } else if (ObStorageType::OB_STORAGE_FILE != storage_info_->device_type_) {
    const int64_t str_len = strlen(buf);
    if (OB_FAIL(databuff_printf(buf + str_len, buf_size - str_len, "?%s", storage_info_->endpoint_))) {
      LOG_WARN("failed to set backup path", K(ret), K(storage_info_->endpoint_));
    }
  }
  return ret;
}

// backup_dest access_key encrypt
int ObBackupDest::get_backup_dest_str(char *buf, const int64_t buf_size) const
{
  int ret = OB_SUCCESS;
  char storage_info_str[OB_MAX_BACKUP_STORAGE_INFO_LENGTH] = { 0 };
  if (!is_valid()) {
    ret = OB_NOT_INIT;
    LOG_WARN("backup dest is not init", K(ret), K(*this));
  } else if (OB_ISNULL(buf) || buf_size < share::OB_MAX_BACKUP_DEST_LENGTH) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", K(ret), KP(buf), K(buf_size));
  } else if (OB_FAIL(databuff_printf(buf, buf_size, "%s", root_path_))) {
    LOG_WARN("failed to get backup dest str", K(ret), K(root_path_), K(storage_info_));
  } else if (OB_FAIL(storage_info_->get_storage_info_str(storage_info_str, sizeof(storage_info_str), true/*no need encrypt*/))) {
    OB_LOG(WARN, "fail to get storage info str!", K(ret), K(storage_info_));
  } else if (0 != strlen(storage_info_str) && OB_FAIL(databuff_printf(buf + strlen(buf), buf_size - strlen(buf), "?%s",storage_info_str))) {
    LOG_WARN("failed to get backup dest str", K(ret), K(root_path_), K(storage_info_));
  }

  return ret;
}

int64_t ObBackupDest::to_string(char *buf, int64_t buf_len) const
{
  int64_t pos = 0;
  if (OB_ISNULL(buf) || buf_len <= 0 || !is_valid()) {
    // do nothing
  } else {
    J_OBJ_START();
    ObString root_path(root_path_);
    J_KV(K(root_path), K_(storage_info));
    J_OBJ_END();
  }
  return pos;
}

const char *ObBackupInfoStatus::get_status_str(const BackupStatus &status)
{
  const char *str = "UNKNOWN";
  const char *info_backup_status_strs[] = {
      "STOP",
      "PREPARE",
      "SCHEDULE",
      "DOING",
      "CANCEL",
      "CLEANUP",
  };
  STATIC_ASSERT(MAX == ARRAYSIZEOF(info_backup_status_strs), "status count mismatch");
  if (status < 0 || status >= MAX) {
    LOG_ERROR_RET(OB_INVALID_ARGUMENT, "invalid backup info status", K(status));
  } else {
    str = info_backup_status_strs[status];
  }
  return str;
}

const char *ObBackupInfoStatus::get_info_backup_status_str() const
{
  return get_status_str(status_);
}

int ObBackupInfoStatus::set_info_backup_status(
    const char *buf)
{
  int ret = OB_SUCCESS;
  if (OB_ISNULL(buf)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("set info backup status get invalid argument", K(ret), KP(buf));
  } else {
    const char *info_backup_status_strs[] = {
        "STOP",
        "PREPARE",
        "SCHEDULE",
        "DOING",
        "CANCEL",
        "CLEANUP",
    };
    BackupStatus tmp_status = MAX;
    STATIC_ASSERT(MAX == ARRAYSIZEOF(info_backup_status_strs), "status count mismatch");
    for (int64_t i = 0; i < ARRAYSIZEOF(info_backup_status_strs); i++) {
      if (0 == STRCMP(info_backup_status_strs[i], buf)) {
        tmp_status = static_cast<BackupStatus>(i);
      }
    }
    if (MAX == tmp_status) {
      ret = OB_ENTRY_NOT_EXIST;
      LOG_WARN("backup info status str not found", K(ret), K(buf));
    } else {
      status_ = tmp_status;
    }
  }
  return ret;
}


/* ObBackupSetDesc */

ObBackupSetDesc::ObBackupSetDesc() : backup_set_id_(-1), backup_type_()
{}

bool ObBackupSetDesc::is_valid() const
{
  return backup_set_id_ > 0;
}

int ObBackupSetDesc::assign(const ObBackupSetDesc &that)
{
  int ret = OB_SUCCESS;
  if (!that.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", K(ret));
  } else {
    backup_set_id_ = that.backup_set_id_;
    backup_type_.type_ = that.backup_type_.type_;
  }
  return ret;
}

bool ObBackupSetDesc::operator==(const ObBackupSetDesc &other) const
{
  return backup_set_id_ == other.backup_set_id_ && backup_type_.type_ == other.backup_type_.type_;
}

void ObBackupSetDesc::reset()
{
  backup_set_id_ = -1;
  backup_type_.type_ = ObBackupType::MAX;
}

OB_SERIALIZE_MEMBER(ObBackupType, type_);

const char *ObBackupType::get_backup_type_str() const
{
  const char *str = "UNKNOWN";
  const char *backup_func_type_strs[] = {
    "",
    "FULL",
    "INC",
  };
  STATIC_ASSERT(MAX == ARRAYSIZEOF(backup_func_type_strs), "types count mismatch");
  if (type_ < 0 || type_ >= MAX) {
    LOG_ERROR_RET(OB_ERR_UNEXPECTED, "invalid backup type", K(type_));
  } else {
    str = backup_func_type_strs[type_];
  }
  return str;
}

int ObBackupType::set_backup_type(
    const char *buf)
{
  int ret = OB_SUCCESS;
  if (OB_ISNULL(buf)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("set backup type get invalid argument", K(ret), KP(buf));
  } else {
    const char *backup_func_type_strs[] = {
      "",
      "FULL",
      "INC",
    };
    BackupType tmp_type = MAX;
    STATIC_ASSERT(MAX == ARRAYSIZEOF(backup_func_type_strs), "types count mismatch");
    for (int64_t i = 0; i < ARRAYSIZEOF(backup_func_type_strs); i++) {
      if (0 == STRCMP(backup_func_type_strs[i], buf)) {
        tmp_type = static_cast<BackupType>(i);
      }
    }

    if (MAX == tmp_type) {
      ret = OB_ENTRY_NOT_EXIST;
      LOG_WARN("backup type str not found", K(ret), K(buf));
    } else {
      type_ = tmp_type;
    }
  }
  return ret;
}

ObBaseBackupInfoStruct::ObBaseBackupInfoStruct()
  : tenant_id_(OB_INVALID_ID), backup_set_id_(0), incarnation_(0), backup_dest_(),
    backup_backup_dest_(), backup_snapshot_version_(0), backup_schema_version_(0),
    backup_data_version_(0), detected_backup_region_(), backup_type_(),
    backup_status_(), backup_task_id_(0),
    encryption_mode_(share::ObBackupEncryptionMode::MAX_MODE),
    passwd_()
{
}

void ObBaseBackupInfoStruct::reset()
{
  tenant_id_ = OB_INVALID_ID;
  backup_set_id_ = 0;
  incarnation_ = 0;
  backup_dest_.reset();
  backup_backup_dest_.reset();
  backup_snapshot_version_ = 0;
  backup_schema_version_ = 0;
  backup_data_version_ = 0;
  detected_backup_region_.reset();
  backup_type_.reset();
  backup_status_.reset();
  backup_task_id_ = 0;
  encryption_mode_ = share::ObBackupEncryptionMode::MAX_MODE;
  passwd_.reset();
}

bool ObBaseBackupInfoStruct::is_valid() const
{
  return OB_INVALID_ID != tenant_id_
      && tenant_id_ > 0
      && backup_status_.is_valid()
      && share::ObBackupEncryptionMode::is_valid(encryption_mode_);
}

bool ObBaseBackupInfoStruct::has_cleaned() const
{
  return is_valid()
      && backup_status_.is_stop_status()
      && 0 == backup_snapshot_version_
      && 0 == backup_schema_version_
      && 0 == backup_data_version_
      && ObBackupType::EMPTY == backup_type_.type_;
}

ObBaseBackupInfoStruct &ObBaseBackupInfoStruct::operator =(
    const ObBaseBackupInfoStruct &info)
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(assign(info))) {
    LOG_ERROR("failed to assign backup info", K(ret), K(info));
  }
  return *this;
}

int ObBaseBackupInfoStruct::assign(const ObBaseBackupInfoStruct &backup_info_struct)
{
  int ret = OB_SUCCESS;
  if (!backup_info_struct.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_ERROR("backup info struct is invalid", K(ret), K(backup_info_struct));
  } else {
    backup_dest_ = backup_info_struct.backup_dest_;
    backup_backup_dest_ = backup_info_struct.backup_backup_dest_;
    detected_backup_region_ = backup_info_struct.detected_backup_region_;
    tenant_id_ = backup_info_struct.tenant_id_;
    backup_set_id_ = backup_info_struct.backup_set_id_;
    incarnation_ = backup_info_struct.incarnation_;
    backup_snapshot_version_ = backup_info_struct.backup_snapshot_version_;
    backup_schema_version_ = backup_info_struct.backup_schema_version_;
    backup_data_version_ = backup_info_struct.backup_data_version_;
    backup_type_ = backup_info_struct.backup_type_;
    backup_status_ = backup_info_struct.backup_status_;
    backup_task_id_ = backup_info_struct.backup_task_id_;
    encryption_mode_ = backup_info_struct.encryption_mode_;
    passwd_ = backup_info_struct.passwd_;
  }
  return ret;
}

int ObBaseBackupInfoStruct::check_backup_info_match(
    const ObBaseBackupInfoStruct &backup_info_struct) const
{
  int ret = OB_SUCCESS;
  if (!backup_info_struct.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_ERROR("backup info struct is invalid", K(ret), K(backup_info_struct));
  } else if (backup_dest_ != backup_info_struct.backup_dest_
      || tenant_id_ != backup_info_struct.tenant_id_
      || backup_set_id_ != backup_info_struct.backup_set_id_
      || incarnation_ != backup_info_struct.incarnation_
      || backup_snapshot_version_ != backup_info_struct.backup_snapshot_version_
      || backup_schema_version_ != backup_info_struct.backup_schema_version_
      || backup_data_version_ != backup_info_struct.backup_data_version_
      || backup_type_.type_ != backup_info_struct.backup_type_.type_
      || backup_status_.status_ != backup_info_struct.backup_status_.status_
      || encryption_mode_ != backup_info_struct.encryption_mode_
      || passwd_ != backup_info_struct.passwd_) {
    ret = OB_BACKUP_INFO_NOT_MATCH;
    LOG_WARN("backup info is not match", K(ret), K(*this), K(backup_info_struct));
  }
  return ret;
}

int ObBackupUtils::check_tenant_data_version_match(const uint64_t tenant_id, const uint64_t data_version)
{
  int ret = OB_SUCCESS;
  uint64_t cur_data_version = 0;
  if (!is_valid_tenant_id(tenant_id)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", K(ret), K(tenant_id));
  } else if (OB_FAIL(GET_MIN_DATA_VERSION(tenant_id, cur_data_version))) {
    LOG_WARN("failed to get min data version", K(ret), K(tenant_id));
  } else if (cur_data_version != data_version) {
    ret = OB_VERSION_NOT_MATCH;
    LOG_WARN("tenant data version is not match", K(ret), K(tenant_id), K(cur_data_version), K(data_version));
  }
  return ret;
}

int ObBackupUtils::get_backup_info_default_timeout_ctx(ObTimeoutCtx &ctx)
{
  int ret = OB_SUCCESS;
  const int64_t DEFAULT_TIMEOUT_US = 2 * 1000 * 1000; // 2s
  int64_t abs_timeout_us = ctx.get_abs_timeout();
  int64_t worker_timeout_us = THIS_WORKER.get_timeout_ts();

  if (abs_timeout_us < 0) {
    abs_timeout_us = ObTimeUtility::current_time() + DEFAULT_TIMEOUT_US;
  }

  if (INT64_MAX == worker_timeout_us) {
    abs_timeout_us = ObTimeUtility::current_time() + DEFAULT_TIMEOUT_US;
  } else if (worker_timeout_us > 0 && worker_timeout_us < abs_timeout_us) {
    abs_timeout_us = worker_timeout_us;
  }

  if (OB_FAIL(ctx.set_abs_timeout(abs_timeout_us))) {
    LOG_WARN("set timeout failed", K(ret), K(abs_timeout_us));
  } else  if (ctx.is_timeouted()) {
    ret = OB_TIMEOUT;
    LOG_WARN("is timeout", K(ret),
             "abs_timeout", ctx.get_abs_timeout(),
             "this worker timeout ts", THIS_WORKER.get_timeout_ts());
  }
  return ret;
}

bool ObBackupUtils::is_need_retry_error(const int err)
{
  bool bret = true;
  switch (err) {
    case OB_NOT_INIT :
    case OB_INVALID_ARGUMENT :
    case OB_ERR_UNEXPECTED :
    case OB_ERR_SYS :
    case OB_INIT_TWICE :
    case OB_SRC_DO_NOT_ALLOWED_MIGRATE :
    case OB_CANCELED :
    case OB_BACKUP_DATA_VERSION_GAP_OVER_LIMIT :
    case OB_LOG_ARCHIVE_STAT_NOT_MATCH :
    case OB_NOT_SUPPORTED :
    case OB_TENANT_HAS_BEEN_DROPPED :
    case OB_SERVER_OUTOF_DISK_SPACE :
    case OB_HASH_NOT_EXIST:
    case OB_ARCHIVE_LOG_NOT_CONTINUES_WITH_DATA :
    case OB_BACKUP_DELETE_BACKUP_SET_NOT_ALLOWED :
    case OB_BACKUP_DELETE_BACKUP_PIECE_NOT_ALLOWED :
    case OB_BACKUP_FORMAT_FILE_NOT_EXIST :
    case OB_BACKUP_FORMAT_FILE_NOT_MATCH :
    case OB_BACKUP_PERMISSION_DENIED :
    case OB_BACKUP_DEVICE_OUT_OF_SPACE :
    case OB_BACKUP_DEST_NOT_CONNECT :
    case OB_BACKUP_FILE_NOT_EXIST :
    case OB_LOG_ARCHIVE_INTERRUPTED :
    case OB_LOG_ARCHIVE_NOT_RUNNING :
    case OB_CHECKSUM_ERROR :
      bret = false;
      break;
    default:
      break;
  }
  return bret;
}

int ObBackupUtils::retry_get_tenant_schema_guard(
    const uint64_t tenant_id,
    schema::ObMultiVersionSchemaService &schema_service,
    const int64_t tenant_schema_version,
    schema::ObSchemaGetterGuard &schema_guard)
{
  int ret = OB_SUCCESS;
  const schema::ObMultiVersionSchemaService::RefreshSchemaMode refresh_mode =
      schema::ObMultiVersionSchemaService::RefreshSchemaMode::FORCE_FALLBACK;

  if (OB_UNLIKELY(OB_INVALID_ID == tenant_id
      || tenant_schema_version < OB_INVALID_VERSION)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", K(tenant_id), K(tenant_schema_version));
  } else {
    int retry_times = 0;
    const int64_t sys_schema_version = OB_INVALID_VERSION;//sys_schema_version use latest
    while (retry_times < MAX_RETRY_TIMES) {
      if (OB_FAIL(schema_service.get_tenant_schema_guard(
          tenant_id,
          schema_guard,
          tenant_schema_version,
          sys_schema_version,
          refresh_mode))) {
        STORAGE_LOG(WARN, "fail to get schema, sleep 1s and retry",
            K(ret), K(tenant_id), K(tenant_schema_version), K(sys_schema_version));
        ob_usleep(RETRY_INTERVAL);
      } else {
        break;
      }
      ++retry_times;
    }
  }
  return ret;
}

int ObBackupUtils::convert_timestamp_to_date(
    const int64_t snapshot_version,
    int64_t &date)
{
  int ret = OB_SUCCESS;
  date = 0;
  static const int64_t MAX_BUF_LENGTH = 64;
  char buf[MAX_BUF_LENGTH] = {0};
  int64_t tmp_date = 0;

  if (snapshot_version <=0) {
    ret = OB_INVALID_ARGUMENT;
    STORAGE_LOG(WARN, "get snapshot to time str get invalid argument", K(ret), K(snapshot_version));
  } else {
    const time_t rawtime = snapshot_version / (1000 * 1000); //second
    struct tm time_info;
    struct tm *time_info_ptr = NULL;

    if (NULL == (time_info_ptr = (localtime_r(&rawtime, &time_info)))) {
      ret = OB_ERR_SYS;
      LOG_WARN("get localtime failed", K(ret));
    } else if (0 == strftime(buf, MAX_BUF_LENGTH, "%Y%m%d", time_info_ptr)) {
      ret = OB_ERR_UNEXPECTED;
      STORAGE_LOG(WARN, "failed to strftime", K(ret), K(buf), K(snapshot_version));
    } else if (OB_FAIL(ob_atoll(buf, tmp_date))) {
      STORAGE_LOG(WARN, "failed to atoll", K(ret), K(buf));
    } else {
      date = tmp_date;
    }
  }
  return ret;
}

int ObBackupUtils::get_backup_scn(const uint64_t &tenant_id, share::SCN &scn)
{
  int ret = OB_SUCCESS;
  if (OB_UNLIKELY(OB_INVALID_TENANT_ID == tenant_id)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("tenant id is invalid", KR(ret), K(tenant_id));
  } else {
    ret = OB_EAGAIN;
    const transaction::MonotonicTs stc = transaction::MonotonicTs::current_time();
    transaction::MonotonicTs unused_ts(0);
    const int64_t start_time = ObTimeUtility::fast_current_time();
    const int64_t TIMEOUT = 10 * 1000 * 1000; //10s
    while (OB_EAGAIN == ret) {
      if (ObTimeUtility::fast_current_time() - start_time > TIMEOUT) {
        ret = OB_TIMEOUT;
        LOG_WARN("stmt is timeout", KR(ret), K(start_time), K(TIMEOUT));
      } else if (OB_FAIL(OB_TS_MGR.get_gts(tenant_id, stc, NULL,
                                           scn, unused_ts))) {
        if (OB_EAGAIN != ret) {
          LOG_WARN("failed to get gts", KR(ret), K(tenant_id));
        } else {
          // waiting 10ms
          ob_usleep(10L * 1000L);
        }
      }
    }
  }
  LOG_INFO("get tenant gts", KR(ret), K(tenant_id), K(scn));
  return ret;
}

int ObBackupUtils::convert_timestamp_to_timestr(const int64_t ts, char *buf, int64_t len)
{
  int ret = OB_SUCCESS;
  const int64_t TIME_MICROSECOND = 1UL;
  const int64_t TIME_MILLISECOND = 1000UL;
  const int64_t TIME_SECOND = 1000 * 1000UL;
  const int64_t TIME_MINUTE = 60 * 1000 * 1000UL;
  const int64_t TIME_HOUR = 60 * 60 * 1000 * 1000UL;
  const int64_t TIME_DAY = 24 * 60 * 60 * 1000 * 1000UL;
  if (0 >= ts || OB_ISNULL(buf) || 0 >= len) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid ts", K(ret), K(ts), KP(buf), K(len));
  } else if (ts >= TIME_DAY) {
    if (OB_FAIL(databuff_printf(buf, len, "%ldd", ts / TIME_DAY))) {
      LOG_WARN("failed to convert time to timestr", K(ret), K(ts), K(len));
    }
  } else if (ts >= TIME_HOUR) {
    if (OB_FAIL(databuff_printf(buf, len, "%ldh", ts / TIME_HOUR))) {
      LOG_WARN("failed to convert time to timestr", K(ret), K(ts), K(len));
    }
  } else if (ts >= TIME_MINUTE) {
    if (OB_FAIL(databuff_printf(buf, len, "%ldm", ts / TIME_MINUTE))) {
      LOG_WARN("failed to convert time to timestr", K(ret), K(ts), K(len));
    }
  } else if (ts >= TIME_SECOND) {
    if (OB_FAIL(databuff_printf(buf, len, "%lds", ts / TIME_SECOND))) {
      LOG_WARN("failed to convert time to timestr", K(ret), K(ts), K(len));
    }
  } else if (ts >= TIME_MILLISECOND) {
    if (OB_FAIL(databuff_printf(buf, len, "%ldms", ts / TIME_MILLISECOND))) {
      LOG_WARN("failed to convert time to timestr", K(ret), K(ts), K(len));
    }
  } else {
    // ts >= TIME_MICROSECOND
    if (OB_FAIL(databuff_printf(buf, len, "%ldus", ts / TIME_MICROSECOND))) {
      LOG_WARN("failed to convert time to timestr", K(ret), K(ts), K(len));
    }
  }

  return ret;
}

bool ObBackupUtils::can_backup_pieces_be_deleted(const ObBackupPieceStatus::STATUS &status)
{
  return ObBackupPieceStatus::BACKUP_PIECE_INACTIVE == status
      || ObBackupPieceStatus::BACKUP_PIECE_FROZEN == status;
}

int ObBackupUtils::check_passwd(const char *passwd_array, const char *passwd)
{
  int ret = OB_SUCCESS;
  if (OB_ISNULL(passwd_array) || OB_ISNULL(passwd)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid args", K(ret), KP(passwd_array), KP(passwd));
  } else if (STRLEN(passwd) == 0) {
    // empty password no need check
  } else if (OB_ISNULL(STRSTR(passwd_array, passwd))) {
    ret = OB_BACKUP_INVALID_PASSWORD;
    LOG_WARN("no valid passwd found", K(ret), K(passwd_array), K(passwd));
  }
  return ret;
}

int ObBackupUtils::check_is_tmp_file(const common::ObString &file_name, bool &is_tmp_file)
{
  int ret = OB_SUCCESS;
  is_tmp_file = false;
  char *buf = NULL;
  ObArenaAllocator allocator;
  if (file_name.empty()) {
    // no need check
  } else if (OB_ISNULL(buf = static_cast<char *>(allocator.alloc(file_name.length() + 1)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("allocate memory failed", K(ret), K(file_name));
  } else {
    STRNCPY(buf, file_name.ptr(), file_name.length());
    buf[file_name.length()] = '\0';
    char *ptr = STRSTR(buf, OB_STR_TMP_FILE_MARK);
    if (OB_ISNULL(ptr)) {
      is_tmp_file = false;
    } else {
      ptr = ptr + STRLEN(OB_STR_TMP_FILE_MARK);
      bool is_valid_tmp_file = false;
      while (OB_NOT_NULL(ptr)) {
        if ('\0' == *ptr) {
          break; // reach str end
        } else if (!isdigit(*ptr)) {
          is_valid_tmp_file = false;
          break;
        } else {
          is_valid_tmp_file = true;
        }
        ++ptr;
      }
      if (is_valid_tmp_file) {
        is_tmp_file = true;
      } else {
        is_tmp_file = false;
      }
    }
  }
  return ret;
}

ObPhysicalRestoreInfo::ObPhysicalRestoreInfo()
  : cluster_id_(0),
    incarnation_(0),
    tenant_id_(0),
    full_backup_set_id_(0),
    inc_backup_set_id_(0),
    log_archive_round_(0),
    restore_snapshot_version_(OB_INVALID_TIMESTAMP),
    restore_start_ts_(0),
    compatible_(0),
    cluster_version_(0),
    multi_restore_path_list_(),
    backup_date_(0)
{
  backup_dest_[0] = '\0';
  cluster_name_[0] = '\0';
}

void ObPhysicalRestoreInfo::destroy()
{
  backup_dest_[0] = '\0';
  cluster_name_[0] = '\0';
  cluster_id_ = 0;
  incarnation_ = 0;
  tenant_id_ = 0;
  full_backup_set_id_ = 0;
  inc_backup_set_id_ =0;
  log_archive_round_ = 0;
  restore_snapshot_version_ = OB_INVALID_TIMESTAMP;
  restore_start_ts_ = 0;
  compatible_ = 0;
  cluster_version_ = 0;
  backup_date_ = 0;
}

bool ObPhysicalRestoreInfo::is_valid() const
{
  return (strlen(backup_dest_) > 0 || multi_restore_path_list_.get_backup_set_path_list().count() > 0)
      && !(strlen(backup_dest_) > 0 && multi_restore_path_list_.get_backup_set_path_list().count() > 0)
      && strlen(cluster_name_) > 0
      && cluster_id_ > 0 && OB_START_INCARNATION == incarnation_
      && tenant_id_ > 0 && full_backup_set_id_ > 0 && inc_backup_set_id_ > 0
      && log_archive_round_ > 0 && restore_snapshot_version_ > 0 && restore_start_ts_ > 0
      && compatible_ > 0
      && cluster_version_ > 0
      && backup_date_ >= 0;
}

void ObPhysicalRestoreInfo::set_array_label(const char *lable)
{
  UNUSED(lable);
  // backup_piece_list_.set_label(lable);
  // backup_set_list_.set_label(lable);
}


int ObPhysicalRestoreInfo::assign(const ObPhysicalRestoreInfo &other)
{
  int ret = OB_SUCCESS;
  STRNCPY(backup_dest_, other.backup_dest_, share::OB_MAX_BACKUP_DEST_LENGTH);
  STRNCPY(cluster_name_, other.cluster_name_, common::OB_MAX_CLUSTER_NAME_LENGTH);
  cluster_id_ = other.cluster_id_;
  incarnation_ = other.incarnation_;
  tenant_id_ = other.tenant_id_;
  full_backup_set_id_ = other.full_backup_set_id_;
  inc_backup_set_id_ = other.inc_backup_set_id_;
  log_archive_round_ = other.log_archive_round_;
  restore_snapshot_version_ = other.restore_snapshot_version_;
  restore_start_ts_ = other.restore_start_ts_;
  compatible_ = other.compatible_;
  cluster_version_ = other.cluster_version_;
  backup_date_ = other.backup_date_;
  if (OB_FAIL(multi_restore_path_list_.assign(other.multi_restore_path_list_))) {
    LOG_WARN("failed to assign multi restore path list", KR(ret));
  }
  return ret;
}

OB_SERIALIZE_MEMBER(ObPhysicalRestoreInfo,
    backup_dest_,
    cluster_name_,
    cluster_id_,
    incarnation_,
    tenant_id_,
    full_backup_set_id_,
    inc_backup_set_id_,
    log_archive_round_,
    restore_snapshot_version_,
    restore_start_ts_,
    compatible_,
    cluster_version_,
    multi_restore_path_list_,
    backup_date_);

ObBackupPieceInfoKey::ObBackupPieceInfoKey()
  : incarnation_(0),
    tenant_id_(0),
    round_id_(0),
    backup_piece_id_(0),
    copy_id_(0)
{
}

bool ObBackupPieceInfoKey::is_valid() const
{
  return OB_START_INCARNATION == incarnation_
      && tenant_id_ > 0
      && round_id_ > 0
      && backup_piece_id_ >= 0
      && copy_id_ >= 0;
}

void ObBackupPieceInfoKey::reset()
{
  incarnation_ = 0;
  tenant_id_ = 0;
  round_id_ = 0;
  backup_piece_id_ = 0;
  copy_id_ = 0;
}

bool ObBackupPieceInfoKey::operator==(const ObBackupPieceInfoKey &o) const
{
  return incarnation_ == o.incarnation_
      && tenant_id_ == o.tenant_id_
      && round_id_ == o.round_id_
      && backup_piece_id_ == o.backup_piece_id_
      && copy_id_ == o.copy_id_;
}


bool ObBackupPieceInfoKey::operator < (const  ObBackupPieceInfoKey &o) const
{
  bool result = false;

  if (incarnation_ < o.incarnation_) {
    result = true;
  } else if (incarnation_ > o.incarnation_) {
    result = false;
  } else if (tenant_id_ < o.tenant_id_) {
    result = true;
  } else if (tenant_id_ > o.tenant_id_) {
    result = false;
  } else if (round_id_ < o.round_id_) {
    result = true;
  } else if (round_id_ > o.round_id_) {
    result = false;
  } else if (backup_piece_id_ < o.backup_piece_id_) {
    result = true;
  } else if (backup_piece_id_ > o.backup_piece_id_) {
    result = false;
  } else if (copy_id_ < o.copy_id_) {
    result = true;
  } else {
    result = false;
  }
  return result;
}

static const char *PIECE_STATUS_STRS[] = {
    "ACTIVE",
    "FREEZING",
    "FROZEN",
    "INACTIVE",
};

const char *ObBackupPieceStatus::get_str(const STATUS &status)
{
  const char *str = nullptr;

  if (status < 0 || status >= BACKUP_PIECE_MAX) {
    str = "UNKNOWN";
  } else {
    str = PIECE_STATUS_STRS[status];
  }
  return str;

}

ObBackupPieceStatus::STATUS ObBackupPieceStatus::get_status(const char *status_str)
{
  ObBackupPieceStatus::STATUS status = ObBackupPieceStatus::BACKUP_PIECE_MAX;

  const int64_t count = ARRAYSIZEOF(PIECE_STATUS_STRS);
  STATIC_ASSERT(static_cast<int64_t>(ObBackupPieceStatus::BACKUP_PIECE_MAX) == count, "status count mismatch");
  for (int64_t i = 0; i < count; ++i) {
    if (0 == strcmp(status_str, PIECE_STATUS_STRS[i])) {
      status = static_cast<ObBackupPieceStatus::STATUS>(i);
      break;
    }
  }
  return status;
}

static const char *PIECE_FILE_STATUS_STRS[] = {
    "AVAILABLE",
    "COPYING",
    "INCOMPLETE",
    "DELETING",
    "EXPIRED",
    "BROKEN",
    "DELETED",
};

const char *ObBackupFileStatus::get_str(const STATUS &status)
{
  const char *str = nullptr;

  if (status < 0 || status >= BACKUP_FILE_MAX) {
    str = "UNKNOWN";
  } else {
    str = PIECE_FILE_STATUS_STRS[status];
  }
  return str;

}

ObBackupFileStatus::STATUS ObBackupFileStatus::get_status(const char *status_str)
{
  ObBackupFileStatus::STATUS status = ObBackupFileStatus::BACKUP_FILE_MAX;

  const int64_t count = ARRAYSIZEOF(PIECE_FILE_STATUS_STRS);
  STATIC_ASSERT(static_cast<int64_t>(ObBackupFileStatus::BACKUP_FILE_MAX) == count, "status count mismatch");
  for (int64_t i = 0; i < count; ++i) {
    if (0 == strcmp(status_str, PIECE_FILE_STATUS_STRS[i])) {
      status = static_cast<ObBackupFileStatus::STATUS>(i);
      break;
    }
  }
  return status;
}

int ObBackupFileStatus::check_can_change_status(
    const ObBackupFileStatus::STATUS &src_file_status,
    const ObBackupFileStatus::STATUS &dest_file_status)
{
  int ret = OB_SUCCESS;
  if (ObBackupFileStatus::BACKUP_FILE_MAX == src_file_status
      || ObBackupFileStatus::BACKUP_FILE_MAX == dest_file_status) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("check can change status get invalid argument", K(ret), K(src_file_status), K(dest_file_status));
  } else {
    switch (src_file_status) {
    case ObBackupFileStatus::BACKUP_FILE_AVAILABLE : {
      if (ObBackupFileStatus::BACKUP_FILE_BROKEN != dest_file_status
          && ObBackupFileStatus::BACKUP_FILE_EXPIRED != dest_file_status
          && ObBackupFileStatus::BACKUP_FILE_DELETING != dest_file_status
          && ObBackupFileStatus::BACKUP_FILE_AVAILABLE != dest_file_status
          && ObBackupFileStatus::BACKUP_FILE_INCOMPLETE != dest_file_status/*for sys backup backup*/) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("can not change backup file status", K(ret), K(src_file_status), K(dest_file_status));
      }
      break;
    }
    case ObBackupFileStatus::BACKUP_FILE_INCOMPLETE : {
      if (ObBackupFileStatus::BACKUP_FILE_COPYING != dest_file_status
          && ObBackupFileStatus::BACKUP_FILE_INCOMPLETE != dest_file_status
          && ObBackupFileStatus::BACKUP_FILE_DELETING != dest_file_status) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("can not change backup file status", K(ret), K(src_file_status), K(dest_file_status));
      }
      break;
    }
    case ObBackupFileStatus::BACKUP_FILE_COPYING : {
      if (ObBackupFileStatus::BACKUP_FILE_COPYING != dest_file_status
          && ObBackupFileStatus::BACKUP_FILE_AVAILABLE != dest_file_status
          && ObBackupFileStatus::BACKUP_FILE_INCOMPLETE != dest_file_status) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("can not change backup file status", K(ret), K(src_file_status), K(dest_file_status));
      }
      break;
    }
    case ObBackupFileStatus::BACKUP_FILE_BROKEN : {
      if (ObBackupFileStatus::BACKUP_FILE_BROKEN != dest_file_status
          && ObBackupFileStatus::BACKUP_FILE_DELETING != dest_file_status) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("can not change backup file status", K(ret), K(src_file_status), K(dest_file_status));
      }
      break;
    }
    case ObBackupFileStatus::BACKUP_FILE_EXPIRED : {
      if (ObBackupFileStatus::BACKUP_FILE_EXPIRED != dest_file_status
          && ObBackupFileStatus::BACKUP_FILE_AVAILABLE != dest_file_status
          && ObBackupFileStatus::BACKUP_FILE_DELETING != dest_file_status) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("can not change backup file status", K(ret), K(src_file_status), K(dest_file_status));
      }
      break;
    }
    case ObBackupFileStatus::BACKUP_FILE_DELETING : {
      if (ObBackupFileStatus::BACKUP_FILE_DELETING != dest_file_status
          && ObBackupFileStatus::BACKUP_FILE_DELETED != dest_file_status) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("can not change backup file status", K(ret), K(src_file_status), K(dest_file_status));
      }
      break;
    }
    case ObBackupFileStatus::BACKUP_FILE_DELETED : {
      if (ObBackupFileStatus::BACKUP_FILE_DELETED != dest_file_status) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("can not update backup set file info", K(ret), K(src_file_status), K(dest_file_status));
      }
      break;
    }
    case ObBackupFileStatus::BACKUP_FILE_MAX : {
      ret = OB_ERR_UNEXPECTED;
      LOG_WARN("can not update backup task", K(ret), K(src_file_status), K(dest_file_status));
      break;
    }
    default: {
      ret = OB_ERR_UNEXPECTED;
      LOG_WARN("unknown file status", K(ret), K(src_file_status), K(dest_file_status));
      break;
    }
    }
  }
  return ret;
}

bool ObBackupFileStatus::can_show_in_preview(
     const ObBackupFileStatus::STATUS &status)
{
  return ObBackupFileStatus::BACKUP_FILE_AVAILABLE == status
      || ObBackupFileStatus::BACKUP_FILE_DELETING == status
      || ObBackupFileStatus::BACKUP_FILE_DELETED == status;
}

ObBackupPieceInfo::ObBackupPieceInfo()
  : key_(),
    create_date_(0),
    start_ts_(0),
    checkpoint_ts_(0),
    max_ts_(0),
    status_(ObBackupPieceStatus::BACKUP_PIECE_MAX),
    file_status_(ObBackupFileStatus::BACKUP_FILE_AVAILABLE),
    backup_dest_(),
    compatible_(ObTenantLogArchiveStatus::MAX),
    start_piece_id_(0)
{
}

OB_SERIALIZE_MEMBER(ObBackupPieceInfo, key_.incarnation_, key_.tenant_id_, key_.round_id_,
    key_.backup_piece_id_, key_.copy_id_, create_date_, start_ts_, checkpoint_ts_, max_ts_, status_,
    file_status_, backup_dest_, compatible_, start_piece_id_);

bool ObBackupPieceInfo::is_valid() const
{
  return key_.is_valid()
      && create_date_ >= 0
      && status_ >= 0
      && status_ < ObBackupPieceStatus::BACKUP_PIECE_MAX
      && file_status_ >= 0
      && file_status_ < ObBackupFileStatus::BACKUP_FILE_MAX
      && start_ts_ >= 0
      && checkpoint_ts_ >= 0
      && max_ts_ >= 0
      && backup_dest_.size() > 0
      && (compatible_ >= ObTenantLogArchiveStatus::NONE && compatible_ < ObTenantLogArchiveStatus::MAX)
      && start_piece_id_ >= 0;
}

void ObBackupPieceInfo::reset()
{
  key_.reset();
  create_date_ = 0;
  status_ = ObBackupPieceStatus::BACKUP_PIECE_MAX;
  file_status_ = ObBackupFileStatus::BACKUP_FILE_AVAILABLE;
  start_ts_ = 0;
  checkpoint_ts_ = 0;
  max_ts_ = 0;
  backup_dest_.reset();
  compatible_ = ObTenantLogArchiveStatus::MAX;
  start_piece_id_ = 0;
}

bool ObBackupPieceInfo::operator==(const ObBackupPieceInfo &o) const
{
  return key_ == o.key_
      && create_date_ == o.create_date_
      && start_ts_ == o.start_ts_
      && checkpoint_ts_ == o.checkpoint_ts_
      && max_ts_ == o.max_ts_
      && status_ == o.status_
      && file_status_ == o.file_status_
      && backup_dest_ == o.backup_dest_
      && compatible_ == o.compatible_
      && start_piece_id_ == o.start_piece_id_;
}

int ObBackupPieceInfo::init_piece_info(
    const ObBackupPieceInfo &sys_piece,
    const uint64_t tenant_id)
{
  int ret = OB_SUCCESS;
  reset();

  if (!sys_piece.is_valid() || OB_INVALID_ID == tenant_id
      || OB_SYS_TENANT_ID != sys_piece.key_.tenant_id_) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid args", K(ret), K(sys_piece), K(tenant_id));
  } else if (OB_FAIL(backup_dest_.assign(sys_piece.backup_dest_))) {
    LOG_WARN("failed to copy backup dest", K(ret), K(sys_piece));
  } else {
    // key
    key_ = sys_piece.key_;
    key_.tenant_id_ = tenant_id;
    create_date_ = sys_piece.create_date_;
    // value
    start_ts_ = sys_piece.start_ts_;
    checkpoint_ts_ = sys_piece.checkpoint_ts_;
    max_ts_ = sys_piece.max_ts_;
    status_ = sys_piece.status_;
    file_status_ = sys_piece.file_status_;
    compatible_ = sys_piece.compatible_;
    start_piece_id_ = sys_piece.start_piece_id_;
  }
  return ret;
}

int ObBackupPieceInfo::get_backup_dest(ObBackupDest &backup_dest) const
{
  int ret = OB_SUCCESS;
  backup_dest.reset();
  if (backup_dest_.is_empty()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", KR(ret));
  } else if (OB_FAIL(backup_dest.set(backup_dest_.ptr()))) {
    LOG_WARN("failed to set backup dest", KR(ret), K(backup_dest_));
  }
  return ret;
}

ObNonFrozenBackupPieceInfo::ObNonFrozenBackupPieceInfo()
  : has_prev_piece_info_(false),
    prev_piece_info_(),
    cur_piece_info_()
{
}

void ObNonFrozenBackupPieceInfo::reset()
{
  has_prev_piece_info_ = false;
  prev_piece_info_.reset();
  cur_piece_info_.reset();
}


bool ObNonFrozenBackupPieceInfo::is_valid() const
{
  bool is_valid = true;

  if (!cur_piece_info_.is_valid()) {
    is_valid = false;
    LOG_WARN_RET(OB_INVALID_ERROR, "cur piece info is invalid", K(*this));
  }

  if (has_prev_piece_info_ && is_valid) {
    if (!prev_piece_info_.is_valid()) {
      is_valid = false;
      LOG_WARN_RET(OB_INVALID_ERROR, "prev piece info is invalid", K(*this));
    } else if (ObBackupPieceStatus::BACKUP_PIECE_ACTIVE == prev_piece_info_.status_) {
      is_valid = false;
      LOG_WARN_RET(OB_INVALID_ERROR, "previous piece info is active", K(*this));
    }
  }

  return is_valid;
}

int ObNonFrozenBackupPieceInfo::get_backup_piece_id(int64_t &active_piece_id) const
{
  int ret = OB_SUCCESS;
  active_piece_id = -1;

  if (!is_valid()) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("invalid piece info", K(ret), K(*this));
  } else {
    active_piece_id = cur_piece_info_.key_.backup_piece_id_;
  }

  return ret;
}

int ObNonFrozenBackupPieceInfo::get_backup_piece_info(int64_t &active_piece_id,
                                                      int64_t &active_piece_create_date) const
{
  int ret = OB_SUCCESS;
  active_piece_id = -1;
  active_piece_create_date = OB_INVALID_TIMESTAMP;

  if (!is_valid()) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("invalid piece info", K(ret), K(*this));
  } else {
    active_piece_id = cur_piece_info_.key_.backup_piece_id_;
    active_piece_create_date = cur_piece_info_.create_date_;
  }

  return ret;
}

int64_t ObNonFrozenBackupPieceInfo::to_string(char *buf, const int64_t buf_len) const
{
  int64_t pos = 0;

  if (OB_ISNULL(buf) || buf_len <= 0) {
      // do nothing
  } else {
    J_OBJ_START();
    J_KV(K_(has_prev_piece_info), K_(cur_piece_info));
    if (has_prev_piece_info_) {
      J_COMMA();
      J_KV(K_(prev_piece_info));
    }

    J_OBJ_END();
  }
  return pos;
}

const char *backup_set_file_info_status_strs[] = {
    "DOING",
    "SUCCESS",
    "FAILED",
};


ObBackupRegion::ObBackupRegion()
  : region_(),
    priority_(-1)
{
}

ObBackupRegion::~ObBackupRegion()
{
}

void ObBackupRegion::reset()
{
  region_.reset();
  priority_ = -1;
}

int ObBackupRegion::set(const ObString &region, const int64_t priority)
{
  int ret = OB_SUCCESS;
  if (0 == region.length() || priority < 0) {
    ret = OB_INVALID_ARGUMENT;
    OB_LOG(WARN, "set backup region get invalid argument", K(ret), K(region), K(priority));
  } else if (OB_FAIL(region_.assign(region))) {
    OB_LOG(WARN, "failed to assign region", K(ret), K(region));
  } else {
    priority_ = priority;
  }
  return ret;
}

ObBackupZone::ObBackupZone()
  : zone_(),
    priority_(-1)
{
}

ObBackupZone::~ObBackupZone()
{
}

void ObBackupZone::reset()
{
  zone_.reset();
  priority_ = -1;
}

int ObBackupZone::set(const ObString &zone, const int64_t priority)
{
  int ret = OB_SUCCESS;
  if (0 == zone.length() || priority < 0) {
    ret = OB_INVALID_ARGUMENT;
    OB_LOG(WARN, "set backup zone get invalid argument", K(ret), K(zone), K(priority));
  } else if (OB_FAIL(zone_.assign(zone))) {
    OB_LOG(WARN, "failed to assign zone", K(ret), K(zone));
  } else {
    priority_ = priority;
  }
  return ret;
}

ObBackupServer::ObBackupServer()
  : server_(),
    priority_(-1)
{
}

ObBackupServer::~ObBackupServer()
{
}

void ObBackupServer::reset()
{
  server_.reset();
  priority_ = -1;
}

int ObBackupServer::set(const ObAddr &server, const int64_t priority)
{
  int ret = OB_SUCCESS;
  if (!server.is_valid() || priority < 0) {
    ret = OB_INVALID_ARGUMENT;
    OB_LOG(WARN, "set backup server get invalid argument", K(ret), K(server), K(priority));
  } else {
    server_ = server;
    priority_ = priority;
  }
  return ret;
}

int ObBackupServer::assign(const ObBackupServer &that)
{
  int ret = OB_SUCCESS;
  if (!that.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    OB_LOG(WARN, "set backup server get invalid argument", K(ret), K(that));
  } else {
    server_ = that.server_;
    priority_ = that.priority_;
  }
  return ret;
}

// 4.0 backup

OB_SERIALIZE_MEMBER(ObBackupStatus, status_);

bool ObBackupStatus::is_valid() const
{
  return status_ >= INIT && status_ < MAX_STATUS;
}

ObBackupStatus &ObBackupStatus::operator=(const Status &status)
{
  status_ = status;
  return *this;
}

const char* ObBackupStatus::get_str() const
{
  const char *str = "UNKNOWN";
  const char *status_strs[] = {
    "INIT",
    "DOING",
    "COMPLETED",
    "FAILED",
    "CANCELING",
    "CANCELED",
    "BACKUP_SYS_META",
    "BACKUP_USER_META",
    "BACKUP_DATA_SYS",
    "BACKUP_DATA_MINOR",
    "BACKUP_DATA_MAJOR",
    "BACKUP_LOG",
  };

  STATIC_ASSERT(MAX_STATUS == ARRAYSIZEOF(status_strs), "status count mismatch");
  if (status_ < INIT || status_ >= MAX_STATUS) {
    LOG_ERROR_RET(OB_ERR_UNEXPECTED, "invalid backup data job status", K(status_));
  } else {
    str = status_strs[status_];
  }
  return str;
}

int ObBackupStatus::set_status(const char *str)
{
  int ret = OB_SUCCESS;
  ObString s(str);
  const char *status_strs[] = {
    "INIT",
    "DOING",
    "COMPLETED",
    "FAILED",
    "CANCELING",
    "CANCELED",
    "BACKUP_SYS_META",
    "BACKUP_USER_META",
    "BACKUP_DATA_SYS",
    "BACKUP_DATA_MINOR",
    "BACKUP_DATA_MAJOR",
    "BACKUP_LOG",
  };
  const int64_t count = ARRAYSIZEOF(status_strs);
  if (s.empty()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("status can't empty", K(ret));
  } else {
    for (int64_t i = 0; i < count; ++i) {
      if (0 == s.case_compare(status_strs[i])) {
        status_ = static_cast<Status>(i);
      }
    }
  }
  return ret;
}

int ObBackupStatus::get_backup_data_type(share::ObBackupDataType &backup_data_type) const
{
  int ret = OB_SUCCESS;
  if (BACKUP_DATA_SYS == status_) {
    backup_data_type.set_sys_data_backup();
  } else if (BACKUP_DATA_MINOR == status_) {
    backup_data_type.set_minor_data_backup();
  } else if (BACKUP_DATA_MAJOR == status_) {
    backup_data_type.set_major_data_backup();
  } else {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("status not expected", K(ret), K_(status));
  }
  return ret;
}

OB_SERIALIZE_MEMBER(ObBackupDataType, type_);

bool ObBackupTaskStatus::is_valid() const
{
  return status_ >= Status::INIT && status_ < Status::MAX_STATUS;
}

const char* ObBackupTaskStatus::get_str() const
{
  const char *str = "UNKNOWN";
  const char *status_strs[] = {
    "INIT",
    "PENDING",
    "DOING",
    "FINISH",
  };
  STATIC_ASSERT(MAX_STATUS == ARRAYSIZEOF(status_strs), "status count mismatch");
  if (status_ < INIT || status_ >= MAX_STATUS) {
    LOG_ERROR_RET(OB_ERR_UNEXPECTED, "invalid backup data task status", K(status_));
  } else {
    str = status_strs[status_];
  }
  return str;
}

int ObBackupTaskStatus::set_status(const char *str)
{
  int ret = OB_SUCCESS;
  ObString s(str);
  const char *status_strs[] = {
    "INIT",
    "PENDING",
    "DOING",
    "FINISH",
  };
  const int64_t count = ARRAYSIZEOF(status_strs);
  if (s.empty()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("status can't empty", K(ret));
  } else {
    for (int64_t i = 0; i < count; ++i) {
      if (0 == s.case_compare(status_strs[i])) {
        status_ = static_cast<Status>(i);
      }
    }
  }
  return ret;
}

OB_SERIALIZE_MEMBER(ObBackupStats, input_bytes_, output_bytes_, tablet_count_, finish_tablet_count_,
    macro_block_count_, finish_macro_block_count_, extra_bytes_, finish_file_count_);

ObBackupStats::ObBackupStats()
  : input_bytes_(0),
    output_bytes_(0),
    tablet_count_(0),
    finish_tablet_count_(0),
    macro_block_count_(0),
    finish_macro_block_count_(0),
    extra_bytes_(0),
    finish_file_count_(0)
{
}

bool ObBackupStats::is_valid() const
{
  return input_bytes_ >= 0
      && output_bytes_ >= 0
      && tablet_count_ >= 0
      && finish_tablet_count_ >= 0
      && macro_block_count_ >= 0
      && finish_macro_block_count_ >= 0
      && extra_bytes_ >= 0
      && finish_file_count_ >= 0;
}

void ObBackupStats::assign(const ObBackupStats &other)
{
  input_bytes_ = other.input_bytes_;
  output_bytes_ = other.output_bytes_;
  tablet_count_ = other.tablet_count_;
  finish_tablet_count_ = other.finish_tablet_count_;
  macro_block_count_ = other.macro_block_count_;
  finish_macro_block_count_ = other.finish_macro_block_count_;
  extra_bytes_ = other.extra_bytes_;
  finish_file_count_ = other.finish_file_count_;
}

void ObBackupStats::cum_with(const ObBackupStats &other)
{
  input_bytes_ += other.input_bytes_;
  output_bytes_ += other.output_bytes_;
  tablet_count_ += other.tablet_count_;
  finish_tablet_count_ += other.finish_tablet_count_;
  macro_block_count_ += other.macro_block_count_;
  finish_macro_block_count_ += other.finish_macro_block_count_;
  extra_bytes_ += other.extra_bytes_;
  finish_file_count_ += other.finish_file_count_;
}

void ObBackupStats::reset()
{
  input_bytes_ = 0;
  output_bytes_ = 0;
  tablet_count_ = 0;
  finish_tablet_count_ = 0;
  macro_block_count_ = 0;
  finish_macro_block_count_ = 0;
  extra_bytes_ = 0;
  finish_file_count_ = 0;
}

ObBackupJobAttr::ObBackupJobAttr()
  : job_id_(0),
    tenant_id_(0),
    incarnation_id_(0),
    backup_set_id_(0),
    initiator_tenant_id_(OB_INVALID_TENANT_ID),
    initiator_job_id_(0),
    executor_tenant_id_(),
    plus_archivelog_(false),
    backup_level_(),
    backup_type_(),
    encryption_mode_(ObBackupEncryptionMode::EncryptionMode::NONE),
    passwd_(),
    backup_path_(),
    description_(),
    start_ts_(0),
    end_ts_(0),
    status_(),
    result_(0),
    can_retry_(true),
    retry_count_(0)
{
}

int ObBackupJobAttr::assign(const ObBackupJobAttr &other)
{
  int ret = OB_SUCCESS;
  if (!other.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", K(ret), K(other));
  } else if (OB_FAIL(backup_path_.assign(other.backup_path_))) {
    LOG_WARN("failed to assign backup path", K(ret), K(other.backup_path_));
  } else if (OB_FAIL(description_.assign(other.description_))) {
    LOG_WARN("failed to assign description", K(ret), K(other.description_));
  } else if (OB_FAIL(passwd_.assign(other.passwd_))) {
    LOG_WARN("failed to assign passwd", K(ret), K(other.passwd_));
  } else if (OB_FAIL(executor_tenant_id_.assign(other.executor_tenant_id_))) {
    LOG_WARN("failed to assign backup tenant id", K(ret), K(other.executor_tenant_id_));
  } else {
    job_id_ = other.job_id_;
    tenant_id_ = other.tenant_id_;
    incarnation_id_ = other.incarnation_id_;
    backup_set_id_ = other.backup_set_id_;
    initiator_tenant_id_ = other.initiator_tenant_id_;
    initiator_job_id_ = other.initiator_job_id_;
    plus_archivelog_ = other.plus_archivelog_;
    backup_level_.level_ = other.backup_level_.level_;
    backup_type_.type_ = other.backup_type_.type_;
    start_ts_ = other.start_ts_;
    end_ts_ = other.end_ts_;
    status_.status_ = other.status_.status_;
    result_ = other.result_;
    encryption_mode_ = other.encryption_mode_;
    can_retry_ = other.can_retry_;
    retry_count_ = other.retry_count_;
  }
  return ret;
}

int ObBackupJobAttr::set_plus_archivelog(const char *str)
{
  int ret = OB_SUCCESS;
  if (nullptr == str) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invlaid argument", K(ret));
  } else {
    ObString s(str);
    if (0 == s.case_compare("ON")) {
      plus_archivelog_ = true;
    } else if (0 == s.case_compare("OFF")) {
      plus_archivelog_ = false;
    } else {
      ret = OB_INVALID_ARGUMENT;
      LOG_WARN("invlaid argument", K(ret));
    }
  }
  return ret;
}

const char *ObBackupJobAttr::get_plus_archivelog_str() const
{
  const char *str = nullptr;
  if (plus_archivelog_) {
    str = "ON";
  } else {
    str = "OFF";
  }
  return str;
}

int ObBackupJobAttr::set_executor_tenant_id(const ObString &str)
{
  int ret = OB_SUCCESS;
  char tmp_str[OB_INNER_TABLE_DEFAULT_VALUE_LENTH] = { 0 };
  if (str.empty()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invlaid argument", K(ret));
  } else if (OB_FAIL(databuff_printf(tmp_str, sizeof(tmp_str), "%s", str.ptr()))) {
    LOG_WARN("failed to set dir name", K(ret), K(str));
  } else {
    char *token = nullptr;
    char *saveptr = nullptr;
    char *p_end = nullptr;
    token = ::STRTOK_R(tmp_str, ",", &saveptr);
    while (OB_SUCC(ret) && nullptr != token) {
      uint64_t tenant_id;
      if (OB_FAIL(ob_strtoull(token, p_end, tenant_id))) {
        LOG_WARN("failed to get value from string", K(ret), K(*token));
      } else if ('\0' == *p_end) {
        if (OB_FAIL(executor_tenant_id_.push_back(tenant_id))) {
          LOG_WARN("fail to push back tenant id", K(ret), K(tenant_id));
        }
      } else {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("set tenant id error", K(*token));
      }
      token = STRTOK_R(nullptr, ",", &saveptr);
      p_end = nullptr;
    }
  }
  return ret;
}

int ObBackupJobAttr::get_executor_tenant_id_str(share::ObDMLSqlSplicer &dml) const
{
  int ret = OB_SUCCESS;
  char tmp_path[OB_INNER_TABLE_DEFAULT_VALUE_LENTH] = { 0 };
  MEMSET(tmp_path, '\0', sizeof(tmp_path));
  int64_t cur_pos = 0;
  for (int i = 0; OB_SUCC(ret) && i < executor_tenant_id_.count(); ++i) {
    const uint64_t tenant_id = executor_tenant_id_.at(i);
    if (0 == i) {
      if (OB_FAIL(databuff_printf(tmp_path, sizeof(tmp_path), cur_pos, "%lu", tenant_id))) {
        LOG_WARN("fail to databuff printf tenant id", K(ret));
      }
    } else {
      if (OB_FAIL(databuff_printf(tmp_path, sizeof(tmp_path), cur_pos, ",%lu", tenant_id))) {
        LOG_WARN("fail to databuff printf tenant id", K(ret));
      }
    }
  }
  if (OB_FAIL(ret)) {
  } else if (OB_FAIL(dml.add_column(OB_STR_EXECUTOR_TENANT_ID, tmp_path))) {
    LOG_WARN("fail to add column", K(ret));
  }
  return ret;
}

bool ObBackupJobAttr::is_tmplate_valid() const
{
  return tenant_id_ != OB_INVALID_TENANT_ID && initiator_tenant_id_ != OB_INVALID_TENANT_ID;
}

bool ObBackupJobAttr::is_valid() const
{
  return tenant_id_ != OB_INVALID_TENANT_ID
      && initiator_tenant_id_ != OB_INVALID_TENANT_ID
      && backup_type_.is_valid()
      && ObBackupEncryptionMode::is_valid(encryption_mode_)
      && incarnation_id_ > 0
      && start_ts_ > 0
      && status_.is_valid();
}


bool ObBackupDataTaskType::is_valid() const
{
  return type_ >= Type::BACKUP_DATA_SYS && type_ < Type::BACKUP_MAX;
}

const char* ObBackupDataTaskType::get_str() const
{
  const char *str = "UNKNOWN";
  const char *type_strs[] = {
    "BACKUP_DATA_SYS",
    "BACKUP_DATA_MINOR",
    "BACKUP_DATA_MAJOR",
    "PLUS_ARCHIVE_LOG",
    "BUILD_INDEX",
    "BACKUP_META"
  };
  if (type_ < Type::BACKUP_DATA_SYS || type_ >= Type::BACKUP_MAX) {
    LOG_ERROR_RET(OB_ERR_UNEXPECTED, "invalid compressor type", K(type_));
  } else {
    str = type_strs[type_];
  }
  return str;
}

int ObBackupDataTaskType::set_type(const char *buf)
{
    int ret = OB_SUCCESS;
  ObString s(buf);
  const char *type_strs[] = {
    "BACKUP_DATA_SYS",
    "BACKUP_DATA_MINOR",
    "BACKUP_DATA_MAJOR",
    "PLUS_ARCHIVE_LOG",
    "BUILD_INDEX",
    "BACKUP_META"
  };
  const int64_t count = ARRAYSIZEOF(type_strs);
  if (s.empty()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("status can't empty", K(ret));
  } else {
    for (int64_t i = 0; i < count; ++i) {
      if (0 == s.case_compare(type_strs[i])) {
        type_ = static_cast<Type>(i);
      }
    }
  }
  return ret;
}

int ObBackupDataTaskType::get_backup_data_type(share::ObBackupDataType &backup_data_type) const
{
  int ret = OB_SUCCESS;
  if (!is_backup_data()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("not suitable backup type", K(ret));
  } else if (BACKUP_DATA_SYS == type_) {
    backup_data_type.set_sys_data_backup();
  } else if (BACKUP_DATA_MINOR == type_) {
    backup_data_type.set_minor_data_backup();
  } else {
    backup_data_type.set_major_data_backup();
  }
  return ret;
}

ObBackupSetTaskAttr::ObBackupSetTaskAttr()
  : task_id_(0),
    tenant_id_(0),
    incarnation_id_(0),
    job_id_(0),
    backup_set_id_(0),
    start_ts_(0),
    end_ts_(0),
    start_scn_(),
    end_scn_(),
    user_ls_start_scn_(),
    data_turn_id_(0),
    meta_turn_id_(0),
    status_(),
    encryption_mode_(ObBackupEncryptionMode::EncryptionMode::NONE),
    passwd_(),
    stats_(),
    backup_path_(0),
    retry_cnt_(0),
    result_(0)
{
}

bool ObBackupSetTaskAttr::is_valid() const
{
  return tenant_id_ != OB_INVALID_TENANT_ID
    && task_id_ > 0
    && ObBackupEncryptionMode::is_valid(encryption_mode_)
    && job_id_ > 0
    && backup_set_id_ > 0
    && !backup_path_.is_empty()
    && status_.is_valid();
}

int ObBackupSetTaskAttr::assign(const ObBackupSetTaskAttr &other)
{
  int ret = OB_SUCCESS;
  if (!other.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", K(ret), K(other));
  } else if (OB_FAIL(passwd_.assign(other.passwd_))) {
    LOG_WARN("failed to assign passwd", K(ret));
  } else if (OB_FAIL(backup_path_.assign(other.backup_path_.ptr()))) {
    LOG_WARN("failed to assign backup path", K(ret), K(other.backup_path_));
  } else {
    stats_.assign(other.stats_);
    incarnation_id_ = other.incarnation_id_;
    task_id_ = other.task_id_;
    tenant_id_ = other.tenant_id_;
    job_id_ = other.job_id_;
    backup_set_id_ = other.backup_set_id_;
    status_ = other.status_;
    start_scn_ = other.start_scn_;
    end_scn_ = other.end_scn_;
    encryption_mode_ = other.encryption_mode_;
    result_ = other.result_;
    data_turn_id_ = other.data_turn_id_;
    meta_turn_id_ = other.meta_turn_id_;
    user_ls_start_scn_ = other.user_ls_start_scn_;
    retry_cnt_ = other.retry_cnt_;
  }
  return ret;
}

ObBackupLSTaskAttr::ObBackupLSTaskAttr()
  : task_id_(0),
    tenant_id_(0),
    ls_id_(0),
    job_id_(0),
    backup_set_id_(0),
    backup_type_(),
    task_type_(),
    status_(),
    start_ts_(0),
    end_ts_(0),
    backup_date_(0),
    black_servers_(),
    dst_(),
    task_trace_id_(),
    stats_(),
    start_turn_id_(0),
    turn_id_(0),
    retry_id_(0),
    result_(0)
{
}

bool ObBackupLSTaskAttr::is_valid() const
{
  return task_id_ > 0
      && tenant_id_ != OB_INVALID_TENANT_ID
      && job_id_ > 0
      && backup_set_id_ > 0
      && ls_id_.id() >= 0
      && backup_type_.is_valid()
      && status_.is_valid()
      && start_ts_ > 0
      && turn_id_ > 0;
}

int ObBackupLSTaskAttr::assign(const ObBackupLSTaskAttr &other)
{
  int ret = OB_SUCCESS;
  if (!other.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", K(ret), K(other));
  } else if (OB_FAIL(append(black_servers_, other.black_servers_))) {
    LOG_WARN("failed to append black servers", K(ret));
  } else {
    stats_.assign(other.stats_);
    task_id_ = other.task_id_;
    tenant_id_ = other.tenant_id_;
    ls_id_ = other.ls_id_;
    job_id_ = other.job_id_;
    backup_set_id_ = other.backup_set_id_;
    backup_type_.type_ = other.backup_type_.type_;
    task_type_.type_ = other.task_type_.type_;
    status_.status_ = other.status_.status_;
    start_ts_ = other.start_ts_;
    end_ts_ = other.end_ts_;
    backup_date_ = other.backup_date_;
    dst_ = other.dst_;
    task_trace_id_ = other.task_trace_id_;
    start_turn_id_ = other.start_turn_id_;
    turn_id_ = other.turn_id_;
    retry_id_ = other.retry_id_;
    result_ = other.result_;
  }
  return ret;
}

OB_SERIALIZE_MEMBER(ObBackupSetFileDesc, backup_set_id_, incarnation_, tenant_id_, dest_id_, backup_type_,
    plus_archivelog_, date_, prev_full_backup_set_id_, prev_inc_backup_set_id_, stats_, start_time_, end_time_, status_,
    result_, encryption_mode_, passwd_, file_status_, backup_path_, start_replay_scn_, min_restore_scn_,
    tenant_compatible_, backup_compatible_, data_turn_id_, meta_turn_id_, cluster_version_);

ObBackupSetFileDesc::ObBackupSetFileDesc()
  : backup_set_id_(0),
    incarnation_(0),
    tenant_id_(OB_INVALID_ID),
    dest_id_(-1),
    backup_type_(),
    plus_archivelog_(false),
    date_(0),
    prev_full_backup_set_id_(0),
    prev_inc_backup_set_id_(0),
    stats_(),
    start_time_(0),
    end_time_(0),
    status_(BackupSetStatus::MAX),
    result_(0),
    encryption_mode_(ObBackupEncryptionMode::MAX_MODE),
    passwd_(),
    file_status_(ObBackupFileStatus::BACKUP_FILE_MAX),
    backup_path_(),
    start_replay_scn_(),
    min_restore_scn_(),
    tenant_compatible_(0),
    backup_compatible_(Compatible::MAX_COMPATIBLE_VERSION),
    data_turn_id_(0),
    meta_turn_id_(0),
    cluster_version_(0)
{
}

void ObBackupSetFileDesc::reset()
{
  backup_set_id_ = 0;
  incarnation_ = 0;
  tenant_id_ = OB_INVALID_ID;
  dest_id_ = -1;
  backup_type_.reset();
  date_ = 0;
  plus_archivelog_ = false;
  prev_full_backup_set_id_ = 0;
  prev_inc_backup_set_id_ = 0;
  stats_.reset();
  start_time_ = 0;
  end_time_ = 0;
  status_ = BackupSetStatus::MAX;
  result_ = 0;
  encryption_mode_ = share::ObBackupEncryptionMode::MAX_MODE;
  passwd_.reset();
  file_status_ = ObBackupFileStatus::BACKUP_FILE_MAX;
  backup_path_.reset();
  start_replay_scn_ = SCN::min_scn();
  min_restore_scn_ = SCN::min_scn();
  tenant_compatible_ = 0;
  backup_compatible_ = Compatible::MAX_COMPATIBLE_VERSION;
  data_turn_id_ = 0;
  meta_turn_id_ = 0;
  cluster_version_ = 0;
}


bool ObBackupSetFileDesc::is_key_valid() const
{
  return tenant_id_ != OB_INVALID_ID
      && backup_set_id_ > 0 && incarnation_ > 0 && dest_id_ >= 0;
}

bool ObBackupSetFileDesc::is_valid() const
{
  return is_key_valid() && BackupSetStatus::MAX != status_
      && share::ObBackupEncryptionMode::is_valid(encryption_mode_)
      && ObBackupFileStatus::is_valid(file_status_);
}

bool ObBackupSetFileDesc::is_same_task(const ObBackupSetFileDesc &other) const
{
  return tenant_id_ == other.tenant_id_ && backup_set_id_ == other.backup_set_id_ &&
      incarnation_ == other.incarnation_ && dest_id_ == other.dest_id_;
}

const char *ObBackupSetFileDesc::get_backup_set_status_str() const
{
  const char *str = "UNKNOWN";

  STATIC_ASSERT(MAX == ARRAYSIZEOF(backup_set_file_info_status_strs), "status count mismatch");
  if (status_ < 0 || status_ >= MAX) {
    LOG_WARN_RET(OB_INVALID_ARGUMENT, "invalid backup set status", K(status_));
  } else {
    str = backup_set_file_info_status_strs[status_];
  }
  return str;
}

int ObBackupSetFileDesc::set_backup_set_status(
    const char *buf)
{
  int ret = OB_SUCCESS;
  if (OB_ISNULL(buf)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("set backup task status get invalid argument", K(ret), KP(buf));
  } else {
    BackupSetStatus tmp_status = MAX;
    STATIC_ASSERT(MAX == ARRAYSIZEOF(backup_set_file_info_status_strs), "status count mismatch");
    for (int64_t i = 0; i < ARRAYSIZEOF(backup_set_file_info_status_strs); i++) {
      if (0 == STRCMP(backup_set_file_info_status_strs[i], buf)) {
        tmp_status = static_cast<BackupSetStatus>(i);
      }
    }

    if (MAX == tmp_status) {
      ret = OB_ENTRY_NOT_EXIST;
      LOG_WARN("backup status str not found", K(ret), K(buf));
    } else {
      status_ = tmp_status;
    }
  }
  return ret;
}

int ObBackupSetFileDesc::set_plus_archivelog(const char *str)
{
  int ret = OB_SUCCESS;
  if (nullptr == str) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invlaid argument", K(ret));
  } else {
    ObString s(str);
    if (0 == s.case_compare("ON")) {
      plus_archivelog_ = true;
    } else if (0 == s.case_compare("OFF")) {
      plus_archivelog_ = false;
    } else {
      ret = OB_INVALID_ARGUMENT;
      LOG_WARN("invlaid argument", K(ret));
    }
  }
  return ret;
}

const char *ObBackupSetFileDesc::get_plus_archivelog_str() const
{
  const char *str = nullptr;
  if (plus_archivelog_) {
    str = "ON";
  } else {
    str = "OFF";
  }
  return str;
}


int ObBackupSetFileDesc::check_passwd(const char *passwd_array) const
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(ObBackupUtils::check_passwd(passwd_array, passwd_.ptr()))) {
    LOG_WARN("failed to check passwd", KR(ret), K(passwd_array), K(passwd_));
  }
  return ret;
}

int ObBackupSetFileDesc::assign(const ObBackupSetFileDesc &other)
{
  int ret = OB_SUCCESS;
  if (!other.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", K(ret), K(other));
  } else if (OB_FAIL(backup_path_.assign(other.backup_path_))) {
    LOG_WARN("failed to assign backup path", KR(ret), K(other));
  } else if (OB_FAIL(passwd_.assign(other.passwd_))) {
    LOG_WARN("failed to assign passwd", KR(ret), K(other));
  } else {
    backup_set_id_ = other.backup_set_id_;
    incarnation_ = other.incarnation_;
    tenant_id_ = other.tenant_id_;
    dest_id_ = other.dest_id_;
    plus_archivelog_ = other.plus_archivelog_;
    backup_type_.type_ = other.backup_type_.type_;
    date_ = other.date_;
    prev_full_backup_set_id_ = other.prev_full_backup_set_id_;
    prev_inc_backup_set_id_ = other.prev_inc_backup_set_id_;
    stats_.assign(other.stats_);
    start_time_ = other.start_time_;
    end_time_ = other.end_time_;
    status_ = other.status_;
    result_ = other.result_;
    encryption_mode_ = other.encryption_mode_;
    file_status_ = other.file_status_;
    start_replay_scn_ = other.start_replay_scn_;
    min_restore_scn_ = other.min_restore_scn_;
    tenant_compatible_ = other.tenant_compatible_;
    backup_compatible_ = other.backup_compatible_;
    data_turn_id_ = other.data_turn_id_;
    meta_turn_id_ = other.meta_turn_id_;
    cluster_version_ = other.cluster_version_;
  }
  return ret;
}

ObBackupSkipTabletAttr::ObBackupSkipTabletAttr()
  : task_id_(0),
    tenant_id_(0),
    turn_id_(0),
    retry_id_(0),
    backup_set_id_(0),
    tablet_id_(0),
    ls_id_(),
    skipped_type_()
{
}

bool ObBackupSkipTabletAttr::is_valid() const
{
  return task_id_ > 0
      && tenant_id_ > 0
      && turn_id_ > 0
      && retry_id_ > 0
      && tablet_id_.is_valid()
      && backup_set_id_ > 0
      && ls_id_.is_valid()
      && skipped_type_.is_valid();
}

ObBackupLSTaskInfoAttr::ObBackupLSTaskInfoAttr()
  : task_id_(),
    tenant_id_(),
    ls_id_(),
    turn_id_(),
    retry_id_(),
    backup_data_type_(),
    backup_set_id_(),
    input_bytes_(),
    output_bytes_(),
    tablet_count_(),
    finish_tablet_count_(),
    macro_block_count_(),
    finish_macro_block_count_(),
    extra_bytes_(),
    file_count_(),
    max_file_id_(),
    is_final_()
{}

bool ObBackupLSTaskInfoAttr::is_valid() const
{
  return task_id_ > 0 && OB_INVALID_TENANT_ID != tenant_id_ && ls_id_.is_valid() && turn_id_ > 0 && retry_id_ >= 0;
}

bool ObBackupLevel::is_valid() const
{
  return level_ >= Level::CLUSTER && level_ < Level::MAX_LEVEL;
}

const char *ObBackupLevel::get_str() const
{
  const char *str = "UNKNOWN";
  const char *level_strs[] = {
    "CLUSTER",
    "SYS_TENANT",
    "USER_TENANT"
  };
  if (level_ < Level::CLUSTER || level_ >= Level::MAX_LEVEL) {
    LOG_ERROR_RET(OB_ERR_UNEXPECTED, "invalid backup level", K(level_));
  } else {
    str = level_strs[level_];
  }
  return str;
}

int ObBackupLevel::set_level(const char *str)
{
  int ret = OB_SUCCESS;
  ObString s(str);
  const char *level_strs[] = {
    "CLUSTER",
    "SYS_TENANT",
    "USER_TENANT"
  };
  STATIC_ASSERT(Level::MAX_LEVEL == ARRAYSIZEOF(level_strs), "level count mismatch");
  const int64_t count = ARRAYSIZEOF(level_strs);
  if (s.empty()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("level_strs can't empty", K(ret));
  } else {
    for (int64_t i = 0; i < count; ++i) {
      if (0 == s.case_compare(level_strs[i])) {
        level_ = static_cast<Level>(i);
      }
    }
  }
  return ret;
}

OB_SERIALIZE_MEMBER(ObLogArchiveDestState, state_);

bool ObLogArchiveDestState::is_valid() const
{
  return state_ >= State::ENABLE && state_ < State::MAX;
}

int ObLogArchiveDestState::set_state(const char *buf)
{
  int ret = OB_SUCCESS;
  ObString s(buf);
  const char *str[] = {
    "ENABLE",
    "DEFER"
  };
  STATIC_ASSERT(State::MAX == ARRAYSIZEOF(str), "count mismatch");
  const int64_t count = ARRAYSIZEOF(str);
  if (s.empty()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("state can't empty", K(ret));
  } else {
    for (int64_t i = 0; i < count; ++i) {
      if (0 == s.case_compare(str[i])) {
        state_ = static_cast<State>(i);
        break;
      }
    }
  }
  return ret;
}

int ObLogArchiveDestState::assign(const ObLogArchiveDestState& that)
{
  int ret = OB_SUCCESS;
  if (that.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid arguement", K(ret), K(that));
  } else {
    state_ = that.state_;
  }
  return ret;
}

const char *ObLogArchiveDestState::get_str() const
{
  const char *str = "UNKNOWN";
  const char *state_strs[] = {
    "ENABLE",
    "DEFER"
  };
  if (!is_valid()) {
    LOG_ERROR_RET(OB_ERR_UNEXPECTED, "invalid state", K(state_));
  } else {
    str = state_strs[state_];
  }
  return str;
}

ObLogArchiveDestAtrr::ObLogArchiveDestAtrr()
  : dest_()
{
  // Set default value.
  binding_ = Binding::OPTIONAL;
  dest_id_ = 0;
  piece_switch_interval_ = OB_DEFAULT_PIECE_SWITCH_INTERVAL;
  lag_target_ = OB_DEFAULT_LAG_TARGET;
  state_.set_enable();
}

int ObLogArchiveDestAtrr::set_piece_switch_interval(const char *buf)
{
  int ret = OB_SUCCESS;
  bool is_valid = false;
  if (OB_ISNULL(buf)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid args", K(ret), KP(buf));
  } else if (OB_FALSE_IT(piece_switch_interval_ = ObConfigTimeParser::get(buf, is_valid))) {
  } else if (!is_valid || !is_piece_switch_interval_valid()) {
    ret = OB_INVALID_ARGUMENT;
    piece_switch_interval_ = 0;
    LOG_WARN("invalid piece_switch_interval str", K(ret), K(buf));
  }
  return ret;
}

int ObLogArchiveDestAtrr::set_lag_target(const char *buf)
{
  int ret = OB_SUCCESS;
  bool is_valid = false;
  if (OB_ISNULL(buf)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid args", K(ret), KP(buf));
  } else if (OB_FALSE_IT(lag_target_ = ObConfigTimeParser::get(buf, is_valid))) {
  } else if (!is_valid || !is_lag_target_valid()) {
    ret = OB_INVALID_ARGUMENT;
    lag_target_ = 0;
    LOG_WARN("invalid lag_target", K(ret), K(buf));
  }
  return ret;
}

int ObLogArchiveDestAtrr::set_log_archive_dest(const common::ObString &str)
{
  int ret = OB_SUCCESS;
  char *token = nullptr;
  char *saveptr = nullptr;
  if (str.empty()) {
    ret = OB_INVALID_ARGUMENT;
     LOG_WARN("invalid args", K(ret), K(str));
  } else if (OB_FAIL(dest_.set(str.ptr()))) {
    LOG_WARN("fail to set dest", K(ret));
  }
  return ret;
}

int ObLogArchiveDestAtrr::set_binding(const char *buf)
{
  int ret = OB_SUCCESS;
  ObString s(buf);
  if (s.empty()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("binding can't empty", K(ret));
  } else {
    int64_t i = 0;
    const int64_t size = ARRAYSIZEOF(STR_BINDING_TYPES);
    for (; i < size; ++i) {
      if (0 == s.case_compare(STR_BINDING_TYPES[i])) {
        binding_ = static_cast<Binding>(i);
        break;
      }
    }

    if (i == size) {
      ret = OB_INVALID_ARGUMENT;
      LOG_WARN("invalid binding type", K(ret), K(*buf));
    }
  }
  return ret;
}

bool ObLogArchiveDestAtrr::is_valid() const
{
  return is_dest_valid()
      && dest_id_ > 0
      && is_piece_switch_interval_valid()
      && is_lag_target_valid()
      && state_.is_valid();
};

bool ObLogArchiveDestAtrr::is_dest_valid() const
{
  return dest_.is_valid();
}

bool ObLogArchiveDestAtrr::is_piece_switch_interval_valid() const
{
  return piece_switch_interval_ >= OB_MIN_LOG_ARCHIVE_PIECE_SWITH_INTERVAL;
}

bool ObLogArchiveDestAtrr::is_lag_target_valid() const
{
  return lag_target_ >= OB_MIN_LAG_TARGET
      && lag_target_ <= OB_MAX_LAG_TARGET;
}

int ObLogArchiveDestAtrr::gen_config_items(common::ObIArray<BackupConfigItemPair> &items) const
{
  int ret = OB_SUCCESS;
  BackupConfigItemPair config;
  if (!is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid arguement", K(ret), KPC(this));
  }

  // gen path config
  ObBackupPathString tmp;
  if (OB_FAIL(ret)) {
  } else if (OB_FAIL(config.key_.assign(OB_STR_PATH))) {
    LOG_WARN("failed to assign key", K(ret));
  } else if (OB_FAIL(dest_.get_backup_dest_str(tmp.ptr(), tmp.capacity()))) {
    LOG_WARN("failed to get backup dest", K(ret));
  } else if (OB_FAIL(config.value_.assign(tmp.ptr()))) {
    LOG_WARN("failed to assign value", K(ret));
  } else if(OB_FAIL(items.push_back(config))) {
     LOG_WARN("failed to push backup config", K(ret));
  }

  // gen binding config
  if (OB_FAIL(ret)) {
  } else if (OB_FAIL(config.key_.assign(OB_STR_BINDING))) {
    LOG_WARN("failed to assign key", K(ret));
  } else if (OB_FAIL(get_binding(tmp.ptr(), tmp.capacity()))) {
    LOG_WARN("failed to get binding", K(ret));
  } else if (OB_FAIL(config.value_.assign(tmp.ptr()))) {
    LOG_WARN("failed to assign value", K(ret));
  } else if(OB_FAIL(items.push_back(config))) {
     LOG_WARN("failed to push backup config", K(ret));
  }

  // gen dest_id config
  if (OB_FAIL(ret)) {
  } else if (OB_FAIL(config.key_.assign(OB_STR_DEST_ID))) {
    LOG_WARN("failed to assign key", K(ret));
  } else if (OB_FAIL(config.set_value(dest_id_))) {
    LOG_WARN("failed to set dest_id", K(ret));
  } else if(OB_FAIL(items.push_back(config))) {
     LOG_WARN("failed to push backup config", K(ret));
  }

  // gen piece_switch_interval config
  if (OB_FAIL(ret)) {
  } else if (OB_FAIL(config.key_.assign(OB_STR_PIECE_SWITCH_INTERVAL))) {
    LOG_WARN("failed to assign key", K(ret));
  } else if (OB_FAIL(get_piece_switch_interval(tmp.ptr(), tmp.capacity()))) {
    LOG_WARN("failed to get piece switch interval", K(ret));
  } else if (OB_FAIL(config.value_.assign(tmp.ptr()))) {
    LOG_WARN("failed to assign value", K(ret));
  } else if(OB_FAIL(items.push_back(config))) {
     LOG_WARN("failed to push backup config", K(ret));
  }

  // gen lag_target config
  if (OB_FAIL(ret)) {
  } else if (OB_FAIL(config.key_.assign(OB_STR_LAG_TARGET))) {
    LOG_WARN("failed to assign key", K(ret));
  } else if (OB_FAIL(get_lag_target(tmp.ptr(), tmp.capacity()))) {
    LOG_WARN("failed to get lag target", K(ret));
  } else if (OB_FAIL(config.value_.assign(tmp.ptr()))) {
    LOG_WARN("failed to assign value", K(ret));
  } else if(OB_FAIL(items.push_back(config))) {
     LOG_WARN("failed to push backup config", K(ret));
  }

  // gen state config
  if (OB_FAIL(ret)) {
  } else if (OB_FAIL(config.key_.assign(OB_STR_STATE))) {
    LOG_WARN("failed to assign key", K(ret));
  } else if (OB_FAIL(config.value_.assign(state_.get_str()))) {
    LOG_WARN("failed to assign value", K(ret));
  } else if(OB_FAIL(items.push_back(config))) {
     LOG_WARN("failed to push backup config", K(ret));
  }

  return ret;
}

int ObLogArchiveDestAtrr::gen_path_config_items(common::ObIArray<BackupConfigItemPair> &items) const
{
  int ret = OB_SUCCESS;
  items.reset();
  BackupConfigItemPair config;
  if (!dest_.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid arguement", KR(ret), KPC(this));
  }

  // gen path config
  ObBackupPathString tmp;
  if (OB_FAIL(ret)) {
  } else if (OB_FAIL(config.key_.assign(OB_STR_PATH))) {
    LOG_WARN("failed to assign key", KR(ret), KPC(this));
  } else if (OB_FAIL(dest_.get_backup_dest_str(tmp.ptr(), tmp.capacity()))) {
    LOG_WARN("failed to get backup dest", KR(ret), KPC(this));
  } else if (OB_FAIL(config.value_.assign(tmp.ptr()))) {
    LOG_WARN("failed to assign value", KR(ret), KPC(this));
  } else if(OB_FAIL(items.push_back(config))) {
     LOG_WARN("failed to push backup config", KR(ret), KPC(this));
  }

  return ret;
}

int ObLogArchiveDestAtrr::get_binding(char *buf, int64_t len) const
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(databuff_printf(buf, len, "%s", STR_BINDING_TYPES[binding_]))) {
    LOG_WARN("failed to get binding", K(ret));
  }
  return ret;
}

int ObLogArchiveDestAtrr::get_piece_switch_interval(char *buf, int64_t len) const
{
  return ObBackupUtils::convert_timestamp_to_timestr(piece_switch_interval_, buf, len);
}

int ObLogArchiveDestAtrr::get_lag_target(char *buf, int64_t len) const
{
  return ObBackupUtils::convert_timestamp_to_timestr(lag_target_, buf, len);
}

int ObLogArchiveDestAtrr::assign(const ObLogArchiveDestAtrr& that)
{
  int ret = OB_SUCCESS;
  if (that.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid arguement", K(ret), K(that));
  } else {
    binding_ = that.binding_;
    dest_id_ = that.dest_id_;
    piece_switch_interval_ = that.piece_switch_interval_;
    lag_target_ = that.lag_target_;
    if (OB_FAIL(dest_.deep_copy(that.dest_))) {
      LOG_WARN("fail to deep copy dest", K(ret));
    } else if (OB_FAIL(state_.assign(that.state_))) {
      LOG_WARN("fail to assign state", K(ret));
    }
  }
  return ret;
}


int share::trim_right_backslash(ObBackupPathString &path)
{
  int ret = OB_SUCCESS;
  char *ptr = path.ptr();
  for (int64_t pos = path.size() - 1; pos >= 0; --pos) {
    if (ptr[pos] == '/') {
      ptr[pos] = '\0';
      --pos;
    } else {
      break;
    }
  }

  return ret;
}

// Convert time string.
int share::backup_time_to_strftime(const int64_t &ts_s, char *buf,
    const int64_t buf_len, int64_t &pos, const char concat)
{
  int ret = OB_SUCCESS;
  ObSqlString format;
  struct tm lt;
  int64_t strftime_len = 0;
  time_t t = static_cast<time_t>(ts_s);

  (void) localtime_r(&t, &lt);
  if (OB_FAIL(format.assign("%Y%m%d"))) {
    LOG_WARN("failed to build format string", K(ret), K(concat));
  } else if (OB_FAIL(format.append_fmt("%c", concat))) {
    LOG_WARN("failed to build format string", K(ret), K(concat));
  } else if (OB_FAIL(format.append("%H%M%S"))) {
    LOG_WARN("failed to build format string", K(ret), K(concat));
  } else if (0 == (strftime_len = strftime(buf + pos, buf_len - pos, format.ptr(), &lt))) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("failed to convert timestamp to string", K(ret), K(ts_s), KP(buf), K(buf_len), K(pos), K(concat), K(format));
  } else {
    pos += strftime_len;
  }

  return ret;
}

int share::backup_scn_to_time_tag(const SCN &scn, char *buf, const int64_t buf_len, int64_t &pos)
{
  int ret = OB_SUCCESS;
  const int64_t ts_s = trans_scn_to_second(scn);
  if (OB_FAIL(share::backup_time_to_strftime(ts_s, buf, buf_len, pos, 'T'/* concat */))) {
    LOG_WARN("failed to format time tag", K(ret), K(scn));
  }
  return ret;
}

int ObRestoreBackupSetBriefInfo::assign(const ObRestoreBackupSetBriefInfo &that)
{
  int ret = OB_SUCCESS;
  if (!that.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", K(ret), K(that));
  } else if (OB_FAIL(backup_set_path_.assign(that.backup_set_path_))) {
    LOG_WARN("fail to assign backup set path", K(ret), K(that));
  } else {
    backup_set_desc_ = that.backup_set_desc_;
  }
  return ret;
}

/* ObBackupSkippedType */

const char *ObBackupSkippedType::str() const
{
  const char *str = "INVALID_TYPE";
  switch (type_) {
  case DELETED: {
    str = "DELETED";
    break;
  }
  case TRANSFER: {
    str = "TRANSFER";
    break;
  }
  default: {
    str = "INVALID_TYPE";
  }
  }
  return str;
}

int ObBackupSkippedType::parse_from_str(const ObString &str)
{
  int ret = OB_SUCCESS;
  if (0 == str.case_compare("DELETED")) {
    type_ = DELETED;
  } else if (0 == str.case_compare("TRANSFER")) {
    type_ = TRANSFER;
  } else {
    type_ = MAX_TYPE;
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid backup skipped type str", KR(ret), K(str));
  }
  return ret;
}
