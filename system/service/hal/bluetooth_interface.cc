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

#include "service/hal/bluetooth_interface.h"

#include <base/logging.h>
#include <base/observer_list.h>
#include <dlfcn.h>

#include <mutex>
#include <shared_mutex>

#include "abstract_observer_list.h"
#include "btcore/include/hal_util.h"
#include "service/logging_helpers.h"
#include "types/raw_address.h"

using std::lock_guard;
using std::unique_lock;
using std::shared_lock;
using std::mutex;
#if defined(OS_GENERIC) && defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION < 3500)
using shared_mutex_impl = std::shared_mutex;
#else
using shared_mutex_impl = std::shared_timed_mutex;
#endif

#ifndef DYNAMIC_LOAD_BLUETOOTH
extern bt_interface_t bluetoothInterface;
#endif

namespace bluetooth {
namespace hal {

namespace {

// The global BluetoothInterface instance.
BluetoothInterface* g_bluetooth_interface = nullptr;

// Mutex used by callbacks to access |g_interface|. If we initialize or clean it
// use unique_lock. If only accessing |g_interface| use shared lock.
// TODO(jpawlowski): this should be just shared_mutex, as we currently don't use
// timed methods. Change to shared_mutex when we upgrade to C++14
shared_mutex_impl g_instance_lock;

// Helper for obtaining the observer list. This is forward declared here and
// defined below since it depends on BluetoothInterfaceImpl.
btbase::AbstractObserverList<BluetoothInterface::Observer>* GetObservers();

#define FOR_EACH_BLUETOOTH_OBSERVER(func)  \
  for (auto& observer : *GetObservers()) { \
    observer.func;                         \
  }

#define VERIFY_INTERFACE_OR_RETURN()                                   \
  do {                                                                 \
    if (!g_bluetooth_interface) {                                      \
      LOG(WARNING) << "Callback received while |g_interface| is NULL"; \
      return;                                                          \
    }                                                                  \
  } while (0)

void AdapterStateChangedCallback(bt_state_t state) {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(1) << "Adapter state changed: " << BtStateText(state);
  FOR_EACH_BLUETOOTH_OBSERVER(AdapterStateChangedCallback(state));
}

void AdapterPropertiesCallback(bt_status_t status, int num_properties,
                               bt_property_t* properties) {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(1) << "Adapter properties changed - status: " << BtStatusText(status)
          << ", num_properties: " << num_properties;
  FOR_EACH_BLUETOOTH_OBSERVER(
      AdapterPropertiesCallback(status, num_properties, properties));
}

void RemoteDevicePropertiesCallback(bt_status_t status,
                                    RawAddress* remote_bd_addr,
                                    int num_properties,
                                    bt_property_t* properties) {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(1) << " Remote device properties changed - status: "
          << BtStatusText(status)
          << " - BD_ADDR: " << BtAddrString(remote_bd_addr)
          << ", num_properties: " << num_properties;
  FOR_EACH_BLUETOOTH_OBSERVER(RemoteDevicePropertiesCallback(
      status, remote_bd_addr, num_properties, properties));
}

void DeviceFoundCallback(int num_properties, bt_property_t* properties) {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(1) << " Device found.";
  FOR_EACH_BLUETOOTH_OBSERVER(DeviceFoundCallback(num_properties, properties));
}

void DiscoveryStateChangedCallback(bt_discovery_state_t state) {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(1) << "Discovery state changed - state: " << BtDiscoveryStateText(state);
  FOR_EACH_BLUETOOTH_OBSERVER(DiscoveryStateChangedCallback(state));
}

void PinRequestCallback(RawAddress* remote_bd_addr, bt_bdname_t* bd_name,
                        uint32_t cod, bool min_16_digit) {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(2) << __func__ << " - remote_bd_addr: " << remote_bd_addr
          << " - bd_name: " << bd_name << " - cod: " << cod
          << " - min_16_digit: " << min_16_digit;
  FOR_EACH_BLUETOOTH_OBSERVER(
      PinRequestCallback(remote_bd_addr, bd_name, cod, min_16_digit));
}

void SSPRequestCallback(RawAddress* remote_bd_addr, bt_bdname_t* bd_name,
                        uint32_t cod, bt_ssp_variant_t pairing_variant,
                        uint32_t pass_key) {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(2) << __func__ << " - remote_bd_addr: " << remote_bd_addr
          << " - bd_name: " << bd_name << " - cod: " << cod
          << " - pairing_variant: " << pairing_variant;
  FOR_EACH_BLUETOOTH_OBSERVER(SSPRequestCallback(remote_bd_addr, bd_name, cod,
                                                 pairing_variant, pass_key));
}

void BondStateChangedCallback(bt_status_t status, RawAddress* remote_bd_addr,
                              bt_bond_state_t state, int fail_reason) {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(2) << __func__ << " - remote_bd_addr: " << BtAddrString(remote_bd_addr)
          << " - status: " << status << " - state: " << state;
  FOR_EACH_BLUETOOTH_OBSERVER(
      BondStateChangedCallback(status, remote_bd_addr, state, fail_reason));
}

void AddressConsolidateCallback(RawAddress* main_bd_addr,
                                RawAddress* secondary_bd_addr) {
  // Do nothing
}

void LeAddressAssociateCallback(RawAddress* main_bd_addr,
                                RawAddress* secondary_bd_addr) {
  // Do nothing
}

void AclStateChangedCallback(bt_status_t status, RawAddress* remote_bd_addr,
                             bt_acl_state_t state, int transport_link_type,
                             bt_hci_error_code_t hci_reason,
                             bt_conn_direction_t direction,
                             uint16_t acl_handle) {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(remote_bd_addr);
  VLOG(1) << "Remote device ACL state changed - status: "
          << BtStatusText(status)
          << " - BD_ADDR: " << BtAddrString(remote_bd_addr) << " - state: "
          << ((state == BT_ACL_STATE_CONNECTED) ? "CONNECTED" : "DISCONNECTED")
          << " - HCI_REASON: " << std::to_string(hci_reason);
  FOR_EACH_BLUETOOTH_OBSERVER(AclStateChangedCallback(
      status, *remote_bd_addr, state, transport_link_type, hci_reason,
      direction, acl_handle));
}

void ThreadEventCallback(bt_cb_thread_evt evt) {
  VLOG(1) << "ThreadEventCallback" << BtEventText(evt);

  // TODO(armansito): This callback is completely useless to us but btif borks
  // out if this is not set. Consider making this optional.
}

bool SetWakeAlarmCallout(uint64_t /* delay_millis */, bool /* should_wake */,
                         alarm_cb /* cb */, void* /* data */) {
  // TODO(armansito): According to sharvil@, this interface doesn't even need to
  // exist and can be done entirely from within osi by interfacing directly with
  // the kernel. Remove these stubs once that's fixed. (See http://b/23390297)
  return false;
}

int AcquireWakeLockCallout(const char* /* lock_name */) {
  // TODO(armansito): According to sharvil@, this interface doesn't even need to
  // exist and can be done entirely from within osi by interfacing directly with
  // the kernel. Remove these stubs once that's fixed. (See http://b/23390297)
  // Lie here and return success so that enabling and disabling the controller
  // works before this is properly implemented.
  return BT_STATUS_SUCCESS;
}

int ReleaseWakeLockCallout(const char* /* lock_name */) {
  // TODO(armansito): According to sharvil@, this interface doesn't even need to
  // exist and can be done entirely from within osi by interfacing directly with
  // the kernel. Remove these stubs once that's fixed. (See http://b/23390297)
  // Lie here and return success so that enabling and disabling the controller
  // works before this is properly implemented.
  return BT_STATUS_SUCCESS;
}

void LinkQualityReportCallback(uint64_t timestamp, int report_id, int rssi,
    int snr, int retransmission_count, int packets_not_receive_count,
    int negative_acknowledgement_count) {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  LOG(WARNING) << __func__ << " - timestamp: " << timestamp
               << " - report_id: " << report_id << " - rssi: " << rssi
               << " - snr: " << snr
               << " - retransmission_count: " << retransmission_count
               << " - packets_not_receive_count: " << packets_not_receive_count
               << " - negative_acknowledgement_count: "
               << negative_acknowledgement_count;
  FOR_EACH_BLUETOOTH_OBSERVER(LinkQualityReportCallback(
      timestamp, report_id, rssi, snr, retransmission_count,
      packets_not_receive_count, negative_acknowledgement_count));
}

void SwitchBufferSizeCallback(bool is_low_latency_buffer_size) {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  LOG(WARNING) << __func__ << " - is_low_latency_buffer_size: "
               << is_low_latency_buffer_size;
  FOR_EACH_BLUETOOTH_OBSERVER(
      SwitchBufferSizeCallback(is_low_latency_buffer_size));
}

void SwitchCodecCallback(bool is_low_latency_buffer_size) {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  LOG(WARNING) << __func__ << " - is_low_latency_buffer_size: "
               << is_low_latency_buffer_size;
  FOR_EACH_BLUETOOTH_OBSERVER(SwitchCodecCallback(is_low_latency_buffer_size));
}

// The HAL Bluetooth DM callbacks.
bt_callbacks_t bt_callbacks = {
    sizeof(bt_callbacks_t),
    AdapterStateChangedCallback,
    AdapterPropertiesCallback,
    RemoteDevicePropertiesCallback,
    DeviceFoundCallback,
    DiscoveryStateChangedCallback,
    PinRequestCallback,
    SSPRequestCallback,
    BondStateChangedCallback,
    AddressConsolidateCallback,
    LeAddressAssociateCallback,
    AclStateChangedCallback,
    ThreadEventCallback,
    nullptr, /* dut_mode_recv_cb */
    nullptr, /* energy_info_cb */
    LinkQualityReportCallback,
    nullptr /* generate_local_oob_data_cb */,
    SwitchBufferSizeCallback,
    SwitchCodecCallback,
    nullptr /* le_rand_cb */,
};

bt_os_callouts_t bt_os_callouts = {sizeof(bt_os_callouts_t),
                                   SetWakeAlarmCallout, AcquireWakeLockCallout,
                                   ReleaseWakeLockCallout};

constexpr char kLibbluetooth[] = "libbluetooth.so";
constexpr char kBluetoothInterfaceSym[] = "bluetoothInterface";

int hal_util_load_bt_library_from_dlib(const bt_interface_t** interface) {
#ifndef DYNAMIC_LOAD_BLUETOOTH
  (void)kLibbluetooth;
  (void)kBluetoothInterfaceSym;
  *interface = &bluetoothInterface;
  return 0;
#else
  bt_interface_t* itf{nullptr};

  // Always try to load the default Bluetooth stack on GN builds.
  void* handle = dlopen(kLibbluetooth, RTLD_NOW);
  if (!handle) {
    const char* err_str = dlerror();
    LOG(ERROR) << __func__ << ": failed to load bluetooth library, error="
               << (err_str ? err_str : "error unknown");
    goto error;
  }

  // Get the address of the bt_interface_t.
  itf = (bt_interface_t*)dlsym(handle, kBluetoothInterfaceSym);
  if (!itf) {
    LOG(ERROR) << __func__ << ": failed to load symbol from Bluetooth library "
               << kBluetoothInterfaceSym;
    goto error;
  }

  // Success.
  LOG(INFO) << __func__ << " loaded HAL path=" << kLibbluetooth
            << " btinterface=" << itf << " handle=" << handle;

  *interface = itf;
  return 0;

error:
  *interface = NULL;
  if (handle) dlclose(handle);

  return -EINVAL;
#endif
}

}  // namespace

// BluetoothInterface implementation for production.
class BluetoothInterfaceImpl : public BluetoothInterface {
 public:
  BluetoothInterfaceImpl() : hal_iface_(nullptr) {}

  BluetoothInterfaceImpl(const BluetoothInterfaceImpl&) = delete;
  BluetoothInterfaceImpl& operator=(const BluetoothInterfaceImpl&) = delete;

  ~BluetoothInterfaceImpl() override {
    if (hal_iface_) hal_iface_->cleanup();
  }

  // BluetoothInterface overrides.
  void AddObserver(Observer* observer) override {
    shared_lock<shared_mutex_impl> lock(g_instance_lock);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    shared_lock<shared_mutex_impl> lock(g_instance_lock);
    observers_.RemoveObserver(observer);
  }

  const bt_interface_t* GetHALInterface() const override { return hal_iface_; }

  bt_callbacks_t* GetHALCallbacks() const override { return &bt_callbacks; }

  // Initialize the interface. This loads the shared Bluetooth library and sets
  // up the callbacks.
  bool Initialize() {
    // Load the Bluetooth shared library module.
    const bt_interface_t* interface;
    int status = hal_util_load_bt_library_from_dlib(&interface);
    if (status) {
      LOG(ERROR) << "Failed to open the Bluetooth module";
      return false;
    }

    hal_iface_ = interface;

    // Initialize the Bluetooth interface. Set up the adapter (Bluetooth DM) API
    // callbacks.
    status = hal_iface_->init(&bt_callbacks, false, false, 0, nullptr, false,
                              nullptr);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed to initialize Bluetooth stack";
      return false;
    }

    status = hal_iface_->set_os_callouts(&bt_os_callouts);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed to set up Bluetooth OS callouts";
      return false;
    }

    return true;
  }

  btbase::AbstractObserverList<Observer>* observers() { return &observers_; }

 private:
  // List of observers that are interested in notifications from us. We're not
  // using a base::ObserverListThreadSafe, which it posts observer events
  // automatically on the origin threads, as we want to avoid that overhead and
  // simply forward the events to the upper layer.
  btbase::AbstractObserverList<Observer> observers_;

  // The HAL handle obtained from the shared library. We hold a weak reference
  // to this since the actual data resides in the shared Bluetooth library.
  const bt_interface_t* hal_iface_;
};

namespace {

// Helper for obtaining the observer list from the global instance. This
// function is NOT thread safe.
btbase::AbstractObserverList<BluetoothInterface::Observer>* GetObservers() {
  CHECK(g_bluetooth_interface);
  return static_cast<BluetoothInterfaceImpl*>(g_bluetooth_interface)
      ->observers();
}

}  // namespace

// Default observer implementations. These are provided so that the methods
// themselves are optional.
void BluetoothInterface::Observer::AdapterStateChangedCallback(
    bt_state_t /* state*/) {
  // Do nothing.
}

void BluetoothInterface::Observer::AdapterPropertiesCallback(
    bt_status_t /* status */, int /* num_properties */,
    bt_property_t* /* properties */) {
  // Do nothing.
}

void BluetoothInterface::Observer::RemoteDevicePropertiesCallback(
    bt_status_t /* status */, RawAddress* /* remote_bd_addr */,
    int /* num_properties */, bt_property_t* /* properties */) {
  // Do nothing.
}

void BluetoothInterface::Observer::DeviceFoundCallback(
    int /* num_properties */, bt_property_t* /* properties */) {
  // Do nothing.
}

void BluetoothInterface::Observer::DiscoveryStateChangedCallback(
    bt_discovery_state_t /* state */) {
  // Do nothing.
}

void BluetoothInterface::Observer::PinRequestCallback(
    RawAddress* remote_bd_addr, bt_bdname_t* bd_name, uint32_t cod,
    bool min_16_digit) {
  // Do nothing.
}

void BluetoothInterface::Observer::SSPRequestCallback(
    RawAddress* remote_bd_addr, bt_bdname_t* bd_name, uint32_t cod,
    bt_ssp_variant_t pairing_variant, uint32_t pass_key) {
  // Do nothing.
}

void BluetoothInterface::Observer::BondStateChangedCallback(
    bt_status_t status, RawAddress* remote_bd_addr, bt_bond_state_t state,
    int fail_reason) {
  // Do nothing.
}

void BluetoothInterface::Observer::AclStateChangedCallback(
    bt_status_t /* status */, const RawAddress& /* remote_bdaddr */,
    bt_acl_state_t /* state */, int /* transport_link_type */,
    bt_hci_error_code_t /* hci_reason */, bt_conn_direction_t /* direction */,
    uint16_t /* acl_handle*/) {
  // Do nothing.
}

void BluetoothInterface::Observer::LinkQualityReportCallback(
    uint64_t /* timestamp */, int /* report_id */, int /* rssi */,
    int /* snr */, int /* retransmission_count */,
    int /* packets_not_receive_count */,
    int /* negative_acknowledgement_count */) {
  // Do nothing.
}

void BluetoothInterface::Observer::SwitchBufferSizeCallback(
    bool /* is_low_latency_buffer_size */) {
  // Do nothing.
}

void BluetoothInterface::Observer::SwitchCodecCallback(
    bool /* is_low_latency_buffer_size */) {
  // Do nothing.
}

// static
bool BluetoothInterface::Initialize() {
  unique_lock<shared_mutex_impl> lock(g_instance_lock);
  CHECK(!g_bluetooth_interface);

  std::unique_ptr<BluetoothInterfaceImpl> impl(new BluetoothInterfaceImpl());
  if (!impl->Initialize()) {
    LOG(ERROR) << "Failed to initialize BluetoothInterface";
    return false;
  }

  g_bluetooth_interface = impl.release();

  return true;
}

// static
void BluetoothInterface::CleanUp() {
  unique_lock<shared_mutex_impl> lock(g_instance_lock);
  CHECK(g_bluetooth_interface);

  delete g_bluetooth_interface;
  g_bluetooth_interface = nullptr;
}

// static
bool BluetoothInterface::IsInitialized() {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);

  return g_bluetooth_interface != nullptr;
}

// static
BluetoothInterface* BluetoothInterface::Get() {
  shared_lock<shared_mutex_impl> lock(g_instance_lock);
  CHECK(g_bluetooth_interface);
  return g_bluetooth_interface;
}

// static
void BluetoothInterface::InitializeForTesting(
    BluetoothInterface* test_instance) {
  unique_lock<shared_mutex_impl> lock(g_instance_lock);
  CHECK(test_instance);
  CHECK(!g_bluetooth_interface);

  g_bluetooth_interface = test_instance;
}

}  // namespace hal
}  // namespace bluetooth
