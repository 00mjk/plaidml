// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "plaidml_builder.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ngraph/function.hpp"
#include "ngraph/opsets/opset1.hpp"

#include "plaidml_ops.hpp"
#include "plaidml_util.hpp"

namespace PlaidMLPlugin {

namespace {

class ProgramBuilder {
 public:
  explicit ProgramBuilder(const InferenceEngine::ICNNNetwork& network);

  plaidml::Program build();

 private:
  void handleConstant(const std::shared_ptr<ngraph::Node>& node);
  void handleParameter(const std::shared_ptr<ngraph::Node>& node);
  void handleOutput(const std::shared_ptr<ngraph::Node>& node);
  void handleOp(const std::shared_ptr<ngraph::Node>& node);

  const InferenceEngine::ICNNNetwork& network;

  // cache network input/output info
  InferenceEngine::InputsDataMap networkInputs;
  InferenceEngine::OutputsDataMap networkOutputs;

  // Lets us look up the PlaidML tensor by the name of the node that produces it and the index of which output it is
  std::map<std::pair<std::string, size_t>, plaidml::edsl::Tensor> tensorMap;

  // Go from the names OV uses for a networks inputs and outputs to the corresponding PlaidML Tensor
  std::map<std::string, plaidml::edsl::Tensor> tensorIONameMap;

  // Update dependencies map
  std::map<std::pair<std::string, size_t>, std::string> updateDependenciesNameMap;
};

ProgramBuilder::ProgramBuilder(const InferenceEngine::ICNNNetwork& network) : network(network) {
  network.getInputsInfo(networkInputs);
  network.getOutputsInfo(networkOutputs);
}

plaidml::Program ProgramBuilder::build() {
  std::shared_ptr<const ngraph::Function> func = network.getFunction();
  IE_ASSERT(func);  // PlaidML requires that the nGraph-based API be used
  for (const std::shared_ptr<ngraph::Node>& node : func->get_ordered_ops()) {
    if (node->description() == "Constant") {
      handleConstant(node);
    } else if (node->description() == "Parameter") {
      handleParameter(node);
    } else if (node->description() == "Result") {
      handleOutput(node);
    } else {
      handleOp(node);
    }
  }

  std::vector<plaidml::edsl::Tensor> inputs;
  for (const auto& kvp : networkInputs) {
    inputs.push_back(tensorIONameMap.at(kvp.first));
  }
  std::vector<plaidml::edsl::Tensor> outputs;
  for (const auto& kvp : networkOutputs) {
    outputs.push_back(tensorIONameMap.at(kvp.first));
  }

  return plaidml::edsl::buildProgram("ie", inputs, outputs);
}

struct PlaidMLAttributeVisitor : public ngraph::AttributeVisitor {
  plaidml::edsl::Dictionary attrs;

  void on_adapter(const std::string& name, ngraph::ValueAccessor<void>& adapter) final {
    THROW_IE_EXCEPTION << "Unsupported 'void' attribute: " << name;
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<std::string>& adapter) final {}

  void on_adapter(const std::string& name, ngraph::ValueAccessor<bool>& adapter) final {
    attrs[name] = plaidml::edsl::Value(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<int64_t>& adapter) final {
    attrs[name] = plaidml::edsl::Value(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<double>& adapter) final {
    attrs[name] = plaidml::edsl::Value(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<std::vector<std::string>>& adapter) final {
    attrs[name] = plaidml::edsl::make_tuple(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<std::vector<float>>& adapter) final {
    attrs[name] = plaidml::edsl::make_tuple(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<std::vector<double>>& adapter) final {
    attrs[name] = plaidml::edsl::make_tuple(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<std::vector<int8_t>>& adapter) final {
    attrs[name] = plaidml::edsl::make_tuple(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<std::vector<int16_t>>& adapter) final {
    attrs[name] = plaidml::edsl::make_tuple(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<std::vector<int32_t>>& adapter) final {
    attrs[name] = plaidml::edsl::make_tuple(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<std::vector<int64_t>>& adapter) final {
    attrs[name] = plaidml::edsl::make_tuple(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<std::vector<uint8_t>>& adapter) final {
    attrs[name] = plaidml::edsl::make_tuple(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<std::vector<uint16_t>>& adapter) final {
    attrs[name] = plaidml::edsl::make_tuple(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<std::vector<uint32_t>>& adapter) final {
    attrs[name] = plaidml::edsl::make_tuple(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<std::vector<uint64_t>>& adapter) final {
    attrs[name] = plaidml::edsl::make_tuple(adapter.get());
  }

  void on_adapter(const std::string& name, ngraph::ValueAccessor<void*>& adapter) final {
    THROW_IE_EXCEPTION << "Unsupported 'void*' attribute: " << name;
  }
};

void ProgramBuilder::handleConstant(const std::shared_ptr<ngraph::Node>& node) {
  IE_ASSERT(node->get_output_size() == 1);
  IE_ASSERT(node->description() == "Constant");
  plaidml::DType type = to_plaidml(node->get_element_type());
  std::vector<int64_t> dims{node->get_shape().begin(), node->get_shape().end()};
  plaidml::TensorShape shape(type, dims);
  plaidml::Buffer buffer(shape);
  Context ctx{node.get()};
  auto* layer = dynamic_cast<ngraph::opset1::Constant*>(ctx.layer);
  buffer.copy_from(layer->get_data_ptr());
  plaidml::edsl::Tensor tensor = plaidml::edsl::Constant(buffer, node->get_friendly_name());
  tensorMap[std::make_pair(node->get_name(), 0)] = tensor;

  std::vector<int64_t> dims_reordered{dims[0], dims[2], dims[3], dims[1]};
  // Special case for bias, need to solve it, by checking if the user is convolution and needs XCK format
  // or bias that needs NXC format
  if (dims[1] == 1000) {
    dims_reordered[1] = 1;
    dims_reordered[2] = 1;
  }
  auto conv_user = ngraph::as_type<ngraph::opset1::Convolution>(node->get_users()[0].get());
  if (conv_user) {
    dims_reordered[0] = dims[2];
    dims_reordered[1] = dims[3];
    dims_reordered[2] = dims[1];
    dims_reordered[3] = dims[0];
  }
  auto reduce_user = ngraph::as_type<ngraph::opset1::ReduceMean>(node->get_users()[0].get());
  auto reshape_user = ngraph::as_type<ngraph::opset1::Reshape>(node->get_users()[0].get());
  if (reduce_user || reshape_user) {
    dims_reordered[0] = 1;
    dims_reordered[1] = 1;
    dims_reordered[2] = 1;
    dims_reordered[3] = 2048;
  }
  auto matmul_user = ngraph::as_type<ngraph::opset1::MatMul>(node->get_users()[0].get());
  if (matmul_user) {
    dims_reordered[0] = dims[1];
    dims_reordered[1] = dims[0];
    dims_reordered[2] = 1;
    dims_reordered[3] = 1;
  }
  auto reorder = plaidml::edsl::reshape(tensor, dims_reordered);
  tensorMap[std::make_pair(node->get_name() + "_reordered", 0)] = reorder;
  updateDependenciesNameMap[std::make_pair(node->get_name(), 0)] = node->get_name() + "_reordered";
}

void ProgramBuilder::handleParameter(const std::shared_ptr<ngraph::Node>& node) {
  IE_ASSERT(node->get_output_size() == 1);
  // TODO: Decide if we want to compare to the nGraph dims & type and issue warnings
  // std::vector<int64_t> ng_dims{node->get_shape().begin(), node->get_shape().end()};
  auto inputDesc = networkInputs[node->get_friendly_name()]->getTensorDesc();
  std::vector<int64_t> dims{inputDesc.getDims().begin(), inputDesc.getDims().end()};
  plaidml::DType type = to_plaidml(inputDesc.getPrecision());
  plaidml::DType ng_type = to_plaidml(node->get_element_type());
  plaidml::edsl::Tensor tensor = plaidml::edsl::Placeholder(type, dims, node->get_friendly_name());
  plaidml::edsl::Tensor cast_tensor;

  if (ng_type != type) {
    cast_tensor = plaidml::edsl::cast(tensor, ng_type);
  } else {
    cast_tensor = tensor;
  }
  tensorMap[std::make_pair(node->get_name(), 0)] = cast_tensor;
  tensorIONameMap[node->get_friendly_name()] = tensor;

  // TODO: change reshape to reorder to have functional correctness in case input layout is different
  // TODO: add generic function to change dims
  std::vector<int64_t> dims_reordered{dims[0], dims[2], dims[3], dims[1]};
  auto reorder = plaidml::edsl::reshape(cast_tensor, dims_reordered);
  tensorMap[std::make_pair(node->get_name() + "_reordered", 0)] = reorder;
  updateDependenciesNameMap[std::make_pair(node->get_name(), 0)] = node->get_name() + "_reordered";
}

void ProgramBuilder::handleOutput(const std::shared_ptr<ngraph::Node>& node) {
  // The OV output name is the name of the node _prior_ to the result
  // When there are multiple outputs, it has .# appended, where # is the output index
  const ngraph::Output<ngraph::Node>& src_output = node->input(0).get_source_output();
  const ngraph::Node* src_node = src_output.get_node();
  std::string name = src_node->get_friendly_name();
  if (src_node->get_output_size() > 1) {
    name += "." + std::to_string(src_output.get_index());
  }
  const auto requested_prec = to_plaidml(networkOutputs[name]->getTensorDesc().getPrecision());
  const plaidml::edsl::Tensor& tensor = tensorMap.at(std::make_pair(src_node->get_name(), src_output.get_index()));
  if (requested_prec == tensor.dtype()) {
    tensorIONameMap[name] = tensor;
  } else {
    tensorIONameMap[name] = plaidml::edsl::cast(tensor, requested_prec);
  }
}

void ProgramBuilder::handleOp(const std::shared_ptr<ngraph::Node>& node) {
  const Op op = OpsRegistry::instance()->resolve(node->description());
  if (!op) {
    THROW_IE_EXCEPTION << "Unsupported operation: " << node->description();
  }

  Context ctx{node.get()};
  for (const auto& input : node->inputs()) {
    const ngraph::Output<ngraph::Node>& src_output = input.get_source_output();
    const std::string& name = src_output.get_node()->get_name();
    size_t index = src_output.get_index();
    auto dep = std::make_pair(name, index);
    // Check if dependency update is needed
    if (updateDependenciesNameMap.find(dep) != updateDependenciesNameMap.end())
      dep.first = updateDependenciesNameMap.at(dep);
    plaidml::edsl::Tensor tensor = tensorMap.at(dep);
    ctx.operands.push_back(tensor);
  }
  PlaidMLAttributeVisitor visitor;
  node->visit_attributes(visitor);
  plaidml::edsl::TensorVec tuple = plaidml::edsl::layer("ng." + node->description(), visitor.attrs, [&]() {
    plaidml::edsl::Value value = op(ctx);
    std::vector<plaidml::edsl::Value> tuple = value.as_tuple();
    plaidml::edsl::TensorVec outputs;
    outputs.reserve(tuple.size());
    for (plaidml::edsl::Value output : tuple) {
      outputs.push_back(output.as_tensor());
    }
    return outputs;
  });
  IE_ASSERT(tuple.size() == node->get_output_size());
  for (unsigned i = 0; i < tuple.size(); i++) {
    plaidml::edsl::Tensor tensor = tuple.at(i);
    tensorMap[std::make_pair(node->get_name(), i)] = tensor;
  }
}

}  // namespace

plaidml::Program buildProgram(const InferenceEngine::ICNNNetwork& network) { return ProgramBuilder(network).build(); }

}  // namespace PlaidMLPlugin
