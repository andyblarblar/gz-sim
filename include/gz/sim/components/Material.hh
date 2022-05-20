/*
 * Copyright (C) 2018 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/
#ifndef GZ_SIM_COMPONENTS_MATERIAL_HH_
#define GZ_SIM_COMPONENTS_MATERIAL_HH_

#include <gz/msgs/material.pb.h>

#include <sdf/Material.hh>

#include <gz/sim/components/Factory.hh>
#include <gz/sim/components/Component.hh>
#include <gz/sim/components/Serialization.hh>
#include <gz/sim/config.hh>

namespace ignition
{
namespace gazebo
{
// Inline bracket to help doxygen filtering.
inline namespace IGNITION_GAZEBO_VERSION_NAMESPACE {
namespace serializers
{
  using MaterialSerializer =
      serializers::ComponentToMsgSerializer<sdf::Material, msgs::Material>;
}
namespace components
{
  /// \brief This component holds an entity's material.
  using Material = Component<sdf::Material, class MaterialTag,
                             serializers::MaterialSerializer>;
  IGN_GAZEBO_REGISTER_COMPONENT("gz_sim_components.Material", Material)
}
}
}
}

#endif
