#; RUN : onnx-as fuse_conv_bn_mul_add_relu.s | onnx2tg -march bm1880 -ignore-calibration-step -print-module-before-isel | FileCheck fuse_conv_bn_mul_add_relu.s
#; CHECK: FLOAT tensor <4, 64, 112, 112> %scale_conv1_internal_1 = Scale <axis:INT 1, broadcast:INT 1> (FLOAT tensor <4, 64, 112, 112> %bn_conv1_1, FLOAT tensor <64> %scale_conv1_w_0, FLOAT tensor <64> %scale_conv1_b_0)

ir_version: 3
producer_name: "onnx-caffe2"
graph {
  name: "fuse-conv-bn-scale-relu"
  node { input: "data_0" input: "conv1_w_0" output: "conv1_1" name: "" op_type: "Conv" attribute { name: "pads" ints: 3 ints: 3 ints: 3 ints: 3 type: INTS } attribute { name: "strides" ints: 2 ints: 2 type: INTS } attribute { name: "kernel_shape" ints: 7 ints: 7 type: INTS } }
  node { input: "conv1_1" input: "bn_conv1_scale_0" input: "bn_conv1_bias_0" input: "bn_conv1_mean_0" input: "bn_conv1_var_0" output: "bn_conv1_1" name: "" op_type: "BatchNormalization" attribute { name: "is_test" i: 1 type: INT } attribute { name: "epsilon" f: 1e-05 type: FLOAT } }
  node { input: "bn_conv1_1" input: "scale_conv1_w_0" output: "scale_conv1_internal_1" name: "" op_type: "Mul" attribute { name: "axis" i: 1 type: INT } attribute { name: "broadcast" i: 1 type: INT } }
  node { input: "scale_conv1_internal_1" input: "scale_conv1_b_0" output: "scale_conv1_1" name: "" op_type: "Add" attribute { name: "axis" i: 1 type: INT } attribute { name: "broadcast" i: 1 type: INT } }
  node { input: "scale_conv1_1" output: "conv1_relu_1" name: "" op_type: "Relu" }
  input { name: "data_0" type { tensor_type { elem_type: FLOAT shape { dim { dim_value: 4 } dim { dim_value: 3 } dim { dim_value: 224 } dim { dim_value: 224 } } } } }
  input { name: "conv1_w_0" type { tensor_type { elem_type: FLOAT shape { dim { dim_value: 64 } dim { dim_value: 3 } dim { dim_value: 7 } dim { dim_value: 7 } } } } }
  input { name: "bn_conv1_scale_0" type { tensor_type { elem_type: FLOAT shape { dim { dim_value: 64 } } } } }
  input { name: "bn_conv1_bias_0" type { tensor_type { elem_type: FLOAT shape { dim { dim_value: 64 } } } } }
  input { name: "bn_conv1_mean_0" type { tensor_type { elem_type: FLOAT shape { dim { dim_value: 64 } } } } }
  input { name: "bn_conv1_var_0" type { tensor_type { elem_type: FLOAT shape { dim { dim_value: 64 } } } } }
  input { name: "scale_conv1_w_0" type { tensor_type { elem_type: FLOAT shape { dim { dim_value: 64 } } } } }
  input { name: "scale_conv1_b_0" type { tensor_type { elem_type: FLOAT shape { dim { dim_value: 64 } } } } }
  output { name: "conv1_relu_1" type { tensor_type { elem_type: FLOAT shape { dim { dim_value: 4} dim { dim_value: 64 } dim { dim_value: 112} dim {dim_value: 112 } } } } }
}
opset_import { domain: "" version: 6 }
