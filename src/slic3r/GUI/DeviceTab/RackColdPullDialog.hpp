#pragma once
#include <memory>
class wxWindow;
namespace Slic3r {
class DevNozzleRack;
namespace GUI {
bool SupportsRackColdPull(const std::shared_ptr<DevNozzleRack>& rack);
void ShowRackColdPullDialog(wxWindow* parent, const std::shared_ptr<DevNozzleRack>& rack);
}
}
