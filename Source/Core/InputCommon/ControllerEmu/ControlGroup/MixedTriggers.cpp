// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"

namespace ControllerEmu
{
MixedTriggers::MixedTriggers(const std::string& name_)
    : ControlGroup(name_, GroupType::MixedTriggers)
{
  AddDeadzoneSetting(&m_deadzone_setting, 25);

  AddSetting(&m_threshold_setting,
             {_trans("Threshold"),
              // i18n: The percent symbol.
              _trans("%"),
              // i18n: Refers to the "threshold" setting for pressure sensitive gamepad inputs.
              _trans("Input strength required for activation.")},
             90, 0, 100);
}

void MixedTriggers::GetState(u16* const digital, const u16* bitmasks, ControlState* analog,
                             bool adjusted) const
{
  const ControlState threshold = GetThreshold();
  ControlState deadzone = GetDeadzone();

  // Return raw values. (used in UI)
  if (!adjusted)
  {
<<<<<<< HEAD
#ifdef __LIBRETRO__
    // If the analog trigger is at 0 but the digital trigger is set, we assume there's no analog trigger.
    // Otherwise the analog trigger value takes precedence over the digital one.
    if (controls[i + trigger_count]->control_ref->State() > numeric_settings[0]->GetValue()  // analog trigger > threshold
        || (controls[i + trigger_count]->control_ref->State() == 0 && controls[i]->control_ref->State() > numeric_settings[0]->GetValue())) // digital trigger > threshold
    {
      *analog = 1.0;
      *digital |= *bitmasks;
    }
    else
    {
      *analog = controls[i + trigger_count]->control_ref->State();
    }
#else
    if (controls[i]->control_ref->State() > numeric_settings[0]->GetValue())  // threshold
    {
      *analog = 1.0;
      *digital |= *bitmasks;
    }
    else
=======
    deadzone = 0.0;
  }

  const int trigger_count = int(controls.size() / 2);
  for (int i = 0; i != trigger_count; ++i)
  {
    const ControlState button_value = ApplyDeadzone(controls[i]->control_ref->State(), deadzone);
    ControlState analog_value =
        ApplyDeadzone(controls[trigger_count + i]->control_ref->State(), deadzone);

    // Apply threshold:
    if (button_value > threshold)
>>>>>>> 1d5dd5db914d94f3f612c13c6c5e1d5e711b49b5
    {
      // Fully activate analog:
      analog_value = 1.0;

      // Activate button:
      *digital |= bitmasks[i];
    }
<<<<<<< HEAD
#endif
=======

    analog[i] = analog_value;
>>>>>>> 1d5dd5db914d94f3f612c13c6c5e1d5e711b49b5
  }
}

ControlState MixedTriggers::GetDeadzone() const
{
  return m_deadzone_setting.GetValue() / 100;
}

ControlState MixedTriggers::GetThreshold() const
{
  return m_threshold_setting.GetValue() / 100;
}

}  // namespace ControllerEmu
