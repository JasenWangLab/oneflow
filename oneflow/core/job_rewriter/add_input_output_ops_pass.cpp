/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "oneflow/core/common/util.h"
#include "oneflow/core/common/maybe.h"
#include "oneflow/core/device/cuda_util.h"
#include "oneflow/core/job/placement.pb.h"
#include "oneflow/core/job_rewriter/op_graph_pass.h"
#include "oneflow/core/operator/op_conf.pb.h"
#include "oneflow/core/register/logical_blob_id.pb.h"
#include "oneflow/core/register/runtime_blob_desc.h"

namespace oneflow {

namespace {

std::string MakeInputOpConf(const std::string& input_op_name, const InterfaceBlobConf& blob_conf,
                            OperatorConf* input_op_conf) {
  input_op_conf->set_name(input_op_name);
  auto* input_conf = input_op_conf->mutable_input_conf();
  input_conf->set_out("out");
  input_conf->mutable_blob_conf()->CopyFrom(blob_conf);
  return GenLogicalBlobName(input_op_name, "out");
}

std::string MakeOutputOpConf(const std::string& output_op_name, const LogicalBlobId& lbi,
                             OperatorConf* output_op_conf) {
  output_op_conf->set_name(output_op_name);
  auto* return_conf = output_op_conf->mutable_return_conf();
  return_conf->set_in(GenLogicalBlobName(lbi));
  return_conf->set_out("out");
  return GenLogicalBlobName(output_op_name, "out");
}

class AddInputOutputOpsPass final : public OpGraphPass {
 public:
  AddInputOutputOpsPass() = default;
  ~AddInputOutputOpsPass() override = default;
  bool IsEnabled() const override { return true; }
  Maybe<void> Apply(const OpGraph& op_graph, JobBuilder* job_builder) const override;
};

Maybe<void> AddInputOutputOpsPass::Apply(const OpGraph& op_graph, JobBuilder* job_builder) const {
  if (!job_builder->job().job_conf().has_signature()) { return Maybe<void>::Ok(); }
  const auto& job_sig = job_builder->job().job_conf().signature();
  // std::cout << "job signature: " << job_builder->job().job_conf().job_name() << std::endl
  //           << PbMessage2TxtString(job_sig) << std::endl;
  auto IsInputLbi = [&](const LogicalBlobId& lbi) -> bool {
    for (const auto& pair : job_sig.inputs()) {
      if (pair.second.lbi() == lbi) { return true; }
    }
    return false;
  };

  HashSet<std::string> keep_op_names;
  // TODO: This search way only support stateless subgraph.
  // Control edge and mutable input need to be considered when supporting side-effect subgraph.
  std::function<Maybe<void>(const LogicalBlobId&)> SearchConstSrcAndTrace;
  SearchConstSrcAndTrace = [&](const LogicalBlobId& lbi) -> Maybe<void> {
    if (IsInputLbi(lbi)) { return Maybe<void>::Ok(); }
    const auto* op_node = op_graph.OpNode4OpName(lbi.op_name());
    if (op_node->in_edges().empty()) { return Maybe<void>::Ok(); }
    keep_op_names.insert(lbi.op_name());
    for (const auto& ibn : op_node->op().input_bns()) {
      CHECK_OR_RETURN(!op_node->op().InputBlobModifier4Ibn(ibn).is_mutable());
      const auto& src_lbi = op_node->op().BnInOp2Lbi(ibn);
      SearchConstSrcAndTrace(src_lbi);
    }
    return Maybe<void>::Ok();
  };
  for (const auto& pair : job_sig.outputs()) { SearchConstSrcAndTrace(pair.second.lbi()); }

  std::vector<std::string> drop_op_names;
  op_graph.ForEachNode([&](const OpNode* op_node) {
    const auto& op_name = op_node->op().op_name();
    if (keep_op_names.find(op_name) == keep_op_names.end()) { drop_op_names.emplace_back(op_name); }
  });
  for (const auto& op_name : keep_op_names) {
    const auto* op_node = op_graph.OpNode4OpName(op_name);
    for (const auto& ctrl_in_op_name : op_node->op().op_conf().ctrl_in_op_name()) {
      // keep op can't include ctrl_in edge of drop op
      CHECK_OR_RETURN(std::find(drop_op_names.begin(), drop_op_names.end(), ctrl_in_op_name)
                      == drop_op_names.end());
    }
  }

  HashMap<std::string, OperatorConf> io_op_name2op_conf;
  HashMap<std::string, const ParallelConf*> io_op_name2parallel_conf;
  HashSet<std::string> input_consumer_op_names;
  std::vector<OperatorConf> input_consumer_op_confs;
  for (const auto& pair : job_sig.inputs()) {
    const auto& input_name = pair.first;
    const auto& input_def = pair.second;
    const auto* op_node = op_graph.OpNode4OpName(input_def.lbi().op_name());

    CHECK_OR_RETURN(io_op_name2op_conf.emplace(input_name, OperatorConf()).second);
    std::string input_lbn =
        MakeInputOpConf(input_name, input_def.blob_conf(), &io_op_name2op_conf[input_name]);
    CHECK_OR_RETURN(
        io_op_name2parallel_conf.emplace(input_name, &op_node->parallel_desc().parallel_conf())
            .second);

    for (const OpEdge* out_edge : op_node->out_edges()) {
      auto iter = out_edge->lbi2ibns().find(input_def.lbi());
      if (iter == out_edge->lbi2ibns().end()) { continue; }
      const auto* consumer_op_node = out_edge->dst_node();
      const auto& consumer_op_name = consumer_op_node->op().op_name();
      CHECK_OR_RETURN(input_consumer_op_names.insert(consumer_op_name).second);
      input_consumer_op_confs.emplace_back(consumer_op_node->op().op_conf());
      auto* consumer_op_conf = &input_consumer_op_confs.back();
      for (const auto& ibn : iter->second) {
        const auto& old_lbn = ReplaceInputLbnInOpCustomizedConf(consumer_op_conf, ibn, input_lbn);
        CHECK_EQ(old_lbn, GenLogicalBlobName(input_def.lbi()));
      }
    }
  }
  for (const auto& pair : job_sig.outputs()) {
    const auto& output_name = pair.first;
    const auto& output_def = pair.second;
    const auto* op_node = op_graph.OpNode4OpName(output_def.lbi().op_name());
    CHECK_OR_RETURN(io_op_name2op_conf.emplace(output_name, OperatorConf()).second);
    MakeOutputOpConf(output_name, output_def.lbi(), &io_op_name2op_conf[output_name]);
    CHECK_OR_RETURN(
        io_op_name2parallel_conf.emplace(output_name, &op_node->parallel_desc().parallel_conf())
            .second);
  }

  for (const auto& pair : io_op_name2op_conf) {
    const auto* parallel_conf = io_op_name2parallel_conf.at(pair.first);
    job_builder->AddOps(*parallel_conf, {pair.second});
  }
  job_builder->MutOpsOnlyOnce(input_consumer_op_confs);
  job_builder->DelOps(drop_op_names);
  return Maybe<void>::Ok();
}

}  // namespace

REGISTER_FUNCTION_PASS("AddInputOutputOpsPass", AddInputOutputOpsPass);

}  // namespace oneflow
