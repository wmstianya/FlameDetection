# Host unit tests for the pure MCU2_G070 modules (native gcc + Unity).
#
#   make test     # build and run all suites
#   make clean

CC     ?= gcc
CFLAGS := -std=c99 -Wall -g -O0

HERE  := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
M2    := $(abspath $(HERE)/..)
UNITY := $(HERE)/unity/unity.c
BUILD := $(HERE)/build

INC := -I$(HERE)/unity -I$(M2)/config -I$(M2)/protocol -I$(M2)/utils -I$(M2)/ads1220

BINS := $(BUILD)/testHostProtocol $(BUILD)/testTempCalib $(BUILD)/testAds1220Reading

.PHONY: test clean
test: $(BINS)
	@echo "== hostProtocol ==" && $(BUILD)/testHostProtocol
	@echo "== tempCalib =="     && $(BUILD)/testTempCalib
	@echo "== ads1220Reading ==" && $(BUILD)/testAds1220Reading
	@echo "ALL TEST SUITES PASSED"

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/testHostProtocol: $(HERE)/testHostProtocol.c $(M2)/protocol/hostProtocol.c $(UNITY) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $^ -o $@

$(BUILD)/testTempCalib: $(HERE)/testTempCalib.c $(M2)/utils/tempCalib.c $(UNITY) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $^ -o $@

$(BUILD)/testAds1220Reading: $(HERE)/testAds1220Reading.c $(M2)/ads1220/ads1220Reading.c $(UNITY) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $^ -o $@

clean:
	rm -rf $(BUILD)
