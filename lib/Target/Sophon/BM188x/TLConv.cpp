//===---------------------------------------------------------------------===//
//
//                             The ONNC Project
//
// Copyright(c) 2018, The ONNC Team
//
// This file is part of the ONNC Project and is distributed under
// 3-clause BSD license (https://opensource.org/licenses/BSD-3-Clause)
//
// See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------===//
#include "TLConv.h"
#include "BM188xCodeEmitter.h"
#include "PatternMatch.h"
#include <onnc/Support/Debug.h>
#include <onnc/Target/Sophon/BM188x/bmkernel_api.h>

#define DEBUG_TYPE "tl_conv"

namespace onnc {
namespace BM188X {

TLConv::TLConv(const xNode &pNode)
    : BM188xComputeOperator(pNode, std::string("TLConv"))
{
  // Default value
  m_Groups = 1;
  m_DilationH = m_DilationW = 1;
  m_PadHTop = m_PadHBot = m_PadWLeft = m_PadWRight = 0;
  m_StrideH = m_StrideW = 1;
  m_DoResultAdd = false;
  m_DoBias = false;
  m_UseWinograd = false;
  m_DoRelu = false;
  m_RShiftWidth = 0;

  // ONNC extension attribute
  assert(pNode.hasAttribute(xSymbol("op_name")));
  m_SplitName = pNode.s(xSymbol("op_name"));

  assert(pNode.hasAttribute(xSymbol("input_dim")));
  assert(pNode.hasAttribute(xSymbol("weight_dim")));
  assert(pNode.hasAttribute(xSymbol("output_dim")));

  auto &inDim = pNode.is(xSymbol("input_dim"));
  m_InN = inDim[0];
  m_InC = inDim[1];
  m_InH = inDim[2];
  m_InW = inDim[3];

  auto &weightDim = pNode.is(xSymbol("weight_dim"));
  // Weight Dim: <ic, oc, kh, kw>
  m_KH = weightDim[2];
  m_KW = weightDim[3];

  auto &outDim = pNode.is(xSymbol("output_dim"));
  m_OutC = outDim[1];
  m_OutH = outDim[2];
  m_OutW = outDim[3];

  assert(pNode.hasAttribute(xSymbol("result_add")));
  assert(pNode.hasAttribute(xSymbol("ifmap_laddr")));
  assert(pNode.hasAttribute(xSymbol("ofmap_laddr")));
  assert(pNode.hasAttribute(xSymbol("weight_laddr")));

  m_DoResultAdd = pNode.i(xSymbol("result_add"));
  m_IFmapAddr = pNode.i(xSymbol("ifmap_laddr"));
  m_OFmapAddr = pNode.i(xSymbol("ofmap_laddr"));
  m_WeightAddr = pNode.i(xSymbol("weight_laddr"));

  if (pNode.hasAttribute(xSymbol("bias_laddr"))) {
    m_BiasAddr = pNode.i(xSymbol("bias_laddr"));
    m_DoBias = true;
  }

  // End extension

  if (pNode.hasAttribute(xSymbol("group"))) {
    m_Groups = pNode.i(xSymbol("group"));
  }

  if (pNode.hasAttribute(xSymbol("kernel_shape"))) {
    auto &i = pNode.is(xSymbol("kernel_shape"));
    m_KH = i[0];
    m_KW = i[1];
  }

  if (pNode.hasAttribute(xSymbol("dilations"))) {
    auto &i = pNode.is(xSymbol("dilations"));
    m_DilationH = i[0];
    m_DilationW = i[1];
  }

  // [upPad, leftPad, downPad, rightPad]
  if (pNode.hasAttribute(xSymbol("slice_pads"))) {
    auto &i = pNode.is(xSymbol("slice_pads"));
    m_PadHTop = i[0];
    m_PadWLeft = i[1];
    m_PadHBot = i[2];
    m_PadWRight = i[3];
  }

  if (pNode.hasAttribute(xSymbol("strides"))) {
    auto &i = pNode.is(xSymbol("strides"));
    m_StrideH = i[0];
    m_StrideW = i[1];
  }

  // Support FuseRelu
  if (pNode.hasAttribute(xSymbol("do_relu"))) {
    m_DoRelu = true;
  }
}

TLConv *TLConv::addMemOperands(MemOperand *pInput, MemOperand *pWeight,
                               MemOperand *pOutput, MemOperand *pBias)
{
  m_MemOperands.push_back(pInput);
  m_MemOperands.push_back(pWeight);
  m_MemOperands.push_back(pOutput);
  if (pBias != nullptr) {
    assert(m_DoBias);
    m_MemOperands.push_back(pBias);
    m_BiasIdx = 3;
  }

  return this;
}

void TLConv::emit() const
{
  int ctrl = 0;

  // FIXME(arcbbb): Support Group == 1 for the moment
  assert(m_Groups == 1);

  bmnet::bmnet_asm::u32 bias_array[m_Groups];
  bmnet::bmnet_asm::u32 weight_array[m_Groups];

  bias_array[0] = m_BiasAddr;
  weight_array[0] = m_WeightAddr;

  bmnet::bmnet_asm::u32 *group_bias = &bias_array[0];
  bmnet::bmnet_asm::u32 *group_weight = &weight_array[0];

  DEBUG(dbgs()
    << "BM188X::SlicedConv\n" << "  " << m_SplitName << " "
    << m_IFmapAddr << " " << m_OFmapAddr << " "
    << m_WeightAddr << " " << m_BiasAddr << " "
    << m_InN << " " << m_InC << " " << m_InH << " " << m_InW << " "
    << m_Groups << " " << m_OutC << " " << m_OutH << " " << m_OutW << " "
    << m_KH << " " << m_KW << " "
    << m_DilationH << " " << m_DilationW << " "
    << m_PadHTop << " " << m_PadHBot << " "
    << m_PadWLeft << " " << m_PadWRight << " "
    << m_StrideH << " " << m_StrideW << " "
    << m_DoResultAdd << " " << m_RShiftWidth << " "
    << m_DoBias << " " << m_DoRelu << "\n");

  bmnet::bmnet_asm::asm_context::get_context().name = m_SplitName;
  bmnet::bmnet_asm::bmnet_tl_conv_forward_bmkernel(
      m_IFmapAddr,  // ifmap
      m_OFmapAddr,  // ofmap
      m_WeightAddr, // weight
      m_BiasAddr,   // bias
      group_weight, // weight addr for each group
      group_bias,   // bias addr for each group
      m_InN, m_InC, m_InH, m_InW, m_Groups, m_OutC, m_OutH, m_OutW, m_KH, m_KW,
      m_DilationH, m_DilationW,                      // Dilation
      m_PadHTop, m_PadHBot, m_PadWLeft, m_PadWRight, // padding
      m_StrideH, m_StrideW,                          // stride
      m_DoResultAdd,                                 // result_add
      ctrl, // FIXME(arcbbb): DoRelu should be a hint here
      m_RShiftWidth, m_DoBias, m_UseWinograd, m_DoRelu);
}

void TLConv::update(const tg::bm1880::LayerCalibrationParameter *pLayerCtable)
{
  m_RShiftWidth = pLayerCtable->right_shift_width();
}

} // namespace BM188X
} // namespace onnc
