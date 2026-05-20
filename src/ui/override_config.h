#pragma once

namespace ui {

// Number of days the device may keep operating after Wi-Fi/network becomes
// unavailable, before an override password is required (ToDo master task 1,
// State 2). Factory-adjustable: stored in a small persisted file so it can
// be changed without a rebuild, and also editable from Settings.
//
// Default: 7 days.
int  get_override_days();
bool set_override_days(int days);

} // namespace ui
