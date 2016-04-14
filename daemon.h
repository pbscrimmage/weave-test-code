// Copyright 2015 The Weave Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <weave/device.h>
#include <weave/error.h>

#include <base/bind.h>

#include "examples/provider/avahi_client.h"
#include "examples/provider/bluez_client.h"
#include "examples/provider/curl_http_client.h"
#include "examples/provider/event_http_server.h"
#include "examples/provider/event_network.h"
#include "examples/provider/event_task_runner.h"
#include "examples/provider/file_config_store.h"
#include "examples/provider/wifi_manager.h"

class Daemon {
 public:
  struct Options {
    bool force_bootstrapping{false};
    bool disable_privet{false};
    std::string registration_ticket;
    std::string model_id{"AAAAA"};
    std::string service_url;

    static void ShowUsage(const std::string& name) {
      LOG(ERROR) << "\nUsage: " << name << " <option(s)>"
                 << "\nOptions:\n"
                 << "\t-h,--help                    Show this help message\n"
                 << "\t--v=LEVEL                    Logging level\n"
                 << "\t-b,--bootstrapping           Force WiFi bootstrapping\n"
                 << "\t-r,--registration_ticket=TICKET Register device with "
                    "the given ticket\n"
                 << "\t-s,--staging                 Use staging server. Use "
                    "only with -r.\n"
                 << "\t--disable_privet             Disable local privet\n";
    }

    bool Parse(int argc, char** argv) {
      for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
          return false;
        } else if (arg == "-b" || arg == "--bootstrapping") {
          force_bootstrapping = true;
        } else if (arg == "-s" || arg == "--staging") {
          service_url =
              "https://www-googleapis-staging.sandbox.google.com/weave/v1/";
        } else if (arg == "--disable_privet") {
          disable_privet = true;
        } else if (arg.find("--registration_ticket=") != std::string::npos ||
                   arg.find("-r=") != std::string::npos) {
          auto pos = arg.find("=");
          if (pos == std::string::npos) {
            return false;
          }
          registration_ticket = arg.substr(pos + 1);
        } else if (arg.find("--v") != std::string::npos) {
          auto pos = arg.find("=");
          if (pos == std::string::npos) {
            return false;
          }
          logging::SetMinLogLevel(-std::stoi(arg.substr(pos + 1)));
        } else {
          return false;
        }
      }
      return true;
    }
  };

  Daemon(const Options& opts)
      : task_runner_{new weave::examples::EventTaskRunner},
        config_store_{new weave::examples::FileConfigStore(opts.model_id,
                                                           task_runner_.get())},
        http_client_{new weave::examples::CurlHttpClient(task_runner_.get())},
        network_{new weave::examples::EventNetworkImpl(task_runner_.get())},
        bluetooth_{new weave::examples::BluetoothImpl} {
    if (!opts.disable_privet) {
      network_->SetSimulateOffline(opts.force_bootstrapping);

      dns_sd_.reset(new weave::examples::AvahiClient);
      http_server_.reset(
          new weave::examples::HttpServerImpl{task_runner_.get()});
      if (weave::examples::WifiImpl::HasWifiCapability())
        wifi_.reset(
            new weave::examples::WifiImpl{task_runner_.get(), network_.get()});
    }
    device_ = weave::Device::Create(config_store_.get(), task_runner_.get(),
                                    http_client_.get(), network_.get(),
                                    dns_sd_.get(), http_server_.get(),
                                    wifi_.get(), bluetooth_.get());
    if (!opts.registration_ticket.empty()) {
      registration_data_.ticket_id = opts.registration_ticket;
      registration_data_.service_url = opts.service_url;
    }
  }

  void Run() {
    if (!registration_data_.ticket_id.empty()) {
      device_->Register(registration_data_, base::Bind(&OnRegisterDeviceDone, device_.get()));
    }
    task_runner_->Run();
  }

  weave::Device* GetDevice() const { return device_.get(); }

  weave::examples::EventTaskRunner* GetTaskRunner() const {
    return task_runner_.get();
  }

 private:
  static void OnRegisterDeviceDone(weave::Device* device,
                                   weave::ErrorPtr error) {
    CHECK(!error) << "Registration failed device: " << error->GetMessage();
    LOG(INFO) << "Device registered: " << device->GetSettings().cloud_id;
  }

  std::unique_ptr<weave::examples::EventTaskRunner> task_runner_;
  std::unique_ptr<weave::examples::FileConfigStore> config_store_;
  std::unique_ptr<weave::examples::CurlHttpClient> http_client_;
  std::unique_ptr<weave::examples::EventNetworkImpl> network_;
  std::unique_ptr<weave::examples::BluetoothImpl> bluetooth_;
  std::unique_ptr<weave::examples::AvahiClient> dns_sd_;
  std::unique_ptr<weave::examples::HttpServerImpl> http_server_;
  std::unique_ptr<weave::examples::WifiImpl> wifi_;
  std::unique_ptr<weave::Device> device_;
  weave::RegistrationData registration_data_;
};
