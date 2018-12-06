//=================================================================================================
//
//	MJP's DX11 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"
#include "Settings.h"
#include "Exceptions.h"
#include "Utility.h"
#include "App.h"
#include "ImGuiHelper.h"
#include "ImGui/imgui.h"

namespace SampleFramework12
{

static Float2 CanvasTransform(Float2 pos, Float2 canvasStart, Float2 canvasSize)
{
    return canvasStart + canvasSize * (pos * Float2(0.5f, -0.5f) + Float2(0.5f));
}

// == Setting =====================================================================================

Setting::Setting()
{
}

void Setting::Initialize(SettingType type_, void* data_, const char* name_,
                         const char* group_, const char* label_, const char* helpText_)
{
    type = type_;
    data = data_;
    name = name_;
    group = group_;
    label = label_;
    helpText = helpText_;
    changed = false;

    initialized = true;
}

void Setting::SetReadOnly(bool readOnly)
{
    /*Assert_(initialized);
    TwHelper::SetReadOnly(tweakBar, name.c_str(), readOnly);*/
}

void Setting::SetEditable(bool editable)
{
    SetReadOnly(!editable);
}

void Setting::SetHidden(bool hidden)
{
    visible = !hidden;
}

void Setting::SetVisible(bool visible_)
{
    visible = visible_;
}

void Setting::SetLabel(const char* newLabel)
{
    Assert_(newLabel != nullptr);
    label = newLabel;
}

FloatSetting& Setting::AsFloat()
{
    Assert_(type == SettingType::Float);
    return *(static_cast<FloatSetting*>(this));
}

IntSetting& Setting::AsInt()
{
    Assert_(type == SettingType::Int);
    return *(static_cast<IntSetting*>(this));
}

BoolSetting& Setting::AsBool()
{
    Assert_(type == SettingType::Bool);
    return *(static_cast<BoolSetting*>(this));
}

EnumSetting& Setting::AsEnum()
{
    Assert_(type == SettingType::Enum);
    return *(static_cast<EnumSetting*>(this));
}

DirectionSetting& Setting::AsDirection()
{
    Assert_(type == SettingType::Direction);
    return *(static_cast<DirectionSetting*>(this));
}

OrientationSetting& Setting::AsOrientation()
{
    Assert_(type == SettingType::Orientation);
    return *(static_cast<OrientationSetting*>(this));
}

ColorSetting& Setting::AsColor()
{
    Assert_(type == SettingType::Color);
    return *(static_cast<ColorSetting*>(this));
}

Button& Setting::AsButton()
{
    Assert_(type == SettingType::Button);
    return *(static_cast<Button*>(this));
}

bool Setting::Changed() const
{
    return changed;
}

bool Setting::Visible() const
{
    return visible;
}

const std::string& Setting::Name() const
{
    return name;
}

const std::string& Setting::Group() const
{
    return group;
}

uint64 Setting::SerializedValueSize()
{
    ComputeSizeSerializer serializer;
    const uint64 startSize = serializer.Size();
    SerializeValue(serializer);
    return serializer.Size() - startSize;
}

// == FloatSetting ================================================================================

FloatSetting::FloatSetting() : val(0.0f), oldVal(0.0f), minVal(0.0f), maxVal(0.0f), step(0.0)
{
}

void FloatSetting::Initialize(const char* name_, const char* group_,
                              const char* label_, const char* helpText_, float initialVal,
                              float minVal_, float maxVal_, float step_, ConversionMode conversionMode_,
                              float conversionScale_)
{
    val = initialVal;
    oldVal = initialVal;
    minVal = minVal_;
    maxVal = maxVal_;
    step = step_;
    conversionMode = conversionMode_;
    conversionScale = conversionScale_;
    Assert_(minVal <= maxVal);
    Assert_(minVal <= val && val <= maxVal);
    Setting::Initialize(SettingType::Float, &val, name_, group_, label_, helpText_);
}

void FloatSetting::Update(const Float4x4& viewMatrix)
{
    if(minVal > -3.0e+38f && maxVal < 3.0e+38f)
    {
        ImGui::SliderFloat(label.c_str(), &val, minVal, maxVal);
    }
    else
    {
        ImGui::DragFloat(label.c_str(), &val, step, minVal, maxVal);
    }

    if(ImGui::IsItemHovered() && helpText.length() > 0)
        ImGui::SetTooltip("%s", helpText.c_str());

    changed = oldVal != val;
    oldVal = val;
}

float FloatSetting::Value() const
{
    float converted = val;
    if(conversionMode == ConversionMode::Square)
        converted *= converted;
    else if(conversionMode == ConversionMode::SquareRoot)
        converted = std::sqrt(converted);
    else if(conversionMode == ConversionMode::DegToRadians)
        converted *= (180.0f / 3.14159f);
    return converted * conversionScale;
}

float FloatSetting::RawValue() const
{
    return val;
}

void FloatSetting::SetValue(float newVal)
{
    val = Clamp(newVal, minVal, maxVal);
}

FloatSetting::operator float()
{
    return Value();
}

// == IntSetting ==================================================================================

IntSetting::IntSetting() :  val(0), oldVal(0), minVal(0), maxVal(0)
{
}

void IntSetting::Initialize(const char* name_, const char* group_,
                            const char* label_, const char* helpText_, int32 initialVal,
                            int32 minVal_, int32 maxVal_)
{
    val = initialVal;
    oldVal = initialVal;
    minVal = minVal_;
    maxVal = maxVal_;
    Assert_(minVal <= maxVal);
    Assert_(minVal <= val && val <= maxVal);
    Setting::Initialize(SettingType::Int, &val, name_, group_, label_, helpText_);
}

void IntSetting::Update(const Float4x4& viewMatrix)
{
    if(minVal > -INT32_MAX && maxVal < INT32_MAX)
    {
        ImGui::SliderInt(label.c_str(), &val, minVal, maxVal);
    }
    else
    {
        ImGui::InputInt(label.c_str(), &val);
        val = Clamp(val, minVal, maxVal);
    }

    changed = oldVal != val;
    oldVal = val;
}

int32 IntSetting::Value() const
{
    return val;
}

void IntSetting::SetValue(int32 newVal)
{
    val = Clamp(newVal, minVal, maxVal);
}

IntSetting::operator int32()
{
    return val;
}

// == BoolSetting =================================================================================

BoolSetting::BoolSetting() : val(false), oldVal(0)
{
}

void BoolSetting::Initialize(const char* name_,
                             const char* group_, const char* label_,
                             const char* helpText_, bool32 initialVal)
{
    val = initialVal ? true : false;
    oldVal = val;
    Setting::Initialize(SettingType::Bool, &val, name_, group_, label_, helpText_);
}

void BoolSetting::Update(const Float4x4& viewMatrix)
{
    bool setting = val ? true : false;
    ImGui::Checkbox(label.c_str(), &setting);
    if(ImGui::IsItemHovered() && helpText.length() > 0)
        ImGui::SetTooltip("%s", helpText.c_str());

    val = setting;

    changed = oldVal != val;
    oldVal = val;
}

bool32 BoolSetting::Value() const
{
    return val;
}

void BoolSetting::SetValue(bool32 newVal)
{
    val = newVal ? true : false;
}

BoolSetting::operator bool32()
{
    return val;
}

// == EnumSetting =================================================================================

EnumSetting::EnumSetting()
{
}

void EnumSetting::Initialize(const char* name_,
                             const char* group_, const char* label_,
                             const char* helpText_, uint32 initialVal,
                             uint32 numValues_, const char** valueLabels_)
{
    val = std::min(initialVal, numValues - 1);
    oldVal = val;
    numValues = numValues_;
    numValuesClamp = numValues_;
    valueLabels = valueLabels_;

    Setting::Initialize(SettingType::Enum, &val, name_, group_, label_, helpText_);
}

void EnumSetting::Update(const Float4x4& viewMatrix)
{
    ImGui::Combo(label.c_str(), reinterpret_cast<int32*>(&val), valueLabels, numValuesClamp);
    if(ImGui::IsItemHovered() && helpText.length() > 0)
        ImGui::SetTooltip("%s", helpText.c_str());

    changed = oldVal != val;
    oldVal = val;
}

bool32 EnumSetting::Value() const
{
    return val;
}

void EnumSetting::SetValue(uint32 newVal)
{
    val = std::min(newVal, numValues - 1);
}

void EnumSetting::ClampNumValues(uint32 num)
{
    Assert_(num <= numValues);
    numValuesClamp = num;
}

EnumSetting::operator uint32()
{
    return val;
}

// == DirectionSetting ============================================================================

DirectionSetting::DirectionSetting()
{
}

void DirectionSetting::Initialize(const char* name_, const char* group_, const char* label_,
                                  const char* helpText_, Float3 initialVal, bool convertToViewSpace_)
{
    val = Float3::Normalize(initialVal);
    oldVal = val;
    spherical = CartesianToSpherical(val);
    convertToViewSpace = convertToViewSpace_;
    Setting::Initialize(SettingType::Direction, &val, name_, group_, label_, helpText_);

    buttonName = name + "_CanvasButton";
    childName = name + "_Child";
}

void DirectionSetting::Update(const Float4x4& viewMatrix)
{
    const float WidgetSize = 75.0f;

    ImVec2 textSize = ImGui::CalcTextSize(label.c_str());

    ImGui::BeginChild(childName.c_str(), ImVec2(0.0f, WidgetSize + textSize.y * 4.5f), true);
    ImGui::BeginGroup();

    ImGui::Text(label.c_str());

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, WidgetSize + 35.0f);

    ImGui::InvisibleButton(buttonName.c_str(), ImVec2(WidgetSize, WidgetSize));

    if(ImGui::IsItemActive())
    {
        Float2 dragDelta = ToFloat2(ImGui::GetMouseDragDelta());
        Float2 rotAmt = dragDelta;

        if(wasDragged)
            rotAmt -= lastDragDelta;

        rotAmt *= 0.01f;
        Float3x3 rotation = Float3x3::RotationEuler(rotAmt.y, rotAmt.x, 0.0f);        
        if(convertToViewSpace)
        {
            Float3 dirVS = Float3::TransformDirection(val, viewMatrix);
            dirVS = Float3::Transform(dirVS, rotation);
            val = Float3::TransformDirection(dirVS, Float4x4::Transpose(viewMatrix));
        }
        else
            val = Float3::Transform(val, rotation);

        spherical = CartesianToSpherical(val);

        wasDragged = true;
        lastDragDelta = dragDelta;
    }
    else
        wasDragged = false;

    Float2 canvasStart = ToFloat2(ImGui::GetItemRectMin());
    Float2 canvasSize = ToFloat2(ImGui::GetItemRectSize());
    Float2 canvasEnd = ToFloat2(ImGui::GetItemRectMax());

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddCircleFilled(ToImVec2(canvasStart + WidgetSize * 0.5f), WidgetSize * 0.5f,  ImColor(0.5f, 0.5f, 0.5f, 0.5f), 32);

    Float3 drawDir = val;
    if(convertToViewSpace)
        drawDir = Float3::TransformDirection(val, viewMatrix);

    Float3x3 basis;
    basis.SetZBasis(drawDir);

    Float3 up = Float3(0.0f, 1.0f, 0.0f);
    if(std::abs(drawDir.y) >= 0.999f)
        up = Float3(0.0f, 0.0f, -1.0f);

    basis.SetXBasis(Float3::Normalize(Float3::Cross(up, drawDir)));
    basis.SetYBasis(Float3::Cross(drawDir, basis.Right()));

    const float arrowHeadSize = 0.2f;
    Float3 arrowHeadPoints[] =
    {
        Float3(0.0f, 0.0f, 1.0f) + Float3(1.0f, 0.0f, -1.0f) * arrowHeadSize,
        Float3(0.0f, 0.0f, 1.0f) + Float3(0.0f, 1.0f, -1.0f) * arrowHeadSize,
        Float3(0.0f, 0.0f, 1.0f) + Float3(-1.0f, 0.0f, -1.0f) * arrowHeadSize,
        Float3(0.0f, 0.0f, 1.0f) + Float3(0.0f, -1.0f, -1.0f) * arrowHeadSize,
    };

    Float2 startPoint = CanvasTransform(0.0f, canvasStart, canvasSize);
    ImColor color = ImColor(1.0f, 1.0f, 0.0f);

    Float2 endPoint = CanvasTransform(drawDir.To2D(), canvasStart, canvasSize);
    drawList->AddLine(ToImVec2(startPoint), ToImVec2(endPoint), color);

    for(uint64 i = 0; i < ArraySize_(arrowHeadPoints); ++i)
    {
        Float2 headPoint = Float3::Transform(arrowHeadPoints[i], basis).To2D();
        headPoint = CanvasTransform(headPoint, canvasStart, canvasSize);
        drawList->AddLine(ToImVec2(headPoint), ToImVec2(endPoint), color);

        uint64 nextHeadIdx = (i + 1) % ArraySize_(arrowHeadPoints);
        Float2 nextHeadPoint = Float3::Transform(arrowHeadPoints[nextHeadIdx], basis).To2D();
        nextHeadPoint = CanvasTransform(nextHeadPoint, canvasStart, canvasSize);
        drawList->AddLine(ToImVec2(headPoint), ToImVec2(nextHeadPoint), color);
    }

    ImGui::NextColumn();

    if(inputMode == DirectionInputMode::Cartesian)
    {
        ImGui::SliderFloat("x", &val.x, -1.0f, 1.0f);
        ImGui::SliderFloat("y", &val.y, -1.0f, 1.0f);
        ImGui::SliderFloat("z", &val.z, -1.0f, 1.0f);
        spherical = CartesianToSpherical(val);
    }
    else if(inputMode == DirectionInputMode::Spherical)
    {
        Float2 degrees = Float2(RadToDeg(spherical.x), RadToDeg(spherical.y));
        bool sliderChanged = ImGui::SliderFloat("azimuth", &degrees.x, 0.0f, 360.0f);
        sliderChanged |= ImGui::SliderFloat("elevation", &degrees.y, -90.0f, 90.0f);
        if(sliderChanged)
        {
            spherical = Float2(DegToRad(degrees.x), DegToRad(degrees.y));        
            val = SphericalToCartesian(spherical.x, spherical.y);
        }
    }

    bool cartesianButton = ImGui::RadioButton("Cartesian", inputMode == DirectionInputMode::Cartesian);
    ImGui::SameLine();
    bool sphericalButton = ImGui::RadioButton("Spherical", inputMode == DirectionInputMode::Spherical);

    if(cartesianButton)
        inputMode = DirectionInputMode::Cartesian;
    else if(sphericalButton)
        inputMode = DirectionInputMode::Spherical;

    ImGui::EndGroup();
    if(ImGui::IsItemHovered() && helpText.length() > 0)
        ImGui::SetTooltip("%s", helpText.c_str());

    ImGui::EndChild();

    val = Float3::Normalize(val);
    changed = oldVal != val;
    oldVal = val;
}

Float3 DirectionSetting::Value() const
{
    return val;
}

void DirectionSetting::SetValue(Float3 newVal)
{
    val = Float3::Normalize(newVal);
}

DirectionSetting::operator Float3()
{
    return val;
}

// == OrientationSetting ==========================================================================

OrientationSetting::OrientationSetting()
{
}

void OrientationSetting::Initialize(const char* name_, const char* group_, const char* label_, const char* helpText_,
                                    Quaternion initialVal, bool convertToViewSpace_)
{
    val = Quaternion::Normalize(initialVal);
    oldVal = val;
    convertToViewSpace = convertToViewSpace_;

    Setting::Initialize(SettingType::Orientation, &val, name_, group_, label_, helpText_);

    buttonName = name + "_CanvasButton";
    childName = name + "_Child";
}

void OrientationSetting::Update(const Float4x4& viewMatrix)
{
    static const float WidgetSize = 75.0f;

    ImVec2 textSize = ImGui::CalcTextSize(label.c_str());

    ImGui::BeginChild(childName.c_str(), ImVec2(0.0f, WidgetSize + textSize.y * 2.5f), true);

    ImGui::BeginGroup();

    ImGui::Text(label.c_str());

    ImGui::InvisibleButton(buttonName.c_str(), ImVec2(WidgetSize, WidgetSize));

    if(ImGui::IsItemActive())
    {
        Float2 dragDelta = ToFloat2(ImGui::GetMouseDragDelta());
        Float2 rotAmt = dragDelta;

        if(wasDragged)
            rotAmt -= lastDragDelta;

        rotAmt *= 0.01f;
        Quaternion rotation = Quaternion::FromEuler(rotAmt.y, rotAmt.x, 0.0f);
        if(convertToViewSpace)
        {
            Quaternion orientationVS = val * Quaternion(viewMatrix.To3x3());
            orientationVS *= rotation;
            val = orientationVS * Quaternion(Float3x3::Transpose(viewMatrix.To3x3()));
        }
        else
            val *= rotation;

        wasDragged = true;
        lastDragDelta = dragDelta;
    }
    else
        wasDragged = false;

    Float2 canvasStart = ToFloat2(ImGui::GetItemRectMin());
    Float2 canvasSize = ToFloat2(ImGui::GetItemRectSize());
    Float2 canvasEnd = ToFloat2(ImGui::GetItemRectMax());

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddCircleFilled(ToImVec2(canvasStart + WidgetSize * 0.5f), WidgetSize * 0.5f,  ImColor(0.5f, 0.5f, 0.5f, 0.5f), 32);

    Float3x3 basis = val.ToFloat3x3();
    if(convertToViewSpace)
        basis *= viewMatrix.To3x3();

    Float2 startPoint = CanvasTransform(0.0f, canvasStart, canvasSize);

    Float2 endPoints[3] = { basis.Right().To2D(), basis.Up().To2D(), basis.Forward().To2D() };
    Float3 colors[3] = { Float3(1.0f, 0.0, 0.0f), Float3(0.0f, 1.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f) };

    for(uint64 i = 0; i < 3; ++i)
    {
        Float2 endPoint = CanvasTransform(endPoints[i], canvasStart, canvasSize);
        drawList->AddLine(ToImVec2(startPoint), ToImVec2(endPoint), ToImColor(colors[i]));
    }

    ImGui::SameLine();
    ImGui::InputFloat4("xyzw", &val.x, 3);

    ImGui::EndGroup();
    if(ImGui::IsItemHovered() && helpText.length() > 0)
        ImGui::SetTooltip("%s", helpText.c_str());

    ImGui::EndChild();

    val = Quaternion::Normalize(val);

    changed = oldVal != val;
    oldVal = val;
}

Quaternion OrientationSetting::Value() const
{
    return val;
}

void OrientationSetting::SetValue(Quaternion newVal)
{
    val = Quaternion::Normalize(newVal);
}

OrientationSetting::operator Quaternion()
{
    return val;
}

// == ColorSetting ================================================================================

static std::string GetIntensityLabel(const char* colorLabelName, ColorUnit units)
{
    std::string intensityLabel = colorLabelName;
    intensityLabel += " Intensity";
    if(units == ColorUnit::Luminance)
        intensityLabel += " (cd/m^2)";
    else if(units == ColorUnit::Illuminance)
        intensityLabel += " (lux)";
    else if(units == ColorUnit::LuminousPower)
        intensityLabel += " (lm)";
    else if(units == ColorUnit::EV100)
        intensityLabel += " (EV100)";

    return intensityLabel;
}

ColorSetting::ColorSetting()
{
}

void ColorSetting::Initialize(const char* name_,
                              const char* group_, const char* label_,
                              const char* helpText_, Float3 initialVal,
                              bool hdr_, float minIntensity_, float maxIntensity_,
                              float step_, ColorUnit units_)
{
    hdr = hdr_;
    float initialIntensity = 1.0f;
    if(hdr)
    {
        val = Float3::Clamp(initialVal, 0.0f, FLT_MAX);
        initialIntensity = std::max(std::max(val.x, val.y), val.z);
        val /= initialIntensity;
        initialIntensity = Clamp(initialIntensity, minIntensity_, maxIntensity_);
        units = units_;
    }
    else
        val = Float3::Clamp(initialVal, 0.0f, 1.0f);

    oldVal = val * initialIntensity;
    Setting::Initialize(SettingType::Color, &val, name_, group_, label_, helpText_);

    if(hdr)
    {
        std::string intensityName = name_;
        intensityName += "Intensity";
        std::string intensityLabel = GetIntensityLabel(label_, units_);
        std::string intensityHelp = "The intensity of the ";
        intensityHelp += name_;
        intensityHelp += " setting";
        intensity.Initialize(intensityName.c_str(), group_,
                             intensityLabel.c_str(), intensityHelp.c_str(),
                             initialIntensity, minIntensity_, maxIntensity_, step_,
                             ConversionMode::None, 1.0f);
    }
}

void ColorSetting::SetReadOnly(bool readOnly)
{
    Setting::SetReadOnly(readOnly);
    if(hdr)
        intensity.SetReadOnly(readOnly);
}


void ColorSetting::Update(const Float4x4& viewMatrix)
{
    ImGui::ColorEdit3(label.c_str(), reinterpret_cast<float*>(&val));
    if(ImGui::IsItemHovered() && helpText.length() > 0)
        ImGui::SetTooltip("%s", helpText.c_str());

    float multiplier = 1.0f;
    if(hdr)
    {
        intensity.Update(viewMatrix);

        multiplier = intensity.Value();
        if(units != ColorUnit::None)
        {
            float clrLum = ComputeLuminance(val);
            if(clrLum > 0.00001f)
                multiplier *= 1.0f / clrLum;
        }
    }
    Float3 newVal = val * multiplier;
    changed = oldVal != newVal;
    oldVal = newVal;
}

Float3 ColorSetting::Value() const
{
    return oldVal;
}

void ColorSetting::SetValue(Float3 newVal)
{
    if(hdr)
    {
        newVal = Float3::Clamp(newVal, 0.0f, FLT_MAX);
        float newIntensity = std::max(std::max(newVal.x, newVal.y), newVal.z);
        newVal /= newIntensity;
        intensity.SetValue(newIntensity);
    }

    val = Float3::Clamp(newVal, 0.0f, 1.0f);
}

ColorSetting::operator Float3()
{
    return Value();
}

float ColorSetting::Intensity() const
{
    if(hdr)
        return intensity.Value();
    else
        return 1.0f;
}

void ColorSetting::SetIntensity(float newIntensity)
{
    if(hdr)
    {
        intensity.SetValue(newIntensity);
        Update(Float4x4());
    }
}

void ColorSetting::SetIntensityVisible(bool visible_)
{
    if(hdr)
        intensity.SetVisible(visible_);
}

void ColorSetting::SetUnits(ColorUnit newUnits)
{
    if(hdr && newUnits != units)
    {
        units = newUnits;
        std::string intensityLabel = GetIntensityLabel(label.c_str(), units);
        intensity.SetLabel(intensityLabel.c_str());
    }
}

// == Button ======================================================================================

Button::Button() : wasPressed(false), currentlyPressed(false)
{
}

void Button::Initialize(const char* name_, const char* group_,
                        const char* label_, const char* helpText_)
{
    type = SettingType::Button;
    name = name_;
    group = group_;
    label = label_;
    helpText = helpText_;
    changed = false;
}

void Button::Update(const Float4x4& viewMatrix)
{
    wasPressed = ImGui::Button(label.c_str());

    currentlyPressed = wasPressed;
    wasPressed = false;
}

// == SettingsContainer ===========================================================================

SettingsContainer::SettingsContainer()
{
}

SettingsContainer::~SettingsContainer()
{
}

void SettingsContainer::Initialize(uint64 numGroups)
{
    groups.Init(numGroups);
    initialized = true;
}

void SettingsContainer::Update(uint32 displayWidth, uint32 displayHeight, const Float4x4& viewMatrix)
{
    if(opened == false)
    {
        ImGui::SetNextWindowSize(ImVec2(75.0f, 25.0f));
        ImGui::SetNextWindowPos(ImVec2(displayWidth - 85.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
        if(ImGui::Begin("settings_button", nullptr, ImVec2(75.0f, 25.0f), 0.0f, flags))
        {
            if(ImGui::Button("Settings"))
                opened = true;
        }

        ImGui::PopStyleVar();

        ImGui::End();

        return;
    }

    ImVec2 initialSize = ImVec2(200.0f, float(displayHeight) * 0.75f);
    ImGui::SetNextWindowSize(initialSize, ImGuiSetCond_FirstUseEver);

    if(ImGui::Begin("Application Settings", &opened) == false)
    {
        ImGui::End();
        return;
    }

    for(uint64 groupIdx = 0; groupIdx < groups.Count(); ++groupIdx)
    {
        SettingsGroup& group = groups[groupIdx];
        if(ImGui::CollapsingHeader(group.Name.c_str(), nullptr, group.Expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0) == false)
            continue;

        for(uint64 settingIdx = 0; settingIdx < group.Settings.Count(); ++settingIdx)
        {
            Setting* setting = group.Settings[settingIdx];
            if(setting->Visible())
                setting->Update(viewMatrix);
        }
    }

    ImGui::End();
}

Setting* SettingsContainer::FindSetting(const char* name)
{
    for(uint64 groupIdx = 0; groupIdx < groups.Count(); ++groupIdx)
    {
        SettingsGroup& group = groups[groupIdx];
        for(uint64 settingIdx = 0; settingIdx < group.Settings.Count(); ++settingIdx)
        {
            if(group.Settings[settingIdx]->Name() == name)
                return group.Settings[settingIdx];
        }
    }

    return nullptr;
}

void SettingsContainer::AddGroup(const char* name, bool expanded)
{
    for(uint64 groupIdx = 0; groupIdx < groups.Count(); ++groupIdx)
        AssertMsg_(groups[groupIdx].Name != name, "Duplicate settings group %s", name);

    SettingsGroup& newGroup = groups.Add();
    newGroup.Name = name;
    newGroup.Expanded = expanded;
}


void SettingsContainer::AddSetting(Setting* setting)
{
    Assert_(initialized);
    Assert_(setting != nullptr);
    Assert_(FindSetting(setting->Name().c_str()) == nullptr);

    for(uint64 groupIdx = 0; groupIdx < groups.Count(); ++groupIdx)
    {
        SettingsGroup& group = groups[groupIdx];
        if(group.Name == setting->Group())
        {
            group.Settings.Add(setting);
            return;
        }
    }

    AssertFail_("Tried to add setting '%s' to non-existent group '%s'",
                setting->Name().c_str(), setting->Group().c_str());
}

}