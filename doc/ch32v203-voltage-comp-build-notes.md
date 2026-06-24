# CH32V203 Voltage Compensation Build Notes

Date: 2026-06-25

## Source

- Repository: `C:\Users\santi\PowerHouse\Orbt\AM32-Orbt-Fork`
- Branch: `feature/voltage-compensation`
- Commit: `2727008dabf69ad222c4f72f769066b6dc019692`
- Build target: `ORBT_ESC_V203`
- Normal feature macros enabled in `Inc/targets.h`:
  - `USE_VREF_CALIBRATION`
  - `USE_VOLTAGE_COMPENSATION`
  - `USE_DEADTIME_COMPENSATION`

Only MounRiver project metadata was dirty during this build; firmware source files matched the commit above.

## Toolchain

- Compiler: `C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC12\bin`
- Make: `C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin\make.exe`
- Build directory: `C:\Users\santi\PowerHouse\Orbt\AM32-Orbt-Fork\MRS_Projects\ch32v203f8u6\Am32V203\obj`

## Build Command

```powershell
$tool='C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC12\bin'
$makeDir='C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin'
$env:Path="$tool;$makeDir;$env:Path"
& "$makeDir\make.exe" clean all
```

Build result:

- `FLASH`: `19788 B / 57136 B`
- `RAM`: `4436 B / 20 KB`
- ELF size: `text=18848`, `data=976`, `bss=3460`

The rebuilt ELF was checked for the normal Vref-enabled path. It contains `ADC_raw_vref`, `adc_vdd_mv`, and ADC rank 4 configured as channel 17 (`ADC_Channel_Vrefint`).

## Inputs For Combined HEX

- Bootloader: `C:\Users\santi\PowerHouse\Orbt\PreCompiled_Boot_and_Software_V203\AM32_V203_BOOTLOADER_PA0_V14.hex`
  - SHA256: `BFD87058AD366CCBF13222DFD48860B320DE4C093F0DF097C8043D5EE3D5F49B`
- Config: `C:\Users\santi\PowerHouse\Orbt\PreCompiled_Boot_and_Software_V203\Orbt_ESC_4500kv_Mk5_motor1_config_ZA.ecf`
  - SHA256: `7C0F883384E61AE0938EB31D835518DEB9360660A0DB84CC21A27A45DB5ABB92`
- M2 config: `C:\Users\santi\PowerHouse\Orbt\PreCompiled_Boot_and_Software_V203\Orbt_ESC_4500kv_Mk5_motor2_config_ZA.ecf`
  - SHA256: `F73D310DACC1607E83C81F59B3E3243D7D4A067B5A4F90B8E0D5F36ED30255BC`
- App build output: `MRS_Projects\ch32v203f8u6\Am32V203\obj\Am32V203.hex`

The config files are 48 bytes and do not include tune bytes. Each combined HEX explicitly writes `0x39` at `0xF830` and pads through `0xF83F` with `0xFF` so the firmware takes the default startup beep path.

## Outputs

Normal app-only HEX:

- `C:\Users\santi\PowerHouse\Orbt\PreCompiled_Boot_and_Software_V203\Am32V203_Orbt_ESC__2026_6_25__NORMAL_voltage_comp_2727008.hex`
- SHA256: `12E184A361D277DACC2D6B9F23FC39A7855761C5141173F6B85C94C77DAA8AA1`
- Ranges: `0x1000-0x1003`, `0x10D0-0x5E1B`, `0xF7E0-0xF7FF`

Clean v1.3.0 app-only HEX name:

- `C:\Users\santi\PowerHouse\Orbt\PreCompiled_Boot_and_Software_V203\Am32V203_Orbt_ESC__2026_6_24__1.3.0.hex`
- SHA256: `12E184A361D277DACC2D6B9F23FC39A7855761C5141173F6B85C94C77DAA8AA1`
- Ranges: `0x1000-0x1003`, `0x10D0-0x5E1B`, `0xF7E0-0xF7FF`
- Does not include bootloader or config EEPROM records.

Combined bootloader + app + config HEX:

- `C:\Users\santi\PowerHouse\Orbt\PreCompiled_Boot_and_Software_V203\Am32V203_Orbt_ESC_4500kv_Mk5_COMBINED_NORMAL_voltage_comp_2727008_boot14+config_ZA_PADDED16_default_beeps.hex`
- SHA256: `1DD033FABEA5D1FE9AEF6D9CBEE3F393051AC216A8491BCC3901AE6B03BE43C4`
- Ranges: `0x0000-0x0FC7`, `0x1000-0x1003`, `0x10D0-0x5E1B`, `0xF7E0-0xF83F`
- Intel HEX checksums: OK
- Record structure: no type-04 extended linear address records, one type-03 start-address record, one EOF record

The app-only HEX is byte-for-byte identical to the earlier normal Vref-enabled build named `Am32V203_Orbt_ESC__2026_6_25__voltage_comp_2727008.hex`.

## Requested Test Matrix Outputs

The repo-local review package is under the top-level release area:

- `releases/`

These are the four combined files prepared for M1/M2 testing:

- `releases/Am32V203_Orbt_ESC__2026_4_8__1.2.0_With_Bootloader_M1.hex`
  - App: `Am32V203_Orbt_ESC__2026_4_8__1.2.0.hex`
  - Config: `Orbt_ESC_4500kv_Mk5_motor1_config_ZA.ecf`
  - SHA256: `03E6BEF3CCF93E348444DCD4337FE91DABE907B3645DEA77915EEF89E8ACF444`
  - Ranges: `0x0000-0x0FC7`, `0x1000-0x1003`, `0x10D0-0x5D77`, `0xF7E0-0xF83F`

- `releases/Am32V203_Orbt_ESC__2026_4_8__1.2.0_With_Bootloader_M2.hex`
  - App: `Am32V203_Orbt_ESC__2026_4_8__1.2.0.hex`
  - Config: `Orbt_ESC_4500kv_Mk5_motor2_config_ZA.ecf`
  - SHA256: `FD37E09D9318469B177C5F3F4141E637C407F6B0A217940813F96318F6E57D04`
  - Ranges: `0x0000-0x0FC7`, `0x1000-0x1003`, `0x10D0-0x5D77`, `0xF7E0-0xF83F`

- `releases/Am32V203_Orbt_ESC__2026_6_24__1.3.0_With_Bootloader_M1.hex`
  - App: `Am32V203_Orbt_ESC__2026_6_25__NORMAL_voltage_comp_2727008.hex`
  - Config: `Orbt_ESC_4500kv_Mk5_motor1_config_ZA.ecf`
  - SHA256: `1DD033FABEA5D1FE9AEF6D9CBEE3F393051AC216A8491BCC3901AE6B03BE43C4`
  - Ranges: `0x0000-0x0FC7`, `0x1000-0x1003`, `0x10D0-0x5E1B`, `0xF7E0-0xF83F`

- `releases/Am32V203_Orbt_ESC__2026_6_24__1.3.0_With_Bootloader_M2.hex`
  - App: `Am32V203_Orbt_ESC__2026_6_25__NORMAL_voltage_comp_2727008.hex`
  - Config: `Orbt_ESC_4500kv_Mk5_motor2_config_ZA.ecf`
  - SHA256: `C4AFCEAFA5A645F886FEFAB5993507D67978EDB95E13DBA249D391F87548ACF1`
  - Ranges: `0x0000-0x0FC7`, `0x1000-0x1003`, `0x10D0-0x5E1B`, `0xF7E0-0xF83F`

All four files have valid Intel HEX checksums, no type-04 extended linear address records, one type-03 start-address record, one EOF record, and `0xF830 = 0x39`.

## Flashing

For the combined HEX in MounRiver/WCH download settings:

- MCU type: `CH32V20x`
- Program Address: `0x08000000`
- Main operations: `Erase All`, `Program`, `Verify`, `Reset and Run`
- Target file: select the combined HEX listed above, not the bootloader-only HEX.

Warning: the saved MounRiver project metadata currently has `flashConfig.target_path` pointing at `AM32_V203_BOOTLOADER_PA0_V14.hex`. With `Erase All`, flashing that file alone erases the app/config and leaves only the bootloader.
