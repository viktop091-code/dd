#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
release_root="$project_root/dist/@OpenConvoy"

mkdir -p "$release_root/Addons"
python3 "$project_root/tools/pack_pbo.py" \
  "$project_root/OpenConvoy" \
  "$release_root/Addons/OpenConvoy.pbo" \
  --prefix OpenConvoy

cp "$project_root/mod.cpp" "$release_root/mod.cpp"
cp "$project_root/README.md" "$release_root/README.md"

(
  cd "$project_root/dist"
  sha256sum '@OpenConvoy/Addons/OpenConvoy.pbo' > SHA256SUMS.txt
  rm -f OpenConvoy-0.1.1.zip
  zip -qr OpenConvoy-0.1.1.zip '@OpenConvoy' SHA256SUMS.txt
)

echo "Release created under $project_root/dist"
