#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/optional.h"
#include "esphome/core/log.h"

#include <CEC_Device.h>
#include <functional>

namespace esphome {
namespace hdmi_cec {

class HdmiCec;
class HdmiCecTrigger;
template<typename... Ts> class HdmiCecSendAction;

static const uint8_t HDMI_CEC_MAX_DATA_LENGTH = 16;

class HdmiCec : public Component, CEC_Device {
 public:
  HdmiCec(){};

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;

  bool LineState() override;
  void SetLineState(bool state) override;
  void OnReady(int logical_address) override;
  void OnReceiveComplete(unsigned char *buffer, int count, bool ack) override;
  void OnTransmitComplete(unsigned char *buffer, int count, bool ack) override;

  void send_data(uint8_t source, uint8_t destination, const std::vector<uint8_t> &data);

  void set_address(uint8_t address) { this->address_ = address; }
  void set_physical_address(uint16_t physical_address) { this->physical_address_ = physical_address; }
  void set_promiscuous_mode(bool promiscuous_mode) { this->promiscuous_mode_ = promiscuous_mode; }

  void set_pin(InternalGPIOPin *pin) {
    this->pin_ = pin;
    this->pin_->pin_mode(gpio::FLAG_INPUT);
  }

  void add_trigger(HdmiCecTrigger *trigger);
  static void pin_interrupt(HdmiCec *arg);

 protected:
  void send_data_internal_(uint8_t source, uint8_t destination, unsigned char *buffer, int count);

  template<typename... Ts> friend class HdmiCecSendAction;

  InternalGPIOPin *pin_;
  std::vector<HdmiCecTrigger *> triggers_{};

  uint8_t address_;
  uint16_t physical_address_;
  bool promiscuous_mode_;

  HighFrequencyLoopRequester high_freq_;
  volatile boolean disable_line_interrupts_ = false;
};

template<typename... Ts>
class HdmiCecSendAction : public Action<Ts...>, public Parented<HdmiCec> {
 public:
  void set_data_template(const std::function<std::vector<uint8_t>(Ts...)> &func) {
    this->data_func_ = func;
    this->data_static_.reset();
  }

  void set_data_static(const std::vector<uint8_t> &data) {
    this->data_static_ = data;
    this->data_func_ = nullptr;
  }

  void set_source(uint8_t source) {
    this->source_ = source;
    this->source_func_ = nullptr;
  }

  void set_source_template(const std::function<uint8_t(Ts...)> &func) {
    this->source_func_ = func;
    this->source_.reset();
  }

  void set_destination(uint8_t destination) {
    this->destination_ = destination;
    this->destination_func_ = nullptr;
  }

  void set_destination_template(const std::function<uint8_t(Ts...)> &func) {
    this->destination_func_ = func;
    this->destination_.reset();
  }

  void play(Ts... x) override {
    uint8_t source;
    if (this->source_func_) {
      source = this->source_func_(x...);
    } else if (this->source_.has_value()) {
      source = *this->source_;
    } else {
      source = this->parent_->address_;
    }

    uint8_t destination;
    if (this->destination_func_) {
      destination = this->destination_func_(x...);
    } else {
      destination = *this->destination_;
    }

    std::vector<uint8_t> data;
    if (this->data_func_) {
      data = this->data_func_(x...);
    } else {
      data = *this->data_static_;
    }

    this->parent_->send_data(source, destination, data);
  }

 protected:
  optional<uint8_t> source_{};
  optional<uint8_t> destination_{};

  std::function<uint8_t(Ts...)> source_func_{};
  std::function<uint8_t(Ts...)> destination_func_{};

  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
  optional<std::vector<uint8_t>> data_static_{};
};

class HdmiCecTrigger : public Trigger<uint8_t, uint8_t, std::vector<uint8_t>>, public Component {
 public:
  explicit HdmiCecTrigger(HdmiCec *parent) : parent_(parent) {}

  void setup() override { this->parent_->add_trigger(this); }

  void set_source(uint8_t source) { this->source_ = source; }
  void set_destination(uint8_t destination) { this->destination_ = destination; }
  void set_opcode(uint8_t opcode) { this->opcode_ = opcode; }
  void set_data(const std::vector<uint8_t> &data) { this->data_ = data; }

  // getters (IMPORTANT for cpp filtering)
  const optional<uint8_t> &get_source() const { return this->source_; }
  const optional<uint8_t> &get_destination() const { return this->destination_; }
  const optional<uint8_t> &get_opcode() const { return this->opcode_; }
  const optional<std::vector<uint8_t>> &get_data() const { return this->data_; }

 protected:
  HdmiCec *parent_;
  optional<uint8_t> source_{};
  optional<uint8_t> destination_{};
  optional<uint8_t> opcode_{};
  optional<std::vector<uint8_t>> data_{};
};

}  // namespace hdmi_cec
}  // namespace esphome

#endif
