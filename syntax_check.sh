#!/bin/sh
# Syntax/consistency check of the driver sources against stub headers (NOT a WDK build).
cd "$(dirname "$0")"
for f in ../drivers/emmc/emmc.c ../drivers/qcompwr/qcom_pwr.c ../drivers/qcompwr/qcom_res.c ../drivers/emmc/emmc_core.c; do
  echo "== $f"; gcc -std=gnu11 -fsyntax-only -Wall -Wno-unknown-pragmas -I stubs -I ../drivers/qcompwr "$f" 2>&1 | head -40
done
