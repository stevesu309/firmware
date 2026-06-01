#!/usr/bin/env python3
# trunk-ignore-all(ruff/F821)
# trunk-ignore-all(flake8/F821): For SConstruct imports
import sys
import os
from os.path import join
import subprocess
import json
import re

from readprops import readProps

Import("env")
platform = env.PioPlatform()


def esp32_create_combined_bin(source, target, env):
    # this sub is borrowed from ESPEasy build toolchain. It's licensed under GPL V3
    # https://github.com/letscontrolit/ESPEasy/blob/mega/tools/pio/post_esp32.py
    print("Generating combined binary for serial flashing")

    app_offset = 0x10000

    new_file_name = env.subst("$BUILD_DIR/${PROGNAME}.factory.bin")
    sections = env.subst(env.get("FLASH_EXTRA_IMAGES"))
    firmware_name = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    chip = env.get("BOARD_MCU")
    flash_size = env.BoardConfig().get("upload.flash_size")
    flash_freq = env.BoardConfig().get("build.f_flash", "40m")
    flash_freq = flash_freq.replace("000000L", "m")
    flash_mode = env.BoardConfig().get("build.flash_mode", "dio")
    memory_type = env.BoardConfig().get("build.arduino.memory_type", "qio_qspi")
    if flash_mode == "qio" or flash_mode == "qout":
        flash_mode = "dio"
    if memory_type == "opi_opi" or memory_type == "opi_qspi":
        flash_mode = "dout"
    cmd = [
        "--chip",
        chip,
        "merge_bin",
        "-o",
        new_file_name,
        "--flash_mode",
        flash_mode,
        "--flash_freq",
        flash_freq,
        "--flash_size",
        flash_size,
    ]

    print("    Offset | File")
    for section in sections:
        sect_adr, sect_file = section.split(" ", 1)
        print(f" -  {sect_adr} | {sect_file}")
        cmd += [sect_adr, sect_file]

    print(f" - {hex(app_offset)} | {firmware_name}")
    cmd += [hex(app_offset), firmware_name]

    print("Using esptool.py arguments: %s" % " ".join(cmd))

    esptool.main(cmd)


if platform.name == "espressif32":
    sys.path.append(join(platform.get_package_dir("tool-esptoolpy")))
    import esptool

    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_create_combined_bin)

    esp32_kind = env.GetProjectOption("custom_esp32_kind")
    if esp32_kind == "esp32":
        # Free up some IRAM by removing auxiliary SPI flash chip drivers.
        # Wrapped stub symbols are defined in src/platform/esp32/iram-quirk.c.
        env.Append(
            LINKFLAGS=[
                "-Wl,--wrap=esp_flash_chip_gd",
                "-Wl,--wrap=esp_flash_chip_issi",
                "-Wl,--wrap=esp_flash_chip_winbond",
            ]
        )
    else:
        # For newer ESP32 targets, using newlib nano works better.
        env.Append(LINKFLAGS=["--specs=nano.specs", "-u", "_printf_float"])

if platform.name == "nordicnrf52":
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex",
                      env.VerboseAction(f"\"{sys.executable}\" ./bin/uf2conv.py $BUILD_DIR/firmware.hex -c -f 0xADA52840 -o $BUILD_DIR/firmware.uf2",
                                        "Generating UF2 file"))


def has_cpp_define(env_obj, define_name, expected_value=None):
    for define in env_obj.get("CPPDEFINES", []):
        if isinstance(define, tuple):
            name, value = define
            if name == define_name:
                if expected_value is None:
                    return True
                return str(value) == str(expected_value)
        elif define == define_name and expected_value is None:
            return True
    return False


def get_project_option_safe(option_name, default=None):
    try:
        return env.GetProjectOption(option_name)
    except Exception:
        return default


def resolve_serial_port():
    candidates = [
        get_project_option_safe("monitor_port"),
        get_project_option_safe("upload_port"),
        env.get("MONITOR_PORT"),
        env.get("UPLOAD_PORT"),
    ]

    for candidate in candidates:
        if candidate and "$" not in str(candidate):
            return str(candidate)

    upload_port = env.subst("$UPLOAD_PORT")
    if upload_port and "$" not in upload_port:
        return upload_port

    return None


def normalize_serial_port(port):
    if not port:
        return port
    if port.startswith("/dev/cu."):
        tty_port = "/dev/tty." + port[len("/dev/cu.") :]
        if os.path.exists(tty_port):
            return tty_port
    return port


def should_auto_upload_chinese_font():
    val = env.GetProjectOption("custom_auto_upload_chinese_font", None)
    if val is None:
        return False

    if platform.name != "nordicnrf52":
        return False

    if not has_cpp_define(env, "CNFONT_EMBED_INTERNAL_TABLE", 0):
        return False

    return get_project_option_safe("custom_upload_external_chinese_font", "true").lower() == "true"


def append_optional_arg(cmd, option_name, option_value):
    if option_value is None:
        return
    option_value = str(option_value).strip()
    if option_value == "":
        return
    cmd.extend([option_name, option_value])


def auto_upload_chinese_font(source, target, env):
    if not should_auto_upload_chinese_font():
        return

    port = resolve_serial_port()
    if not port:
        print("Skipping external Chinese font upload: no serial port configured")
        return
    port = normalize_serial_port(port)

    project_dir = env["PROJECT_DIR"]
    python_exe = sys.executable
    font_source = str(get_project_option_safe("custom_external_font_source", "src/graphics/fonts/ChineseFontData.cpp"))
    font_output = str(get_project_option_safe("custom_external_font_output", "bin/chinese_font.bin"))
    font_target = str(get_project_option_safe("custom_external_font_target", "qspi://chinese_font.bin"))
    font_type_name = str(get_project_option_safe("custom_external_font_type_name", "ChineseFont"))
    font_array_name = str(get_project_option_safe("custom_external_font_array_name", "chineseFont"))
    key_size = get_project_option_safe("custom_external_font_key_size", "4")
    glyph_width = get_project_option_safe("custom_external_font_glyph_width")
    glyph_height = get_project_option_safe("custom_external_font_glyph_height")
    bitmap_size = get_project_option_safe("custom_external_font_bitmap_size")
    font_magic = get_project_option_safe("custom_external_font_magic", "0x43484631")
    font_version = get_project_option_safe("custom_external_font_version", "1")
    font_max_bytes = get_project_option_safe("custom_external_font_max_bytes", "0x00080000")

    font_bin = join(project_dir, font_output)
    generate_script = join(project_dir, "bin", "generate_chinese_font_bin.py")
    upload_script = join(project_dir, "bin", "upload_chinese_font_to_device.py")
    baud = str(get_project_option_safe("monitor_speed", env.get("MONITOR_SPEED", 115200)))

    print("Generating external Chinese font image")
    generate_cmd = [
        python_exe,
        generate_script,
        "--input",
        font_source,
        "--output",
        font_bin,
        "--font-type-name",
        font_type_name,
        "--font-array-name",
        font_array_name,
        "--key-size",
        str(key_size),
        "--magic",
        str(font_magic),
        "--version",
        str(font_version),
    ]
    append_optional_arg(generate_cmd, "--glyph-width", glyph_width)
    append_optional_arg(generate_cmd, "--glyph-height", glyph_height)
    append_optional_arg(generate_cmd, "--bitmap-size", bitmap_size)
    subprocess.check_call(generate_cmd, cwd=project_dir)

    print(f"Uploading external Chinese font image via {port}")
    upload_cmd = [
        python_exe,
        upload_script,
        "--port",
        port,
        "--baud",
        baud,
        "--input",
        font_bin,
        "--target-name",
        font_target,
        "--key-size",
        str(key_size),
        "--magic",
        str(font_magic),
        "--version",
        str(font_version),
        "--max-bytes",
        str(font_max_bytes),
        "--wait-seconds",
        "20",
        "--boot-wait",
        "6",
    ]
    append_optional_arg(upload_cmd, "--bitmap-size", bitmap_size)
    subprocess.check_call(upload_cmd, cwd=project_dir)


if should_auto_upload_chinese_font():
    env.AddPostAction("upload", auto_upload_chinese_font)

Import("projenv")

prefsLoc = projenv["PROJECT_DIR"] + "/version.properties"
verObj = readProps(prefsLoc)
print("Using meshtastic platformio-custom.py, firmware version " + verObj["long"] + " on " + env.get("PIOENV"))

# get repository owner if git is installed
try:
    r_owner = (
        subprocess.check_output(["git", "config", "--get", "remote.origin.url"])
        .decode("utf-8")
        .strip().split("/")
    )
    repo_owner = r_owner[-2] + "/" + r_owner[-1].replace(".git", "")
except subprocess.CalledProcessError:
    repo_owner = "unknown"

jsonLoc = env["PROJECT_DIR"] + "/userPrefs.jsonc"
with open(jsonLoc) as f:
    jsonStr = re.sub("//.*","", f.read(), flags=re.MULTILINE)
    userPrefs = json.loads(jsonStr)

pref_flags = []
# Pre-process the userPrefs
for pref in userPrefs:
    if userPrefs[pref].startswith("{"):
        pref_flags.append("-D" + pref + "=" + userPrefs[pref])
    elif userPrefs[pref].lstrip("-").replace(".", "").isdigit():
        pref_flags.append("-D" + pref + "=" + userPrefs[pref])
    elif userPrefs[pref] == "true" or userPrefs[pref] == "false":
        pref_flags.append("-D" + pref + "=" + userPrefs[pref])
    elif userPrefs[pref].startswith("meshtastic_"):
        pref_flags.append("-D" + pref + "=" + userPrefs[pref])
    # If the value is a string, we need to wrap it in quotes
    else:
        pref_flags.append("-D" + pref + "=" + env.StringifyMacro(userPrefs[pref]) + "")

# General options that are passed to the C and C++ compilers
flags = [
        "-DAPP_VERSION=" + verObj["long"],
        "-DAPP_VERSION_SHORT=" + verObj["short"],
        "-DAPP_ENV=" + env.get("PIOENV"),
        "-DAPP_REPO=" + repo_owner,
    ] + pref_flags

print ("Using flags:")
for flag in flags:
    print(flag)
    
projenv.Append(
    CCFLAGS=flags,
)

for lb in env.GetLibBuilders():
    if lb.name == "meshtastic-device-ui":
        lb.env.Append(CPPDEFINES=[("APP_VERSION", verObj["long"])])
        break

# Get the display resolution from macros
def get_display_resolution(build_flags):
    # Check "DISPLAY_SIZE" to determine the screen resolution
    for flag in build_flags:
        if isinstance(flag, tuple) and flag[0] == "DISPLAY_SIZE":
            screen_width, screen_height = map(int, flag[1].split("x"))
            return screen_width, screen_height
    print("No screen resolution defined in build_flags. Please define DISPLAY_SIZE.")
    exit(1)

def load_boot_logo(source, target, env):
    build_flags = env.get("CPPDEFINES", [])
    logo_w, logo_h = get_display_resolution(build_flags)
    print(f"TFT build with {logo_w}x{logo_h} resolution detected")

    # Load the boot logo from `branding/logo_<width>x<height>.png` if it exists
    source_path = join(env["PROJECT_DIR"], "branding", f"logo_{logo_w}x{logo_h}.png")
    dest_dir = join(env["PROJECT_DIR"], "data", "boot")
    dest_path = join(dest_dir, "logo.png")
    if env.File(source_path).exists():
        print(f"Loading boot logo from {source_path}")
        # Prepare the destination
        env.Execute(f"mkdir -p {dest_dir} && rm -f {dest_path}")
        # Copy the logo to the `data/boot` directory
        env.Execute(f"cp {source_path} {dest_path}")

# Load the boot logo on TFT builds
if ("HAS_TFT", 1) in env.get("CPPDEFINES", []):
    env.AddPreAction('$BUILD_DIR/littlefs.bin', load_boot_logo)
