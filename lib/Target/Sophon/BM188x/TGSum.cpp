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
#define DEBUG_TYPE "tg_sum"
#include "TGSum.h"
#include "BM188xCodeEmitter.h"
#include "PatternMatch.h"
#include <onnc/Support/Debug.h>
#include <onnc/Target/Sophon/BM188x/bmkernel_api.h>

namespace pm = onnc::PatternMatch;

namespace onnc {
namespace BM188X {

TGSum::TGSum(const xNode &pNode)
    : BM188xComputeOperator(pNode, std::string("Sum"))
{
  const std::vector<xDimension> inDim = pNode.inputs()[0]->sizes();
  m_InN = inDim[0].dim;
  m_InC = inDim[1].dim;
  m_InH = inDim[2].dim;
  m_InW = inDim[3].dim;
  m_InputNum = pNode.inputs().size();
  m_ThresholdXQuantized.resize(m_InputNum);
  m_DoRelu = pm::match(m_pNode, pm::mTrueAttr("do_relu"));
}

TGSum *TGSum::addMemOperands(std::vector<MemOperand *> pInput,
                             MemOperand *pOutput)
{
  for (auto in : pInput)
    m_MemOperands.push_back(in);
  m_MemOperands.push_back(pOutput);
  return this;
}

void TGSum::emit() const
{

  int in_size = m_MemOperands.size() - 1;
  uint64_t *input = new uint64_t[in_size];
  for (int i = 0; i < in_size; ++i)
    input[i] = m_MemOperands[i]->m_Addr;

  DEBUG(dbgs() << "TGSum::emit\n";
    dbgs() << "  inputs = ";
    for (int i = 0; i < in_size; ++i) dbgs() << input[i] << " ";
    dbgs() << "\n";
    dbgs() << "  " << m_MemOperands.back()->m_Addr
           << m_InN << " " <<  m_InC << " " <<  m_InH << " " <<  m_InW << " "
           << m_DoRelu << " " << m_RShiftWidth << "\n";
    dbgs() << "  xq = ";
    for (auto i : m_ThresholdXQuantized) dbgs() << i << " ";
    dbgs() << "\n";
  );

  bmnet::bmnet_asm::bmnet_eltwise_fixed_forward_bmkernel(
      input,                        // inputs
      m_MemOperands.back()->m_Addr, // ouput
      m_InputNum,
      1, // op: SUM
      m_InN, m_InC, m_InH, m_InW,
      m_DoRelu,      // do_relu
      0.0,           // relu_slope,
      m_RShiftWidth, // right_shift_width
      m_ThresholdXQuantized.data());

  delete[] input;
}

void TGSum::update(const tg::bm1880::LayerCalibrationParameter *pLayerCtable)
{
  m_RShiftWidth = pLayerCtable->right_shift_width();
  for (size_t i = 0; i < m_InputNum; ++i) {
    m_ThresholdXQuantized[i] = pLayerCtable->threshold_x_quantized(i);
  }
}

} // namespace BM188X
} // namespace onnc
