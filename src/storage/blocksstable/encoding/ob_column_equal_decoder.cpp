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

#define USING_LOG_PREFIX STORAGE

#include "ob_column_equal_decoder.h"
#include "ob_encoding_util.h"

namespace oceanbase
{
using namespace common;
namespace blocksstable
{
const ObColumnHeader::Type ObColumnEqualDecoder::type_;

ObColumnEqualDecoder::ObColumnEqualDecoder()
  : inited_(false), meta_header_(NULL)
{
}

ObColumnEqualDecoder::~ObColumnEqualDecoder()
{
}

int ObColumnEqualDecoder::decode(ObColumnDecoderCtx &ctx, ObObj &cell, const int64_t row_id,
    const ObBitStream &bs, const char *data, const int64_t len) const
{
  UNUSED(bs);
  UNUSED(data);
  UNUSED(len);
  int ret = OB_SUCCESS;
  if (OB_UNLIKELY(!inited_)) {
    ret = OB_NOT_INIT;
    LOG_WARN("not init", K(ret));
  } else if (OB_UNLIKELY(row_id < 0)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid arguments", K(ret), K(row_id));
  } else {
    int64_t ref = 0;
    if (cell.get_meta() != ctx.obj_meta_) {
      cell.set_meta_type(ctx.obj_meta_);
    }

    if (!has_exc(ctx)) {
      ref = -1;
    } else {
      const ObObjType store_type = ctx.col_header_->get_store_obj_type();
      const ObObjTypeClass tc = ob_obj_type_class(store_type);
      switch (get_store_class_map()[tc]) {
        case ObUIntSC:
        case ObIntSC: {
          if (OB_FAIL(ObBitMapMetaReader<ObUIntSC>::read(
              meta_header_->payload_, ctx.micro_block_header_->row_count_,
              ctx.is_bit_packing(), row_id,
              ctx.col_header_->length_ - sizeof(ObColumnEqualMetaHeader),
              ref, cell, store_type))) {
            LOG_WARN("meta_reader_ read failed", K(ret), K(row_id), K(ctx));
          }
          break;
        }
        case ObNumberSC: {
          if (OB_FAIL(ObBitMapMetaReader<ObNumberSC>::read(
              meta_header_->payload_, ctx.micro_block_header_->row_count_,
              ctx.is_bit_packing(), row_id,
              ctx.col_header_->length_ - sizeof(ObColumnEqualMetaHeader),
              ref, cell, store_type))) {
            LOG_WARN("meta_reader_ read failed", K(ret), K(row_id), K(ctx));
          }
          break;
        }
        case ObStringSC:
        case ObTextSC:
        case ObJsonSC:
        case ObGeometrySC: {
          if (OB_FAIL(ObBitMapMetaReader<ObStringSC>::read(
              meta_header_->payload_, ctx.micro_block_header_->row_count_,
              ctx.is_bit_packing(), row_id,
              ctx.col_header_->length_ - sizeof(ObColumnEqualMetaHeader),
              ref, cell, store_type))) {
            LOG_WARN("meta_reader_ read failed", K(ret), K(row_id), K(ctx));
          }
          break;
        }
        case ObOTimestampSC: {
          if (OB_FAIL(ObBitMapMetaReader<ObOTimestampSC>::read(
              meta_header_->payload_, ctx.micro_block_header_->row_count_,
              ctx.is_bit_packing(), row_id,
              ctx.col_header_->length_ - sizeof(ObColumnEqualMetaHeader),
              ref, cell, store_type))) {
            LOG_WARN("meta_reader_ read failed", K(ret), K(row_id), K(ctx));
          }
          break;
        }
        case ObIntervalSC: {
          if (OB_FAIL(ObBitMapMetaReader<ObIntervalSC>::read(
              meta_header_->payload_, ctx.micro_block_header_->row_count_,
              ctx.is_bit_packing(), row_id,
              ctx.col_header_->length_ - sizeof(ObColumnEqualMetaHeader),
              ref, cell, store_type))) {
            LOG_WARN("meta_reader_ read failed", K(ret), K(row_id), K(ctx));
          }
          break;
        }
        default:
          ret = OB_INNER_STAT_ERROR;
          LOG_WARN("not supported store class", K(ret), K(ctx));
      }
    }

    // not an exception, get from reffed column
    if (OB_SUCC(ret) && -1 == ref) {
      if (OB_FAIL(ctx.ref_decoder_->decode(*ctx.ref_ctx_, cell, row_id, bs, data, len))) {
        LOG_WARN("ref_decoder_ decode failed", K(ret),
            K(row_id), KP(data), K(len));
      }
    }
  }
  return ret;
}

int ObColumnEqualDecoder::update_pointer(const char *old_block, const char *cur_block)
{
  int ret = OB_SUCCESS;
  if (!inited_) {
    ret = OB_NOT_INIT;
    LOG_WARN("not init", K(ret));
  } else if (OB_ISNULL(old_block) || OB_ISNULL(cur_block)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", K(ret), KP(old_block), KP(cur_block));
  } else {
    ObIColumnDecoder::update_pointer(meta_header_, old_block, cur_block);
  }
  return ret;
}

int ObColumnEqualDecoder::get_ref_col_idx(int64_t &ref_col_idx) const
{
  int ret = OB_SUCCESS;
  if (!inited_) {
    ret = OB_NOT_INIT;
    LOG_WARN("not init", K(ret));
  } else {
    ref_col_idx = meta_header_->ref_col_idx_;
  }
  return ret;
}

}//end namespace blocksstable
}//end namespace oceanbase

