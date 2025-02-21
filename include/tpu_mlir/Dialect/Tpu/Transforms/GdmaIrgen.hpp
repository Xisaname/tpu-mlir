//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//
#pragma once
#include <list>
#include <map>
#include <set>
#include <vector>
#include "mlir/Support/LLVM.h"
#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"
#include "tpu_mlir/Support/Module.h"
#include "tpu_mlir/Dialect/Tpu/Transforms/IrInfo.hpp"
#include "tpu_mlir/Dialect/Tpu/Transforms/DynamicLayer.hpp"
using namespace std;
namespace tpu_mlir {
namespace tpu {

int static_ld_coeff_irgen_ctrl(
    Operation *op,
    int tensor_id, uint64_t global_addr,
    uint64_t local_addr,
    ir_tensor_gdma_info_t& ir_tensor_gdma_info,
    int dynamic_ver
  );

int static_ld_neuron_irgen_ctrl(
    Operation *op,
    int tensor_id, uint64_t global_addr,
    uint64_t local_addr,
    ir_tensor_gdma_info_t& ir_tensor_gdma_info,
    int dynamic_ver
  );

int static_st_neuron_irgen_ctrl(
    Operation *op,
    int tensor_id, uint64_t global_addr,
    uint64_t local_addr,
    ir_tensor_gdma_info_t& ir_tensor_gdma_info,
    int dynamic_ver
  );
}
}
