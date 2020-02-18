//
// Copyright (c) 2008-2020 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/RenderPath.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Slider.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/UIEvents.h>
#include <Urho3D/UI/Window.h>

#include "StaticScene.h"

#include <Urho3D/DebugNew.h>

URHO3D_DEFINE_APPLICATION_MAIN(StaticScene)

StaticScene::StaticScene(Context* context) :
    Sample(context)
{
}

void StaticScene::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the scene content
    CreateScene();

    // Create the UI content
    CreateInstructions();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update events
    SubscribeToEvents();

    // Set the mouse mode to use in the sample
    Sample::InitMouseMode(MM_RELATIVE);
}

void StaticScene::CreateScene()
{
    auto* cache = GetSubsystem<ResourceCache>();

    scene_ = new Scene(context_);

    cache->SetAutoReloadResources(true);

    // Create the Octree component to the scene. This is required before adding any drawable components, or else nothing will
    // show up. The default octree volume will be from (-1000, -1000, -1000) to (1000, 1000, 1000) in world coordinates; it
    // is also legal to place objects outside the volume but their visibility can then not be checked in a hierarchically
    // optimizing manner
    scene_->CreateComponent<Octree>();

    // Create a child scene node (at world origin) and a StaticModel component into it. Set the StaticModel to show a simple
    // plane mesh with a "stone" material. Note that naming the scene nodes is optional. Scale the scene node larger
    // (100 x 100 world units)
    Node* planeNode = scene_->CreateChild("Plane");
    planeNode->SetScale(Vector3(100.0f, 1.0f, 100.0f));
    auto* planeObject = planeNode->CreateComponent<StaticModel>();
    planeObject->SetModel(cache->GetResource<Model>("Models/Plane.mdl"));
    planeObject->SetMaterial(cache->GetResource<Material>("Materials/Prototype.xml"));

    Node* zoneNode = scene_->CreateChild("Zone", LOCAL);
    auto zone = zoneNode->CreateComponent<Zone>();
    zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
    zone->SetAmbientColor(Color(0.5f, 0.5f, 0.5f));
    zone->SetFogStart(100.0f);
    zone->SetFogEnd(300.0f);
    // Create a directional light to the world so that we can see something. The light scene node's orientation controls the
    // light direction; we will use the SetDirection() function which calculates the orientation from a forward direction vector.
    // The light will use default settings (white light, no shadows)
//    Node* lightNode = scene_->CreateChild("DirectionalLight");
//    lightNode->SetDirection(Vector3(0.6f, -1.0f, 0.8f)); // The direction vector does not need to be normalized
//    auto* light = lightNode->CreateComponent<Light>();
//    light->SetLightType(LIGHT_DIRECTIONAL);

    // Create more StaticModel objects to the scene, randomly positioned, rotated and scaled. For rotation, we construct a
    // quaternion from Euler angles where the Y angle (rotation about the Y axis) is randomized. The mushroom model contains
    // LOD levels, so the StaticModel component will automatically select the LOD level according to the view distance (you'll
    // see the model get simpler as it moves further away). Finally, rendering a large number of the same object with the
    // same material allows instancing to be used, if the GPU supports it. This reduces the amount of CPU work in rendering the
    // scene.
    const unsigned NUM_OBJECTS = 200;
    for (unsigned i = 0; i < NUM_OBJECTS; ++i)
    {
        Node* mushroomNode = scene_->CreateChild("Mushroom");
        mushroomNode->SetPosition(Vector3(i, 1.5f, 0));
//        mushroomNode->SetRotation(Quaternion(0.0f, Random(360.0f), 0.0f));
        mushroomNode->SetScale(3.0f + Random(2.0f));
        auto* mushroomObject = mushroomNode->CreateComponent<StaticModel>();
        mushroomObject->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
        mushroomObject->SetMaterial(cache->GetResource<Material>("Materials/Prototype.xml"));
    }

    // Create a scene node for the camera, which we will move around
    // The camera will use default settings (1000 far clip distance, 45 degrees FOV, set aspect ratio automatically)
    cameraNode_ = scene_->CreateChild("Camera");
    cameraNode_->CreateComponent<Camera>();

    // Set an initial position for the camera scene node above the plane
    cameraNode_->SetPosition(Vector3(0.0f, 5.0f, 0.0f));
}

void StaticScene::CreateInstructions()
{
    auto* cache = GetSubsystem<ResourceCache>();
    auto* ui = GetSubsystem<UI>();
    ui->GetRoot()->SetDefaultStyle(cache->GetResource<XMLFile>("UI/DefaultStyle.xml"));

    {
        // Construct new Text object, set string to display and font to use
        auto *instructionText = ui->GetRoot()->CreateChild<Text>();
        instructionText->SetText("SSAO Output");
        instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
        instructionText->SetColor(Color::BLACK);
        instructionText->SetTextEffect(TextEffect::TE_SHADOW);

        // Position the text relative to the screen center
        instructionText->SetHorizontalAlignment(HA_CENTER);
        instructionText->SetVerticalAlignment(VA_BOTTOM);
        instructionText->SetPosition(0, -50);
    }

    {
        // Construct new Text object, set string to display and font to use
        auto *instructionText = ui->GetRoot()->CreateChild<Text>();
        instructionText->SetText("SSAO Enabled");
        instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
        instructionText->SetColor(Color::GREEN);
        instructionText->SetTextEffect(TextEffect::TE_SHADOW);

        // Position the text relative to the screen center
        instructionText->SetHorizontalAlignment(HA_LEFT);
        instructionText->SetVerticalAlignment(VA_TOP);
        instructionText->SetPosition(50, 50);
    }

    {
        // Construct new Text object, set string to display and font to use
        auto *instructionText = ui->GetRoot()->CreateChild<Text>();
        instructionText->SetText("SSAO Disabled");
        instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
        instructionText->SetColor(Color::RED);
        instructionText->SetTextEffect(TextEffect::TE_SHADOW);

        // Position the text relative to the screen center
        instructionText->SetHorizontalAlignment(HA_RIGHT);
        instructionText->SetVerticalAlignment(VA_TOP);
        instructionText->SetPosition(-50, 50);
    }

    window_ = ui->GetRoot()->CreateChild<Window>();
    window_->SetHorizontalAlignment(HA_LEFT);
    window_->SetVerticalAlignment(VA_CENTER);
    window_->SetPosition(10, 0);
    window_->SetFixedWidth(400);
    window_->SetLayoutMode(LM_VERTICAL);
    window_->SetLayoutSpacing(10);
    window_->SetStyleAuto();
    window_->SetVisible(false);

    auto strengthSlider = CreateSlider("Strength", 1.0, 5.0);
    SubscribeToEvent(strengthSlider, E_SLIDERCHANGED, [&](StringHash eventType, VariantMap& eventData) {
        using namespace SliderChanged;
        float value = eventData[P_VALUE].GetFloat();
        GetSubsystem<Renderer>()->GetViewport(0)->GetRenderPath()->SetShaderParameter("SSAOStrength", value);
    });

    auto areaSlider = CreateSlider("Area", 1.75, 3.0);
    SubscribeToEvent(areaSlider, E_SLIDERCHANGED, [&](StringHash eventType, VariantMap& eventData) {
        using namespace SliderChanged;
        float value = eventData[P_VALUE].GetFloat();
        GetSubsystem<Renderer>()->GetViewport(0)->GetRenderPath()->SetShaderParameter("SSAOArea", value);
    });

    auto falloffSlider = CreateSlider("Falloff", 1.0, 10.0);
    SubscribeToEvent(falloffSlider, E_SLIDERCHANGED, [&](StringHash eventType, VariantMap& eventData) {
        using namespace SliderChanged;
        float value = eventData[P_VALUE].GetFloat();
        GetSubsystem<Renderer>()->GetViewport(0)->GetRenderPath()->SetShaderParameter("SSAOFalloff", value / 1000.0f);
    });

    auto noiseFactorSlider = CreateSlider("Noise Factor", 7.0, 20.0);
    SubscribeToEvent(noiseFactorSlider, E_SLIDERCHANGED, [&](StringHash eventType, VariantMap& eventData) {
        using namespace SliderChanged;
        float value = eventData[P_VALUE].GetFloat();
        GetSubsystem<Renderer>()->GetViewport(0)->GetRenderPath()->SetShaderParameter("SSAONoiseFactor", value);
    });

    auto radiusSlider = CreateSlider("Radius", 0.6, 10.0);
    SubscribeToEvent(radiusSlider, E_SLIDERCHANGED, [&](StringHash eventType, VariantMap& eventData) {
        using namespace SliderChanged;
        float value = eventData[P_VALUE].GetFloat();
        GetSubsystem<Renderer>()->GetViewport(0)->GetRenderPath()->SetShaderParameter("SSAORadius", value);
    });

}

void StaticScene::SetupViewport()
{
    auto* renderer = GetSubsystem<Renderer>();
    auto cache = GetSubsystem<ResourceCache>();
    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen. We need to define the scene and the camera
    // at minimum. Additionally we could configure the viewport screen size and the rendering path (eg. forward / deferred) to
    // use, but now we just use full screen and default render path configured in the engine command line options
    SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
    SharedPtr<RenderPath> effectRenderPath = viewport->GetRenderPath()->Clone();
    effectRenderPath->Append(cache->GetResource<XMLFile>("PostProcess/SSAO.xml"));
    effectRenderPath->SetEnabled("SSAO", true);

//    effectRenderPath->Append(cache->GetResource<XMLFile>("PostProcess/Bloom.xml"));
//    effectRenderPath->SetEnabled("Bloom", true);
    viewport->SetRenderPath(effectRenderPath);
    renderer->SetViewport(0, viewport);
}

void StaticScene::MoveCamera(float timeStep)
{
    // Do not move if the UI has a focused element (the console)
    if (GetSubsystem<UI>()->GetFocusElement())
        return;

    auto* input = GetSubsystem<Input>();

    if (input->GetKeyPress(KEY_TAB)) {
        window_->SetVisible(!window_->IsVisible());
        if (window_->IsVisible()) {
            input->SetMouseVisible(true);
            input->SetMouseMode(MM_FREE);
        } else {
            input->SetMouseVisible(false);
            input->SetMouseMode(MM_ABSOLUTE, true);
        }
    }

    if (window_->IsVisible())
        return;

    // Movement speed as world units per second
    const float MOVE_SPEED = 20.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.1f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    IntVector2 mouseMove = input->GetMouseMove();
    yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
    pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
    pitch_ = Clamp(pitch_, -90.0f, 90.0f);

    // Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    // Use the Translate() function (default local space) to move relative to the node's orientation.
    if (input->GetKeyDown(KEY_W))
        cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_S))
        cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_A))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_D))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);
}

void StaticScene::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(StaticScene, HandleUpdate));
}

void StaticScene::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;

    // Take the frame time step, which is stored as a float
    float timeStep = eventData[P_TIMESTEP].GetFloat();

    // Move the camera, scale movement with time step
    MoveCamera(timeStep);
}

Slider* StaticScene::CreateSlider(const String& text, float value, float range)
{
    UIElement* root = window_->CreateChild<UIElement>();
    root->SetFixedWidth(window_->GetWidth());
    root->SetLayoutMode(LM_VERTICAL);
    root->SetLayoutSpacing(20);
    auto* cache = GetSubsystem<ResourceCache>();
    auto* font = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");

    // Create text and slider below it
    auto* sliderText = root->CreateChild<Text>();
    sliderText->SetFixedHeight(30);
    sliderText->SetFont(font, 12);
    sliderText->SetText(text);

    auto* slider = root->CreateChild<Slider>();
    slider->SetStyleAuto();
    slider->SetRange(range);
    slider->SetValue(value);
    slider->SetFixedHeight(30);

    return slider;
}
