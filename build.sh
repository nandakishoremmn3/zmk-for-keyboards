#!/usr/bin/env bash
set -e # Stop execution if any command fails

WORKDIR="/home/loki/code/zmk-for-keyboards"
IMAGE="zmkfirmware/zmk-build-arm:stable"
WINDOWS_DESKTOP="/mnt/c/Users/nanda/Desktop"

clear
echo "===================================================="
echo "🚀       STARTING CORNE ZMK BUILD PROCESS        🚀"
echo "===================================================="
echo "📂 Workdir: ${WORKDIR}"
echo "🐳 Image:   ${IMAGE}"
echo "===================================================="

# ----------------------------------------------------------------
# 1. BUILD LEFT SIDE (MAIN / USB COUPLING)
# ----------------------------------------------------------------
echo ""
echo "🔮 [1/2] Preparing Left Side..."
echo "----------------------------------------------------"

docker run --rm \
  -v "${WORKDIR}":/zmk-for-keyboards \
  --workdir /zmk-for-keyboards \
  "${IMAGE}" \
  /bin/bash -c "
    rm -rf .west zmk/.west /tmp/zmk_build && \
    west init -l zmk/app && \
    cd zmk && \
    west build -p always -s app -d /tmp/zmk_build -b nice_nano//zmk -S studio-rpc-usb-uart -- \
      -DZMK_CONFIG=/zmk-for-keyboards/config \
      -DSHIELD=corne_left && \
    cp /tmp/zmk_build/zephyr/zmk.uf2 /zmk-for-keyboards/corne_left.uf2
  "

echo "🎨 Left Side Build Complete!"

# ----------------------------------------------------------------
# 2. BUILD RIGHT SIDE (PERIPHERAL / WPM DISABLED)
# ----------------------------------------------------------------
echo ""
echo "🔮 [2/2] Preparing Right Side..."
echo "----------------------------------------------------"

docker run --rm \
  -v "${WORKDIR}":/zmk-for-keyboards \
  --workdir /zmk-for-keyboards \
  "${IMAGE}" \
  /bin/bash -c "
    rm -rf .west zmk/.west /tmp/zmk_build && \
    west init -l zmk/app && \
    cd zmk && \
    west build -p always -s app -d /tmp/zmk_build -b nice_nano//zmk -S studio-rpc-usb-uart -- \
      -DZMK_CONFIG=/zmk-for-keyboards/config \
      -DSHIELD=corne_right \
      -DCONFIG_ZMK_WPM=n && \
    cp /tmp/zmk_build/zephyr/zmk.uf2 /zmk-for-keyboards/corne_right.uf2
  "

echo "🎨 Right Side Build Complete!"

# ----------------------------------------------------------------
# 3. EXPORT TO WINDOWS DESKTOP
# ----------------------------------------------------------------
echo ""
echo "🚚 Exporting firmware to Windows Desktop..."
echo "----------------------------------------------------"

# Clear the clipboard first — a prior run's "copy corne_left.uf2 to clipboard" step
# (below) leaves an open handle on that file that blocks overwriting it from WSL.
powershell.exe -NoProfile -Command "Set-Clipboard -Value \$null" >/dev/null 2>&1 || true

# Create the folder if it doesn't exist, then copy the files over
mkdir -p "${WINDOWS_DESKTOP}"
cp "${WORKDIR}"/corne_left.uf2 "${WINDOWS_DESKTOP}/corne_left.uf2"
cp "${WORKDIR}"/corne_right.uf2 "${WINDOWS_DESKTOP}/corne_right.uf2"

# ----------------------------------------------------------------
# 4. RE-RENDER KEYMAP SVG
# ----------------------------------------------------------------
echo ""
echo "🖼️  Re-rendering keymap SVG..."
echo "----------------------------------------------------"

export PATH="$PATH:${HOME}/.local/bin"
keymap parse -z "${WORKDIR}/config/corne.keymap" > /tmp/corne_keymap.yaml
keymap draw /tmp/corne_keymap.yaml > "${WORKDIR}/corne_keymap.svg"

# ----------------------------------------------------------------
# 5. COPY LEFT HALF FIRMWARE TO WINDOWS CLIPBOARD
# ----------------------------------------------------------------
echo ""
echo "📋 Copying corne_left.uf2 to clipboard..."
echo "----------------------------------------------------"

LEFT_WIN_PATH="$(wslpath -w "${WINDOWS_DESKTOP}/corne_left.uf2")"
powershell.exe -NoProfile -Command "Set-Clipboard -Path '${LEFT_WIN_PATH}'"

# ----------------------------------------------------------------
# SUMMARY OUTPUT
# ----------------------------------------------------------------
echo ""
echo "===================================================="
echo "🎉 SUCCESS! Your files are ready on your Desktop:  "
echo "===================================================="
ls -lh "${WINDOWS_DESKTOP}"/corne_*.uf2
echo "===================================================="
echo "👉 Drag and drop from your Desktop to flash your nice!nanos!"
echo "===================================================="
