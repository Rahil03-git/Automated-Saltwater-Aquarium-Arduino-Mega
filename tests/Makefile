# Host-side firmware tests (needs g++). Run:  make -C tests test
FW      := ../firmware/AutomatedSaltwaterAquarium
CXX     ?= g++
CXXFLAGS := -std=gnu++11 -Wall -Wextra -O1 -I mocks -I $(FW)
SRC     := mocks/mock_arduino.cpp $(FW)/aqclock.cpp $(FW)/actuators.cpp $(FW)/buttons.cpp \
           $(FW)/calibration.cpp $(FW)/cli.cpp $(FW)/display.cpp $(FW)/sensors.cpp test_main.cpp
DEPS    := $(wildcard $(FW)/*.h) $(wildcard $(FW)/*.ino) $(wildcard mocks/*.h)

.PHONY: test clean
test: build/runner_report build/runner_lockout build/runner_marine build/runner_optional
	@set -e; for t in logic normal alarms feeding lighting pump pumpfault calibration ui cli; do build/runner_report $$t; done
	@build/runner_lockout lockout
	@set -e; for t in logic marine pump; do build/runner_marine $$t; done
	@set -e; for t in logic normal pump optional; do build/runner_optional $$t; done
	@echo "ALL HOST TESTS PASSED"

build/runner_report: $(SRC) $(DEPS)
	@mkdir -p build && $(CXX) $(CXXFLAGS) $(SRC) -o $@
build/runner_lockout: $(SRC) $(DEPS)
	@mkdir -p build && $(CXX) $(CXXFLAGS) -DBOOT_FEED_LOCKOUT_MS=300000UL $(SRC) -o $@
build/runner_optional: $(SRC) $(DEPS)
	@mkdir -p build && $(CXX) $(CXXFLAGS) -DENABLE_HEATER_RELAY=1 -DENABLE_PH_DOSING=1 $(SRC) -o $@
build/runner_marine: $(SRC) $(DEPS)
	@mkdir -p build && $(CXX) $(CXXFLAGS) -DACTIVE_PROFILE=2 $(SRC) -o $@

clean:
	rm -rf build
