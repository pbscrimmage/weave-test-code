// Copyright 2015 The Weave Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "daemon.h"

#include <weave/device.h>
#include <weave/provider/task_runner.h>

#include <base/bind.h>
#include <base/memory/weak_ptr.h>

namespace {

const char kTraits[] = R"({
  "_weave_led": {
    "commands": {
      "hello": {
        "minimalRole": "user",
        "parameters": {
          "name": { "type": "string" }
        },
        "results": {
          "reply": { "type": "string" }
        }
      },
      "toggle": {
        "minimalRole": "user",
        "parameters": {}
      },
      "blink": {
        "minimalRole": "user",
        "parameters": {
          "seconds": {
            "type": "integer",
            "minimum": 1,
            "maximum": 25
          }
        }
      }
    },
    "state": {
      "led_status": { "type": "string" }
    }
  }
})";

const char kComponent[] = "sample";

}  // anonymous namespace

// SampleHandler is a command handler example.
// It implements the following commands:
// - _hello: handle a command with an argument and set its results.
// - _ping: update device state.
// - _countdown: handle long running command and report progress.
class SampleHandler {
 public:
  SampleHandler(weave::provider::TaskRunner* task_runner)
      : task_runner_{task_runner} {
          fp = fopen("/sys/class/gpio/gpio930/value","r+");
          if(!fp){
              LOG(INFO) << "ERROR: COULD NOT OPEN GPIO FILESTREAM";
          }
      }
  void Register(weave::Device* device) {
    device_ = device;

    device->AddTraitDefinitionsFromJson(kTraits);
    CHECK(device->AddComponent(kComponent, {"_sample"}, nullptr));
    CHECK(device->SetStatePropertiesFromJson(
        kComponent, R"({"_weave_led": {"led_status": 0}})", nullptr));

    device->AddCommandHandler(kComponent, "_weave_led.hello",
                              base::Bind(&SampleHandler::OnHelloCommand,
                                         weak_ptr_factory_.GetWeakPtr()));
    device->AddCommandHandler(kComponent, "_weave_led.toggle",
                              base::Bind(&SampleHandler::OnToggleCommand,
                                         weak_ptr_factory_.GetWeakPtr()));
    device->AddCommandHandler(kComponent, "_weave_led.blink",
                              base::Bind(&SampleHandler::OnBlinkCommand,
                                         weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnHelloCommand(const std::weak_ptr<weave::Command>& command) {
    auto cmd = command.lock();
    if (!cmd)
      return;
    LOG(INFO) << "received command: " << cmd->GetName();

    const auto& params = cmd->GetParameters();
    std::string name;
    if (!params.GetString("name", &name)) {
      weave::ErrorPtr error;
      weave::Error::AddTo(&error, FROM_HERE, "invalid_parameter_value",
                          "Name is missing");
      cmd->Abort(error.get(), nullptr);
      return;
    }

    base::DictionaryValue result;
    result.SetString("reply", "Hello " + name);
    cmd->Complete(result, nullptr);
    LOG(INFO) << cmd->GetName() << " command finished: " << result;
  }

  void OnToggleCommand(const std::weak_ptr<weave::Command>& command) {
    auto cmd = command.lock();
    if (!cmd)
      return;
    LOG(INFO) << "received command: " << cmd->GetName();

    /*************************************
     *                                   *
     *   TRY WRITING TO GPIO FILESTREAM  *
     *                                   *
     *************************************/
    char *state;
    fgets(state, 2, fp);
    fseek(fp,0,SEEK_SET);
    char *newstate;
    if(state == "1"){
        newstate = "0";
    }else{
        newstate = "1";
    }
    fputs(newstate,fp);
    fflush(fp);
    fseek(fp,0,SEEK_SET);
    //************************************

    device_->SetStateProperty(kComponent, "_weave_led.led_status",
                              newstate, nullptr);

    LOG(INFO) << "New component state: " << device_->GetComponents();

    cmd->Complete({}, nullptr);

    LOG(INFO) << cmd->GetName() << " command finished";
  }

  void OnBlinkCommand(const std::weak_ptr<weave::Command>& command) {
    auto cmd = command.lock();
    if (!cmd)
      return;
    LOG(INFO) << "received command: " << cmd->GetName();

    const auto& params = cmd->GetParameters();
    int seconds;
    if (!params.GetInteger("seconds", &seconds))
      seconds = 10;

    LOG(INFO) << "starting countdown";
    DoTick(cmd, seconds);
  }

  void DoTick(const std::weak_ptr<weave::Command>& command, int seconds) {
    auto cmd = command.lock();
    if (!cmd)
      return;

    if (seconds > 0) {
      LOG(INFO) << "countdown tick: " << seconds << " seconds left";
      
      //***********************
      // TRY FLASHING THE LED
      //***********************
      fputs("0",fd);
      fflush(fd);
      fseek(fp,0,SEEK_SET);
      sleep(0.5);
      fputs("1",fd);
      fflush(fd);
      fseek(fp,0,SEEK_SET);

      base::DictionaryValue progress;
      progress.SetInteger("seconds_left", seconds);
      cmd->SetProgress(progress, nullptr);
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&SampleHandler::DoTick, weak_ptr_factory_.GetWeakPtr(),
                     command, --seconds),
          base::TimeDelta::FromSeconds(1));
      return;
    }

    cmd->Complete({}, nullptr);
    LOG(INFO) << "countdown finished";
    LOG(INFO) << cmd->GetName() << " command finished";
  }

  weave::Device* device_{nullptr};
  weave::provider::TaskRunner* task_runner_{nullptr};

  base::WeakPtrFactory<SampleHandler> weak_ptr_factory_{this};

  FILE *fp; // for gpio pin
};

int main(int argc, char** argv) {
  Daemon::Options opts;
  if (!opts.Parse(argc, argv)) {
    Daemon::Options::ShowUsage(argv[0]);
    return 1;
  }
  Daemon daemon{opts};
  SampleHandler handler{daemon.GetTaskRunner()};
  handler.Register(daemon.GetDevice());
  daemon.Run();
  return 0;
}
