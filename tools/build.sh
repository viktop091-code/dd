#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
release_root="$project_root/dist/@Wild-Fields"

mkdir -p "$release_root/Addons"
python3 "$project_root/tools/pack_pbo.py" \
  "$project_root/WildFields" \
  "$release_root/Addons/WildFields.pbo" \
  --prefix WildFields

cp "$project_root/mod.cpp" "$release_root/mod.cpp"
cp "$project_root/README.md" "$release_root/README.md"
cp "$project_root/THIRD_PARTY_NOTICES.md" "$release_root/THIRD_PARTY_NOTICES.md"

(
  cd "$project_root/dist"
  sha256sum '@Wild-Fields/Addons/WildFields.pbo' > SHA256SUMS.txt
  rm -f Wild-Fields-1.0.0.zip
  zip -qr Wild-Fields-1.0.0.zip '@Wild-Fields' SHA256SUMS.txt
)

echo "Release created under $project_root/dist"
