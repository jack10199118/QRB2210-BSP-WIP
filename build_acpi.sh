#!/bin/sh
# Compile the ACPI sources with iasl (install acpica-tools). Run from the project root.
set -e
mkdir -p build/acpi
iasl -p build/acpi/dsdt acpi/dsdt.asl
for t in madt gtdt dbg2 iort; do
  iasl -p build/acpi/$t acpi/tables/$t.asl      # data tables are detected automatically
done
echo "Compiled tables in build/acpi. Fix every error and warning before loading on hardware."
