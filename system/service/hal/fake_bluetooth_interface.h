//
//  Copyright 2015 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#include <base/observer_list.h>

#include "abstract_observer_list.h"
#include "service/hal/bluetooth_interface.h"
#include "types/raw_address.h"

namespace bluetooth {
namespace hal {

class FakeBluetoothInterface : public BluetoothInterface {
 public:
  // A Fake HAL Bluetooth interface. This is kept as a global singleton as the
  // Bluetooth HAL doesn't support anything otherwise.
  //
  // TODO(armansito): Use an abstract "TestHandler" interface instead.
  struct Manager {
    Manager();
    ~Manager() = default;

    // Values that should be returned from bt_interface_t methods.
    bool enable_succeed;
    bool disable_succeed;
    bool set_property_succeed;
  };

  // Returns the global Manager.
  static Manager* GetManager();

  FakeBluetoothInterface() = default;
  FakeBluetoothInterface(const FakeBluetoothInterface&) = delete;
  FakeBluetoothInterface& operator=(const FakeBluetoothInterface&) = delete;

  ~FakeBluetoothInterface() override = default;

  // Notifies the observers that the adapter state changed to |state|.
  void NotifyAdapterStateChanged(bt_state_t state);

  // Triggers an adapter property change event.
  void NotifyAdapterPropertiesChanged(int num_properties,
                                      bt_property_t* properties);
  void NotifyAdapterNamePropertyChanged(const std::string& name);
  void NotifyAdapterAddressPropertyChanged(const RawAddress* address);
  void NotifyAdapterLocalLeFeaturesPropertyChanged(
      const bt_local_le_features_t* features);
  void NotifyAclStateChangedCallback(
      bt_status_t status, const RawAddress& remote_bdaddr, bt_acl_state_t state,
      int transport_link_type, bt_hci_error_code_t hci_reason,
      bt_conn_direction_t direction, uint16_t acl_handle);

  // hal::BluetoothInterface overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  const bt_interface_t* GetHALInterface() const override;
  bt_callbacks_t* GetHALCallbacks() const override;

 private:
  btbase::AbstractObserverList<Observer> observers_;
};

}  // namespace hal
}  // namespace bluetooth
