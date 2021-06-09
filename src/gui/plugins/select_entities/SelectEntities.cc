/*
 * Copyright (C) 2021 Open Source Robotics Foundation
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

#include <ignition/common/MouseEvent.hh>
#include <ignition/gui/Application.hh>
#include <ignition/gui/GuiEvents.hh>
#include <ignition/gui/MainWindow.hh>
#include <ignition/plugin/Register.hh>
#include <ignition/rendering/RenderingIface.hh>
#include <ignition/rendering/Visual.hh>
#include <ignition/rendering/WireBox.hh>
#include "ignition/rendering/Camera.hh"

#include "ignition/gazebo/Entity.hh"
#include "ignition/gazebo/gui/GuiEvents.hh"
#include "ignition/gazebo/components/Name.hh"
#include "ignition/gazebo/rendering/RenderUtil.hh"

#include "SelectEntities.hh"

namespace ignition
{
namespace gazebo
{
/// \brief Helper to store selection requests to be handled in the render
/// thread by `IgnRenderer::HandleEntitySelection`.
struct SelectionHelper
{
  /// \brief Entity to be selected
  Entity selectEntity{kNullEntity};

  /// \brief Deselect all entities
  bool deselectAll{false};

  /// \brief True to send an event and notify all widgets
  bool sendEvent{false};
};
}
}

/// \brief Private data class for SelectEntities
class ignition::gazebo::plugins::SelectEntitiesPrivate
{
  public: void Initialize();

  public: void HandleEntitySelection();

  public: void UpdateSelectedEntity(const rendering::VisualPtr &_visual,
    bool _sendEvent);

  public: void HighlightNode(const rendering::VisualPtr &_visual);

  public: void SetSelectedEntity(const rendering::VisualPtr &_visual);

  public: void DeselectAllEntities();

  public: void LowlightNode(const rendering::VisualPtr &_visual);

  /// \brief Helper object to select entities. Only the latest selection
  /// request is kept.
  public: SelectionHelper selectionHelper;

  /// \brief Currently selected entities, organized by order of selection.
  public: std::vector<Entity> selectedEntities;

  //// \brief Pointer to the rendering scene
  public: rendering::ScenePtr scene = nullptr;

  /// \brief A map of entity ids and wire boxes
  public: std::unordered_map<Entity, ignition::rendering::WireBoxPtr> wireBoxes;

  public: ignition::common::MouseEvent mouseEvent;

  public: bool mouseDirty = false;

  /// \brief User camera
  public: rendering::CameraPtr camera = nullptr;

  public: bool transformControlActive = false;
};

using namespace ignition;
using namespace gazebo;
using namespace plugins;

/////////////////////////////////////////////////
void SelectEntitiesPrivate::HandleEntitySelection()
{
  if (!mouseDirty)
    return;

  this->mouseDirty = false;

  rendering::VisualPtr visual = this->scene->VisualAt(
        this->camera,
        this->mouseEvent.Pos());

  if (!visual)
  {
    this->DeselectAllEntities();
    return;
  }

  this->selectionHelper.selectEntity = std::get<int>(visual->UserData("gazebo-entity"));

  if (this->selectionHelper.deselectAll)
  {
    this->DeselectAllEntities();

    this->selectionHelper = SelectionHelper();
  }
  else if (this->selectionHelper.selectEntity != kNullEntity)
  {
    this->UpdateSelectedEntity(visual,
        this->selectionHelper.sendEvent);

    this->selectionHelper = SelectionHelper();
  }
}

////////////////////////////////////////////////
void SelectEntitiesPrivate::LowlightNode(const rendering::VisualPtr &_visual)
{
  Entity entityId = kNullEntity;
  if (_visual)
    entityId = std::get<int>(_visual->UserData("gazebo-entity"));
  if (this->wireBoxes.find(entityId) != this->wireBoxes.end())
  {
    ignition::rendering::WireBoxPtr wireBox =
      this->wireBoxes[entityId];
    auto visParent = wireBox->Parent();
    if (visParent)
      visParent->SetVisible(false);
  }
}

////////////////////////////////////////////////
void SelectEntitiesPrivate::HighlightNode(const rendering::VisualPtr &_visual)
{
  Entity entityId = kNullEntity;

  if (_visual)
    entityId = std::get<int>(_visual->UserData("gazebo-entity"));

  // If the entity is not found in the existing map, create a wire box
  auto wireBoxIt = this->wireBoxes.find(entityId);
  if (wireBoxIt == this->wireBoxes.end())
  {
    auto white = this->scene->Material("highlight_material");
    if (!white)
    {
      white = this->scene->CreateMaterial("highlight_material");
      white->SetAmbient(1.0, 1.0, 1.0);
      white->SetDiffuse(1.0, 1.0, 1.0);
      white->SetSpecular(1.0, 1.0, 1.0);
      white->SetEmissive(1.0, 1.0, 1.0);
    }

    ignition::rendering::WireBoxPtr wireBox =
      this->scene->CreateWireBox();
    ignition::math::AxisAlignedBox aabb = _visual->LocalBoundingBox();
    wireBox->SetBox(aabb);

    // Create visual and add wire box
    ignition::rendering::VisualPtr wireBoxVis =
      this->scene->CreateVisual();
    wireBoxVis->SetInheritScale(false);
    wireBoxVis->AddGeometry(wireBox);
    wireBoxVis->SetMaterial(white, false);
    _visual->AddChild(wireBoxVis);

    // Add wire box to map for setting visibility
    this->wireBoxes.insert(
        std::pair<Entity, ignition::rendering::WireBoxPtr>(entityId, wireBox));
  }
  else
  {
    ignition::rendering::WireBoxPtr wireBox = wireBoxIt->second;
    ignition::math::AxisAlignedBox aabb = _visual->LocalBoundingBox();
    wireBox->SetBox(aabb);
    auto visParent = wireBox->Parent();
    if (visParent)
      visParent->SetVisible(true);
  }
}

/////////////////////////////////////////////////
void SelectEntitiesPrivate::SetSelectedEntity(const rendering::VisualPtr &_visual)
{
  Entity entityId = kNullEntity;

  if (_visual)
    entityId = std::get<int>(_visual->UserData("gazebo-entity"));

  if (entityId == kNullEntity)
    return;

  this->selectedEntities.push_back(_visual->Id());
  this->HighlightNode(_visual);
  ignition::gazebo::gui::events::EntitiesSelected entitiesSelected(
    this->selectedEntities, true);
  ignition::gui::App()->sendEvent(
      ignition::gui::App()->findChild<ignition::gui::MainWindow *>(),
      &entitiesSelected);
}

/////////////////////////////////////////////////
void SelectEntitiesPrivate::DeselectAllEntities()
{
  if (this->scene)
  {
    for (const auto &entity : this->selectedEntities)
    {
      auto node = this->scene->VisualById(entity);
      auto vis = std::dynamic_pointer_cast<rendering::Visual>(node);
      this->LowlightNode(vis);
    }
    this->selectedEntities.clear();

    ignition::gazebo::gui::events::DeselectAllEntities deselectEvent(true);
    ignition::gui::App()->sendEvent(
        ignition::gui::App()->findChild<ignition::gui::MainWindow *>(),
        &deselectEvent);
  }
}

/////////////////////////////////////////////////
void SelectEntitiesPrivate::UpdateSelectedEntity(const rendering::VisualPtr &_visual,
    bool _sendEvent)
{

  bool deselectedAll{false};
  std::cerr << "transformControlActive " << transformControlActive << '\n';

  // Deselect all if control is not being held
  if ((!(QGuiApplication::keyboardModifiers() & Qt::ControlModifier) &&
      !this->selectedEntities.empty()) || this->transformControlActive)
  {
    // Notify other widgets regardless of _sendEvent, because this is a new
    // decision from this widget
    this->DeselectAllEntities();
    deselectedAll = true;
  }

  // Select new entity
  this->SetSelectedEntity(_visual);

  // Notify other widgets of the currently selected entities
  if (_sendEvent || deselectedAll)
  {
    ignition::gazebo::gui::events::EntitiesSelected selectEvent(
        this->selectedEntities);
    ignition::gui::App()->sendEvent(
        ignition::gui::App()->findChild<ignition::gui::MainWindow *>(),
        &selectEvent);
  }
}

/////////////////////////////////////////////////
void SelectEntitiesPrivate::Initialize()
{
  if (nullptr == this->scene)
  {
    this->scene = rendering::sceneFromFirstRenderEngine();
    if (nullptr == this->scene)
      return;

    this->camera = std::dynamic_pointer_cast<rendering::Camera>(this->scene->SensorByName("Scene3DCamera"));
    if (!this->camera)
    {
      ignerr << "Camera is not available" << std::endl;
      return;
    }
  }
}

/////////////////////////////////////////////////
SelectEntities::SelectEntities()
  : GuiSystem(), dataPtr(new SelectEntitiesPrivate)
{
}

/////////////////////////////////////////////////
SelectEntities::~SelectEntities()
{
}

/////////////////////////////////////////////////
void SelectEntities::LoadConfig(const tinyxml2::XMLElement *)
{
  if (this->title.empty())
    this->title = "Select entities";

  ignition::gui::App()->findChild<
      ignition::gui::MainWindow *>()->QuickWindow()->installEventFilter(this);
  ignition::gui::App()->findChild<
      ignition::gui::MainWindow *>()->installEventFilter(this);
}

/////////////////////////////////////////////////
void SelectEntities::Update(const UpdateInfo &/* _info */,
    EntityComponentManager &_ecm)
{
}

/////////////////////////////////////////////////
bool SelectEntities::eventFilter(QObject *_obj, QEvent *_event)
{
  if (_event->type() == ignition::gui::events::LeftClickOnScene::kType)
  {
    ignition::gui::events::LeftClickOnScene *_e =
      static_cast<ignition::gui::events::LeftClickOnScene*>(_event);
    this->dataPtr->mouseEvent = _e->Mouse();
    this->dataPtr->mouseDirty = true;
  }
  else if (_event->type() == ignition::gui::events::Render::kType)
  {
    this->dataPtr->Initialize();
    this->dataPtr->HandleEntitySelection();
  }
  else if (_event->type() == ignition::gazebo::gui::events::TransformControlMode::kType)
  {
    auto transformControlMode = reinterpret_cast<ignition::gazebo::gui::events::TransformControlMode *>(
        _event);
    this->dataPtr->transformControlActive = transformControlMode->TransformControl();
  }

  // Standard event processing
  return QObject::eventFilter(_obj, _event);
}

// Register this plugin
IGNITION_ADD_PLUGIN(ignition::gazebo::plugins::SelectEntities,
                    ignition::gui::Plugin)