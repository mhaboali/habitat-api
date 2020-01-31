// Copyright (c) Facebook, Inc. and its affiliates.
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "Agent.h"

#include <Magnum/EigenIntegration/GeometryIntegration.h>
#include <Magnum/EigenIntegration/Integration.h>

#include "esp/scene/ObjectControls.h"
#include "esp/sensor/PinholeCamera.h"
#include "esp/sensor/Sensor.h"

using Magnum::EigenIntegration::cast;

namespace esp {
namespace agent {

const std::set<std::string> Agent::BodyActions = {"moveRight",   "moveLeft",
                                                  "moveForward", "moveBackward",
                                                  "turnLeft",    "turnRight"};

Agent::Agent(scene::SceneNode& agentNode, const AgentConfiguration& cfg)
    : Magnum::SceneGraph::AbstractFeature3D(agentNode),
      configuration_(cfg),
      sensors_(),
      controls_(scene::ObjectControls::create()) {
  agentNode.setType(scene::SceneNodeType::AGENT);
  for (sensor::SensorSpec::ptr spec : cfg.sensorSpecifications) {
    // TODO: this should take type into account to create appropriate
    // sensor

    auto& sensorNode = agentNode.createChild();
    sensors_.add(
        sensor::PinholeCamera::create(sensorNode, spec));  // transformed within
  }
}

Agent::~Agent() {
  LOG(INFO) << "Deconstructing Agent";
  sensors_.clear();
}

bool Agent::act(const std::string& actionName) {
  if (hasAction(actionName)) {
    const ActionSpec& actionSpec = *configuration_.actionSpace.at(actionName);
    if (BodyActions.find(actionSpec.name) != BodyActions.end()) {
      controls_->action(object(), actionSpec.name,
                        actionSpec.actuation.at("amount"),
                        /*applyFilter=*/true);
    } else {
      for (auto p : sensors_.getSensors()) {
        controls_->action(p.second->object(), actionSpec.name,
                          actionSpec.actuation.at("amount"),
                          /*applyFilter=*/false);
      }
    }
    return true;
  } else {
    return false;
  }
}

bool Agent::hasAction(const std::string& actionName) {
  auto actionSpace = configuration_.actionSpace;
  return !(actionSpace.find(actionName) == actionSpace.end());
}

void Agent::getState(AgentState::ptr state) const {
  // TODO this should be done less hackishly
  state->position = cast<vec3f>(node().absoluteTransformation().translation());
  state->rotation = quatf(node().rotation()).coeffs();
  // TODO other state members when implemented
}

void Agent::setState(const AgentState& state,
                     const bool resetSensors /*= true*/) {
  node().setTranslation(Magnum::Vector3(state.position));

  const Eigen::Map<const quatf> rot(state.rotation.data());
  CHECK_LT(std::abs(rot.norm() - 1.0),
           2.0 * Magnum::Math::TypeTraits<float>::epsilon())
      << state.rotation << " not a valid rotation";
  node().setRotation(Magnum::Quaternion(quatf(rot)).normalized());

  if (resetSensors) {
    for (auto p : sensors_.getSensors()) {
      p.second->setTransformationFromSpec();
    }
  }
  // TODO other state members when implemented
}

bool operator==(const ActionSpec& a, const ActionSpec& b) {
  return a.name == b.name && a.actuation == b.actuation;
}
bool operator!=(const ActionSpec& a, const ActionSpec& b) {
  return !(a == b);
}

bool operator==(const AgentConfiguration& a, const AgentConfiguration& b) {
  return a.height == b.height && a.radius == b.radius && a.mass == b.mass &&
         a.linearAcceleration == b.linearAcceleration &&
         a.angularAcceleration == b.angularAcceleration &&
         a.linearFriction == b.linearFriction &&
         a.angularFriction == b.angularFriction &&
         a.coefficientOfRestitution == b.coefficientOfRestitution &&
         esp::equal(a.sensorSpecifications, b.sensorSpecifications) &&
         esp::equal(a.actionSpace, b.actionSpace) && a.bodyType == b.bodyType;
}
bool operator!=(const AgentConfiguration& a, const AgentConfiguration& b) {
  return !(a == b);
}

}  // namespace agent
}  // namespace esp
