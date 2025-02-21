//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//
#include "tpu_mlir/Conversion/TopToTpu/LoweringCV18xx.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "lowering-Preprocess"
namespace tpu_mlir {
namespace cv18xx {

template<typename T>
void swapInputChannelOfFilter(
      std::vector<T> &filter_data, std::vector<T> &new_filter,
      RankedTensorType &filter_type) {

  std::vector<int64_t> shape(filter_type.getShape());
  int64_t size = std::accumulate(std::begin(shape), std::end(shape), 1,
                      std::multiplies<>());
  assert(filter_data.size() == size && "filter size should be equal");

  int64_t oc, ic, frame_size;
  int64_t index = shape.size();
  assert((index == 4 || index == 5) && "filter shape size should be 4 or 5");

  frame_size = shape[index - 1] * shape[index - 2];
  ic = shape[index - 3];
  oc = shape[index - 4];
  if (index == 5) {
    oc *= shape[index - 5];
  }
  std::vector<int> order {2, 1, 0};
  //std::vector<T> new_filter(size);
  T *filter = (T *)filter_data.data();
  for (int i = 0; i < oc; ++i) {
    for (int j = 0; j < ic; ++j) {
      assert(order[j] < ic);
      T *in = filter + i * ic * frame_size + order[j] * frame_size;
      T *out =
          (T *)new_filter.data() + i * ic * frame_size + j * frame_size;
      memcpy(out, in, frame_size * sizeof(T));
    }
  }
}

template<typename T>
LogicalResult FoldSwapAxisOp(Operation *op, PatternRewriter &rewriter) {
  auto swapOp = dyn_cast_or_null<top::SwapChannelOp>(op);
  if (!swapOp) {
    return failure();
  }
  auto nextOp = module::getNextOp(swapOp);
  if (!nextOp) {
    return failure();
  }
  //nextOp->getResult(0).dump();
  auto convOp = dyn_cast_or_null<tpu::Conv2DOp>(nextOp);
  if (!convOp) {
    return failure();
  }
  if (convOp.getGroup() != 1) {
    return failure();
  }
  assert(convOp.getNumOperands() == 3 && "Conv2D op should have 3 operands");
  // filter
  auto filterOp = cast<top::WeightOp>(convOp.getFilter().getDefiningOp());
  auto filter_data = *(filterOp.read<T>());
  auto filter_name = module::getName(filterOp.getOutput()).str();
  auto filter_type = convOp.getFilter().getType().template cast<RankedTensorType>();
  std::vector<T> new_filter_data(filter_data.size());
  swapInputChannelOfFilter<T>(filter_data, new_filter_data, filter_type);
  auto newFilterOp = top::WeightOp::create(convOp, filter_name + "_swap_channel",
                                            new_filter_data, filter_type);
  convOp.setOperand(1, newFilterOp);
  rewriter.replaceOp(op, {swapOp.getOperand()});
  return success();
}

class LowerPreprocess {
public:
  void lowerPreprocess(PatternRewriter &rewriter, top::PreprocessOp op) {
    auto name = module::getName(op.getOutput()).str();
    std::string quant_mode = op.getQuantMode().str();
    auto resized_dims = module::getI64Array(op.getResizeDims());
    std::string channel_order = op.getChannelOrder().str();
    this->mean = *(module::getF64Array(op.getMean()));
    this->scale = *(module::getF64Array(op.getScale()));
    std::string pixel_format = op.getCustomizationFormat().str();
    std::vector<int64_t> model_shape;
    module::getShapeVec(op.getResult(), model_shape);
    module::getNCHW(model_shape, n, c, h, w, false);
    if (resized_dims->size() == 2) {
      this->resize_h = resized_dims->at(0);
      this->resize_w = resized_dims->at(1);
    } else {
      this->resize_h = h;
      this->resize_w = w;
    }
    auto caliType = module::getCalibratedType(op.getOutput());
    double threshold = caliType.getMax();
    std::map<std::string,
              std::pair<std::string, std::string>> attributes_map = {
      {"RGB_PLANAR",    {"rgb", "nchw"}},
      {"RGB_PACKED",    {"rgb", "nhwc"}},
      {"BGR_PLANAR",    {"bgr", "nchw"}},
      {"BGR_PACKED",    {"bgr", "nhwc"}},
      {"GRAYSCALE",     {"bgr", "nchw"}},
      {"YUV420_PLANAR", {"bgr", "nchw"}},
      {"YUV_NV21",      {"bgr", "nchw"}},
      {"YUV_NV12",      {"bgr", "nchw"}},
      {"RGBA_PLANAR", {"rgba", "nchw"}}
    };
    auto color = std::get<0>(attributes_map[pixel_format]);
    auto layout = std::get<1>(attributes_map[pixel_format]);
    bool swap_channel = (color != channel_order) ? true : false;
    llvm::errs() << "pixel_format:" << pixel_format
                << ", color:" << color
                << ", layout:" << layout
                << ", swap_channel:" << swap_channel
                << "\n";
    mlir::Value currentOut = op.getInput();
    rewriter.setInsertionPointAfterValue(currentOut);
      // create transpose Op if need
    if (layout == "nhwc") {
      currentOut = this->insertTransposeOp(rewriter, name, currentOut);
      rewriter.setInsertionPointAfterValue(currentOut);
    }

    // create SliceOp
    if (resize_h != h || resize_w != w) {
      //llvm::errs()<<"this->type:"<<this->type<<"\n";
      currentOut = this->insertSliceOp(rewriter, name, currentOut);
      rewriter.setInsertionPointAfterValue(currentOut);
    }

    if (quant_mode == "INT8") {
      currentOut = this->insertScaleLutOp(rewriter, name, currentOut, threshold, swap_channel);
      rewriter.setInsertionPointAfterValue(currentOut);
    } else {
      //BF16
      currentOut = this->insertScaleOp(rewriter, name, currentOut, threshold, swap_channel);
    }

    if (swap_channel) {
      currentOut = this->insertSwapAxisOp(rewriter, name, currentOut, threshold);
    }

    rewriter.replaceOp(op, {currentOut});

    Operation *curOp = currentOut.getDefiningOp();
    if (quant_mode == "INT8") {
      FoldSwapAxisOp<int8_t>(curOp, rewriter);
    } else {
      FoldSwapAxisOp<uint16_t>(curOp, rewriter);
    }
  }

private:
  int64_t n, c, h, w;
  int64_t resize_h, resize_w;
  std::vector<double> mean, scale;

  Value insertTransposeOp(PatternRewriter &rewriter, std::string &name, Value opd) {
    auto loc = NameLoc::get(rewriter.getStringAttr(name + "_tranpose"));
    std::vector<int64_t> order{0, 3, 1, 2};
    std::vector<NamedAttribute> attrs;
    attrs.emplace_back(rewriter.getNamedAttr("order", rewriter.getI64ArrayAttr(order)));
    //auto cali_type = quant::CalibratedQuantizedType::get(rewriter.getIntegerType(8, false), 0, 255);
    auto cali_type = quant::CalibratedQuantizedType::get(rewriter.getF32Type(), 0, 255);
    auto type = RankedTensorType::get({n, c, resize_h, resize_w}, cali_type);
    auto newOp = rewriter.create<top::PermuteOp>(loc, type, ArrayRef<Value>{opd}, attrs);
    return newOp.getOutput();
  }

  Value insertSliceOp(PatternRewriter &rewriter, std::string &name, Value opd) {
    auto loc = NameLoc::get(rewriter.getStringAttr(name + "_slice"));
    int64_t start_h = resize_h / 2 - h / 2;
    int64_t start_w = resize_w / 2 - w / 2;
    std::vector<int64_t> slice_offset{0, 0, start_h, start_w};
    std::vector<int64_t> slice_step{1, 1, 1, 1};
    std::vector<NamedAttribute> attrs;
    attrs.emplace_back(rewriter.getNamedAttr("offset", rewriter.getI64ArrayAttr(slice_offset)));
    attrs.emplace_back(rewriter.getNamedAttr("steps", rewriter.getI64ArrayAttr(slice_step)));
    //auto cali_type = quant::CalibratedQuantizedType::get(rewriter.getIntegerType(8, false), 0, 255);
    auto cali_type = quant::CalibratedQuantizedType::get(rewriter.getF32Type(), 0, 255);
    auto type = RankedTensorType::get({n, c, h, w}, cali_type);
    auto newOp = rewriter.create<top::SliceOp>(loc, type, ArrayRef<Value>{opd}, attrs);
    return newOp.getOutput();
  }

  Value insertScaleLutOp(PatternRewriter &rewriter, std::string &name, Value opd,
                          double threshold, bool swap_channel) {
    auto loc = NameLoc::get(rewriter.getStringAttr(name + "_scale_lut"));
    double qscale = module::getScale(threshold, true, 8);
    //double qscale = 127.0 / threshold;
    std::vector<double> scales;
    std::vector<double> bias;
    for (int i = 0; i < c; i++) {
      scales.push_back(this->scale[i]);
      bias.push_back(-1 * this->scale[i] * this->mean[i]);
    }
    if (swap_channel) {
      // keep order bgr
      std::swap(scales[0], scales[2]);
      std::swap(bias[0], bias[2]);
    }
    //quant
    for (int i = 0; i < c; i++) {
      scales[i] /= qscale;
      bias[i] /= qscale;
    }
    std::vector<NamedAttribute> attrs;
    attrs.emplace_back(rewriter.getNamedAttr("scale", rewriter.getF64ArrayAttr(scales)));
    attrs.emplace_back(rewriter.getNamedAttr("bias", rewriter.getF64ArrayAttr(bias)));
    //auto cali_type = quant::CalibratedQuantizedType::get(rewriter.getIntegerType(8, true), -threshold, threshold);
    auto cali_type = quant::CalibratedQuantizedType::get(rewriter.getF32Type(), -threshold, threshold);
    auto type = RankedTensorType::get({n, c, h, w}, cali_type);
    auto newOp = rewriter.create<top::ScaleLutOp>(loc, type, ArrayRef<Value>{opd}, attrs);
    return newOp.getOutput();
  }

  Value insertScaleOp(PatternRewriter &rewriter, std::string &name, Value opd,
                          double threshold, bool swap_channel) {
    auto loc = NameLoc::get(rewriter.getStringAttr(name + "_scale"));
    auto none = module::getNoneOp(opd.getDefiningOp());
    std::vector<float> scales;
    std::vector<float> bias;
    for (int i = 0; i < c; i++) {
      scales.push_back(this->scale[i]);
      bias.push_back(-1 * this->scale[i] * this->mean[i]);
    }

    llvm::errs() << "scale:";
    for (auto s : scales)
      llvm::errs() << " " << s;
    llvm::errs() << "\n";
    llvm::errs() << "bias:";
    for (auto b : bias)
      llvm::errs() << " " << b;
    llvm::errs() << "\n";

    if (c == 3 && swap_channel) {
      std::swap(scales[0], scales[2]);
      std::swap(bias[0], bias[2]);
    }

    auto scale_type = RankedTensorType::get({1, c, 1, 1}, rewriter.getF32Type());
    auto bias_type = RankedTensorType::get({1, c, 1, 1}, rewriter.getF32Type());
    std::vector<Value> operands;
    operands.emplace_back(opd);
    operands.emplace_back(none);
    operands.emplace_back(none);
    std::vector<NamedAttribute> attrs;
    attrs.emplace_back(rewriter.getNamedAttr("do_relu", rewriter.getBoolAttr(false)));
    auto cali_type = quant::CalibratedQuantizedType::get(rewriter.getF32Type(), -threshold, threshold);
    auto type = RankedTensorType::get({n, c, h, w}, cali_type);
    auto newOp = rewriter.create<top::ScaleOp>(loc, type, operands, attrs);
    auto scale_weight = top::WeightOp::create(newOp, name + "_scale_0", scales, scale_type);
    auto bias_Weight = top::WeightOp::create(newOp, name + "_scale_1", bias, bias_type);
    newOp.setOperand(1, scale_weight);
    newOp.setOperand(2, bias_Weight);
    return newOp.getOutput();
  }

  Value insertSwapAxisOp(PatternRewriter &rewriter, std::string &name, Value opd, float threshold) {
    llvm::errs()<<"Entering insertSwapAxisOp.\n";
    auto loc = NameLoc::get(rewriter.getStringAttr(name + "_swapaxis"));
    std::vector<NamedAttribute> attrs;
    std::vector<int64_t> orders {2, 1, 0};
    attrs.emplace_back(rewriter.getNamedAttr("channel_order", rewriter.getI64ArrayAttr(orders)));
    auto cali_type = quant::CalibratedQuantizedType::get(rewriter.getF32Type(), -threshold, threshold);
    auto type = RankedTensorType::get({n, c, h, w}, cali_type);
    auto newOp = rewriter.create<top::SwapChannelOp>(loc, type, ArrayRef<Value>{opd}, attrs);
    return newOp.getOutput();
  }
};


void PreprocessLowering::LoweringINT8(PatternRewriter &rewriter, top::PreprocessOp op,
                                bool asymmetric) const {
  //lowering_common_int8<tpu::PreprocessOp>(rewriter, op, asymmetric);
  LowerPreprocess prelower = LowerPreprocess();
  prelower.lowerPreprocess(rewriter, op);

}

void PreprocessLowering::LoweringBF16(PatternRewriter &rewriter,
                                top::PreprocessOp op) const {
  //auto out = op.getOutput();
  LoweringINT8(rewriter, op, false);
}

}
}
